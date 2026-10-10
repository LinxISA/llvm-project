//===-- LinxV5TileCarrierTransport.cpp - CUBE Tile carrier ABI transport --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Issue #120: a pto::Tile<..., CubeM16/M32/N8, ...> object that crosses a
// function boundary (reference/by-value parameter, address-escaped alloca)
// must keep its CUBE layout. Its carrier stores/loads are then Local<->GM
// transports (M322ND/M162ND/N82ND out, ND2M32/ND2M16/ND2N8 in), never raw
// S64/S32 NORM payloads: a NORM reload feeding a CUBE consumer violates the
// "CUBE elementwise operands must share one M carrier layout" contract.
//
// Clang attaches a "linx.tile.transport" metadata node (an MDString
// "linx.tile.carrier:v1;..." contract) to every CUBE Tile carrier
// load/store. Promotable accesses vanish under SROA together with their
// metadata, so this pass only ever sees genuine boundary accesses; it
// rewrites them into llvm.linx.blk.tload / blk.tstore transport calls.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-tile-carrier-transport"

namespace {

// pto BLayout template values (common/layout.hpp).
constexpr unsigned BLayout_CubeM16 = 2;
constexpr unsigned BLayout_CubeM32 = 3;
constexpr unsigned BLayout_CubeN8 = 4;

// LinxV5Op::ArgFormat GM<->Local CUBE transport codes (LinxV5TileTrans.def):
// 21 ND2M32 / 22 ND2M16 / 23 ND2N8 (GM -> Local, TLOAD-only),
// 24 M322ND / 25 M162ND / 26 N82ND (Local -> GM, TSTORE-only).
constexpr unsigned LoadTransportForLayout(unsigned BLayout) {
  switch (BLayout) {
  case BLayout_CubeM16:
    return 22; // ND2M16
  case BLayout_CubeM32:
    return 21; // ND2M32
  case BLayout_CubeN8:
    return 23; // ND2N8
  }
  return 0;
}

constexpr unsigned StoreTransportForLayout(unsigned BLayout) {
  switch (BLayout) {
  case BLayout_CubeM16:
    return 25; // M162ND
  case BLayout_CubeM32:
    return 24; // M322ND
  case BLayout_CubeN8:
    return 26; // N82ND
  }
  return 0;
}

// Element width in bytes for the DataType codes this contract can carry.
constexpr unsigned dataTypeElementBytes(unsigned DataType) {
  switch (DataType) {
  case 0:  // FP64
  case 16: // S64
  case 24: // U64
    return 8;
  case 1:  // FP32
  case 2:  // TF32
  case 3:  // HF32
  case 17: // S32
  case 25: // U32
    return 4;
  case 4:  // FP16
  case 5:  // BF16
  case 18: // S16
  case 26: // U16
    return 2;
  case 6:  // HiF8
  case 7:  // e4m3
  case 8:  // e5m2
  case 19: // S8
  case 27: // U8
    return 1;
  }
  return 0;
}

struct CarrierContract {
  unsigned Layout = 0;   // BLayout value
  unsigned DataType = 0; // LinxV5Op::DataType code
  int Rows = 0;          // physical rows
  int Cols = 0;          // physical cols
  int ValidRow = 0;      // -1 = dynamic (read the object field)
  int ValidCol = 0;      // -1 = dynamic
  bool isValid() const {
    return (Layout == BLayout_CubeM16 || Layout == BLayout_CubeM32 ||
            Layout == BLayout_CubeN8) &&
           Rows > 0 && Cols > 0 && dataTypeElementBytes(DataType) > 0;
  }
};

static bool parseCarrierContract(StringRef S, CarrierContract &C) {
  if (!S.consume_front("linx.tile.carrier:v1;"))
    return false;
  while (!S.empty()) {
    StringRef Key, Value, Rest;
    std::tie(Key, Rest) = S.split('=');
    if (Key.empty())
      return false;
    std::tie(Value, S) = Rest.split(';');
    int V = 0;
    if (Value.getAsInteger(10, V))
      return false;
    if (Key == "layout")
      C.Layout = V;
    else if (Key == "dtype")
      C.DataType = V;
    else if (Key == "rows")
      C.Rows = V;
    else if (Key == "cols")
      C.Cols = V;
    else if (Key == "vrow")
      C.ValidRow = V;
    else if (Key == "vcol")
      C.ValidCol = V;
    else
      return false;
  }
  return C.isValid();
}

// Recover the Tile object base pointer from a carrier pointer. Supported
// shapes: the constant carrier-field GEP (..., 0, 3) of the object, or a
// not-yet-inlined pto::Tile<...>::data() call result (the callee takes the
// object pointer as its only parameter).
static Value *getTileObject(Value *P) {
  P = P->stripPointerCasts();
  if (auto *GEP = dyn_cast<GetElementPtrInst>(P)) {
    if (!GEP->hasAllConstantIndices())
      return nullptr;
    SmallVector<Value *, 4> Ops(GEP->idx_begin(), GEP->idx_end());
    if (Ops.size() != 2)
      return nullptr;
    auto *FieldIdx = dyn_cast<ConstantInt>(Ops[1]);
    if (!FieldIdx || FieldIdx->getZExtValue() != 3)
      return nullptr; // not the carrier member
    return GEP->getPointerOperand();
  }
  if (auto *CI = dyn_cast<CallInst>(P)) {
    Function *Callee = CI->getCalledFunction();
    if (Callee && Callee->arg_size() == 1 && CI->arg_size() == 1 &&
        Callee->getName().endswith("4dataEv"))
      return CI->getArgOperand(0)->stripPointerCasts();
  }
  return nullptr;
}

class TileCarrierTransport {
  Function &F;

  // RowMaskInternal / ColMaskInternal are the first two int fields of the
  // Tile object (byte offsets 0 and 4); they hold the runtime valid shape
  // for dynamic (-1) masks.
  Value *loadDimField(IRBuilder<> &B, Value *Obj, unsigned ByteOffset) {
    Value *P = B.CreateGEP(Type::getInt8Ty(F.getContext()), Obj,
                           B.getInt64(ByteOffset), "linx.tile.mask.gep");
    Value *V = B.CreateLoad(Type::getInt32Ty(F.getContext()), P,
                            "linx.tile.mask");
    return B.CreateZExt(V, Type::getInt64Ty(F.getContext()));
  }

  bool rewriteAccess(CarrierContract &C, Instruction *Access) {
    Value *CarrierPtr = nullptr;
    if (auto *LI = dyn_cast<LoadInst>(Access))
      CarrierPtr = LI->getPointerOperand();
    else
      CarrierPtr = cast<StoreInst>(Access)->getPointerOperand();
    Value *Obj = getTileObject(CarrierPtr);
    IRBuilder<> B(Access);

    Value *ValidCol = nullptr;
    Value *ValidRow = nullptr;
    if (C.ValidCol > 0) {
      ValidCol = B.getInt64(C.ValidCol);
    } else if (Obj) {
      ValidCol = loadDimField(B, Obj, 4); // ColMaskInternal
    } else {
      return false;
    }
    if (C.ValidRow > 0) {
      ValidRow = B.getInt64(C.ValidRow);
    } else if (Obj) {
      ValidRow = loadDimField(B, Obj, 0); // RowMaskInternal
    } else {
      return false;
    }
    // LB2 carries the physical column count; encoding ValidCol matches the
    // omitted-LB2 transport semantics (omission selects Col=ValidCol).
    Value *Col = ValidCol;
    Value *Stride =
        B.getInt64((uint64_t)C.Cols * dataTypeElementBytes(C.DataType));

    if (auto *LI = dyn_cast<LoadInst>(Access)) {
      Function *TLoad = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_blk_tload, {LI->getType()});
      Value *Call = B.CreateCall(
          TLoad,
          {ValidCol, ValidRow, Col, B.getInt64(C.DataType),
           B.getInt64(0 /* PadValue::Zero */),
           B.getInt64(LoadTransportForLayout(C.Layout)), CarrierPtr, Stride},
          "linx.tile.transport.load");
      LI->replaceAllUsesWith(Call);
      LI->eraseFromParent();
      return true;
    }

    auto *SI = cast<StoreInst>(Access);
    Function *TStore = Intrinsic::getDeclaration(
        F.getParent(), Intrinsic::linx_blk_tstore,
        {SI->getValueOperand()->getType()});
    B.CreateCall(TStore,
                 {ValidCol, ValidRow, Col, B.getInt64(C.DataType),
                  B.getInt64(StoreTransportForLayout(C.Layout)), CarrierPtr,
                  Stride, SI->getValueOperand()});
    SI->eraseFromParent();
    return true;
  }

public:
  TileCarrierTransport(Function &F) : F(F) {}

  bool run() {
    bool Changed = false;
    SmallVector<Instruction *, 8> Accesses;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB) {
        MDNode *MD;
        if (isa<LoadInst>(I) || isa<StoreInst>(I))
          if ((MD = I.getMetadata("linx.tile.transport")))
            Accesses.push_back(&I);
      }

    for (Instruction *Access : Accesses) {
      MDNode *MD = Access->getMetadata("linx.tile.transport");
      CarrierContract C;
      bool Parsed = false;
      if (MD && MD->getNumOperands() == 1)
        if (auto *S = dyn_cast<MDString>(MD->getOperand(0).get()))
          Parsed = parseCarrierContract(S->getString(), C);
      if (!Parsed) {
        LLVM_DEBUG(dbgs() << "unparsed tile carrier contract near "
                          << *Access << "\n");
        continue;
      }
      LLVM_DEBUG(dbgs() << "carrier transport " << *Access << " -> layout="
                        << C.Layout << "\n");
      if (rewriteAccess(C, Access))
        Changed = true;
    }
    return Changed;
  }
};

} // namespace

PreservedAnalyses LinxV5TileCarrierTransportPass::run(Function &F,
                                                      FunctionAnalysisManager &) {
  return TileCarrierTransport(F).run() ? PreservedAnalyses::none()
                                       : PreservedAnalyses::all();
}
