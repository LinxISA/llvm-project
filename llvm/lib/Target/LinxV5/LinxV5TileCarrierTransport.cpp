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

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
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

// ---------------------------------------------------------------------------
// Phase 2 (issue #123): CUBE tile values live across real calls.
//
// Tile registers are caller-save, so regalloc spills such values through
// PseudoTSpill/PseudoTReload, which are hard-coded S64/NORM and lose the
// CUBE layout (a NORM reload feeding a CUBE consumer violates PTO #291).
// The layout of a TileOP asm result is not encoded in the asm string (the
// model infers it transitively), so the spill path cannot recover it.
// Instead of teaching regalloc, de-promote the value at IR level: store it
// to a private slot with a layout-correct transport right after its
// definition and reload it at every use. The tile virtual register then
// never crosses a call and no spill is generated.
// ---------------------------------------------------------------------------

namespace {

struct TileProvenance {
  unsigned Layout = 0;   // direct CUBE layout code: 29 CubeM32, 31 CubeM16
  unsigned DataType = 0; // LinxV5Op::DataType code
  Value *ValidCol = nullptr;
  Value *ValidRow = nullptr;
};

constexpr unsigned DirectCubeM32 = 29;
constexpr unsigned DirectCubeM16 = 31;

constexpr unsigned LoadTransportForDirect(unsigned Direct) {
  return Direct == DirectCubeM32 ? 21 : 22; // ND2M32 : ND2M16
}

constexpr unsigned StoreTransportForDirect(unsigned Direct) {
  return Direct == DirectCubeM32 ? 24 : 25; // M322ND : M162ND
}

// Resolve "$N" / "${N:...}" operand references in a TileOP asm template.
// $0 is the single output; $N (N >= 1) is CallInst arg N-1. Returns 0 when
// no further reference is found, otherwise the position just past it plus
// the referenced index in Idx.
static size_t nextAsmOperandRef(StringRef Asm, size_t Pos, unsigned &Idx) {
  Pos = Asm.find_first_of("$%", Pos);
  while (Pos != StringRef::npos) {
    size_t P = Pos + 1;
    if (Asm[P] == '{') {
      size_t Close = Asm.find('}', P);
      if (Close == StringRef::npos)
        return StringRef::npos;
      StringRef Body = Asm.substr(P + 1, Close - P - 1);
      size_t Colon = Body.find(':');
      if (Colon != StringRef::npos)
        Body = Body.substr(0, Colon);
      if (!Body.getAsInteger(10, Idx))
        return Close + 1;
    } else {
      size_t End = Asm.find_first_not_of("0123456789", P);
      StringRef Digits = Asm.slice(P, End);
      if (!Digits.empty()) {
        if (Digits.getAsInteger(10, Idx))
          return StringRef::npos;
        return End;
      }
    }
    Pos = Asm.find_first_of("$%", Pos + 1);
  }
  return StringRef::npos;
}

// Parse the (layout, DataType, valid shape) of a tile-producing TileOP asm
// call from its template string and constant arguments.
static bool getAsmTileProvenance(CallInst *Asm, TileProvenance &P) {
  InlineAsm *IA = dyn_cast<InlineAsm>(Asm->getCalledOperand());
  if (!IA) {
    return false;
  }
  StringRef AsmStr = IA->getAsmString();
  auto Arg = [&](unsigned Idx) -> Value * {
    return Idx >= 1 && Idx <= Asm->arg_size() ? Asm->getArgOperand(Idx - 1)
                                              : nullptr;
  };

  // BSTART dtype: the first "${N:D}" after "BSTART.".
  size_t BStart = AsmStr.find("BSTART.");
  if (BStart == StringRef::npos) {
    return false;
  }
  size_t Pos = BStart;
  while (true) {
    unsigned Idx = 0;
    size_t Next = nextAsmOperandRef(AsmStr, Pos, Idx);
    if (Next == StringRef::npos)
      return false;
    if (Idx >= 1) {
      size_t Dollar = AsmStr.rfind('$', Next);
      size_t RefEnd = AsmStr.find_first_of(",\n", Dollar);
      if (AsmStr.substr(Dollar, RefEnd - Dollar).contains(":D}")) {
        auto *DTy = dyn_cast_or_null<ConstantInt>(Arg(Idx));
        if (!DTy)
          return false;
        P.DataType = DTy->getZExtValue();
        Pos = Next;
        break;
      }
    }
    Pos = Next;
  }

  // Valid shape: the register operands of "B.DIM $K, 0, ->lb0/->lb1".
  P.ValidCol = nullptr;
  P.ValidRow = nullptr;
  size_t DimPos = 0;
  while (true) {
    size_t Dim = AsmStr.find("B.DIM $", DimPos);
    if (Dim == StringRef::npos)
      break;
    unsigned Idx = 0;
    size_t Next = nextAsmOperandRef(AsmStr, Dim + 6, Idx);
    if (Next == StringRef::npos)
      break;
    StringRef Rest = AsmStr.substr(Next);
    if (Rest.startswith(", 0, ->lb0") && !P.ValidCol)
      P.ValidCol = Arg(Idx);
    else if (Rest.startswith(", 0, ->lb1") && !P.ValidRow)
      P.ValidRow = Arg(Idx);
    DimPos = Next;
  }
  if (!P.ValidCol || !P.ValidRow) {
    return false;
  }

  // Layout: the ".if ${N:c} == 29/31" template conditional when present
  // (TMUL and friends encode the destination CUBE layout), otherwise the
  // single unreferenced constant 29/31 argument (TCVT preserves the source
  // layout and passes the code informationally).
  P.Layout = 0;
  for (unsigned Want : {DirectCubeM32, DirectCubeM16}) {
    size_t CondPos = 0;
    // find ".if ${N:c} == Want" for any N
    while (true) {
      size_t IfPos = AsmStr.find(".if ${", CondPos);
      if (IfPos == StringRef::npos)
        break;
      unsigned Idx = 0;
      size_t Next = nextAsmOperandRef(AsmStr, IfPos + 4, Idx);
      if (Next == StringRef::npos)
        break;
      StringRef Cond = AsmStr.substr(Next).ltrim();
      if (Cond.startswith(("== " + Twine(Want)).str())) {
        auto *L = dyn_cast_or_null<ConstantInt>(Arg(Idx));
        if (L && L->getZExtValue() == Want)
          P.Layout = Want;
        break;
      }
      CondPos = Next;
    }
    if (P.Layout)
      break;
  }
  if (!P.Layout) {
    // TCVT-style fallback: exactly one constant 29/31 argument.
    CallInst::op_iterator It = llvm::find_if(
        Asm->args(), [](Value *A) {
          auto *C = dyn_cast<ConstantInt>(A);
          return C && (C->getZExtValue() == DirectCubeM32 ||
                       C->getZExtValue() == DirectCubeM16);
        });
    if (It != Asm->arg_end())
      P.Layout = cast<ConstantInt>(*It)->getZExtValue();
  }
  LLVM_DEBUG(dbgs() << "provenance layout=" << P.Layout
                      << " dtype=" << P.DataType << " vc=" << !!P.ValidCol
                      << " vr=" << !!P.ValidRow << "\n");
  return P.Layout != 0 && dataTypeElementBytes(P.DataType) > 0;
}

class TileCallCrossing {
  Function &F;
  DominatorTree &DT;
  LLVMContext &Ctx;

  bool isRealCall(CallBase &CB) {
    if (CB.isInlineAsm() || isa<IntrinsicInst>(&CB))
      return false;
    return true;
  }

  void demoteValue(Instruction *Def, TileProvenance &P) {
    auto *VT = cast<FixedVectorType>(Def->getType());
    uint64_t PhysBytes = VT->getPrimitiveSizeInBits().getFixedValue() / 8;
    unsigned ElemBytes = dataTypeElementBytes(P.DataType);

    // The transport intrinsics take i64 dimensions; TileOP asm operands may
    // pass i32 constants or runtime values.
    IRBuilder<> EB(Def);
    auto ToI64 = [&](Value *V) -> Value * {
      if (V->getType()->isIntegerTy(64))
        return V;
      if (auto *C = dyn_cast<ConstantInt>(V))
        return EB.getInt64(C->getZExtValue());
      return EB.CreateZExt(V, Type::getInt64Ty(Ctx), "linx.tile.dim");
    };
    P.ValidCol = ToI64(P.ValidCol);
    P.ValidRow = ToI64(P.ValidRow);

    // The asm's BSTART dtype may describe the SOURCE (TCVT passes the source
    // type in ${1:D}), so the provenance DataType can carry the wrong width
    // for the value being demoted. The slot is private scratch: derive the
    // element width from the physical payload and pick a width-matched
    // integer carrier, which preserves the bytes and keeps the CUBE
    // transport width-legal.
    if (auto *VC = dyn_cast<ConstantInt>(P.ValidCol))
      if (auto *VR = dyn_cast<ConstantInt>(P.ValidRow)) {
        uint64_t Elems = VC->getZExtValue() * VR->getZExtValue();
        if (Elems && PhysBytes % Elems == 0) {
          unsigned Derived = PhysBytes / Elems;
          if (Derived == 1 || Derived == 2 || Derived == 4 || Derived == 8)
            ElemBytes = Derived;
        }
      }
    if (!ElemBytes)
      return;
    switch (ElemBytes) {
    case 1:
      P.DataType = 27; // U8
      break;
    case 2:
      P.DataType = 26; // U16
      break;
    case 4:
      P.DataType = 17; // S32
      break;
    case 8:
      P.DataType = 16; // S64
      break;
    }

    BasicBlock &Entry = F.getEntryBlock();
    AllocaInst *Slot = new AllocaInst(
        Type::getInt8Ty(Ctx), 0,
        ConstantInt::get(Type::getInt64Ty(Ctx), PhysBytes), Align(32),
        "linx.tile.slot", &*Entry.getFirstInsertionPt());

    IRBuilder<> B(Def->getNextNode());
    Value *Stride = P.ValidCol;
    if (auto *C = dyn_cast<ConstantInt>(P.ValidCol))
      Stride = B.getInt64(C->getZExtValue() * ElemBytes);
    else
      Stride = B.CreateMul(P.ValidCol, B.getInt64(ElemBytes));

    Function *TStore = Intrinsic::getDeclaration(
        F.getParent(), Intrinsic::linx_blk_tstore, {VT});
    CallInst *Store = B.CreateCall(
        TStore,
        {P.ValidCol, P.ValidRow, P.ValidCol, B.getInt64(P.DataType),
         B.getInt64(StoreTransportForDirect(P.Layout)), Slot, Stride, Def});

    Function *TLoad =
        Intrinsic::getDeclaration(F.getParent(), Intrinsic::linx_blk_tload, {VT});
    SmallVector<Instruction *, 8> Users;
    for (User *U : Def->users())
      if (U != Store && isa<Instruction>(U))
        Users.push_back(cast<Instruction>(U));
    for (Instruction *User : Users) {
      IRBuilder<> LB(User);
      Value *Reload = LB.CreateCall(
          TLoad,
          {P.ValidCol, P.ValidRow, P.ValidCol, B.getInt64(P.DataType),
           B.getInt64(0), B.getInt64(LoadTransportForDirect(P.Layout)), Slot,
           Stride},
          "linx.tile.slot.load");
      User->replaceUsesOfWith(Def, Reload);
    }
  }

public:
  TileCallCrossing(Function &F, DominatorTree &DT)
      : F(F), DT(DT), Ctx(F.getContext()) {}

  bool run() {
    // CUBE tile values: transport-load results and CUBE asm results.
    SmallVector<std::pair<Instruction *, TileProvenance>, 8> Tiles;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        auto *CB = dyn_cast<CallBase>(&I);
        if (!CB || !CB->getType()->isVectorTy())
          continue;
        TileProvenance P;
        bool Known = false;
        if (auto *II = dyn_cast<IntrinsicInst>(CB)) {
          if (II->getIntrinsicID() == Intrinsic::linx_blk_tload) {
            // args: (ValidCol, ValidRow, Col, DataType, Pad, Layout, Ptr,
            // Stride)
            auto *L = dyn_cast<ConstantInt>(II->getArgOperand(5));
            auto *D = dyn_cast<ConstantInt>(II->getArgOperand(3));
            if (L && D) {
              unsigned Code = L->getZExtValue();
              if (Code == 21) {
                P.Layout = DirectCubeM32;
                P.DataType = D->getZExtValue();
                P.ValidCol = II->getArgOperand(0);
                P.ValidRow = II->getArgOperand(1);
                Known = dataTypeElementBytes(P.DataType) > 0;
              } else if (Code == 22) {
                P.Layout = DirectCubeM16;
                P.DataType = D->getZExtValue();
                P.ValidCol = II->getArgOperand(0);
                P.ValidRow = II->getArgOperand(1);
                Known = dataTypeElementBytes(P.DataType) > 0;
              }
            }
          }
        } else if (auto *CI = dyn_cast<CallInst>(CB)) {
          Known = getAsmTileProvenance(CI, P);
        }
        if (Known)
          Tiles.push_back({&I, P});
      }
    }

    // Clobbering calls: real calls only; TileOP asm ops and intrinsics
    // become ordinary machine instructions without caller-save effects.
    SmallVector<CallBase *, 8> Calls;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *CB = dyn_cast<CallBase>(&I))
          if (isRealCall(*CB))
            Calls.push_back(CB);
    if (Calls.empty() || Tiles.empty())
      return false;

    bool Changed = false;
    for (auto &KV : Tiles) {
      Instruction *Def = KV.first;
      TileProvenance &P = KV.second;
      if (any_of(Def->users(), [](User *U) { return isa<PHINode>(U); }))
        continue; // v1: phi uses keep today's behaviour

      // The value crosses a call when its definition dominates the call and
      // some use lies below the call.
      bool Crosses = false;
      for (CallBase *C : Calls) {
        if (!DT.dominates(Def, C))
          continue;
        if (any_of(Def->users(), [&](User *U) {
              return DT.dominates(C, cast<Instruction>(U));
            })) {
          Crosses = true;
          break;
        }
      }
      if (!Crosses) {
        LLVM_DEBUG(dbgs() << "no crossing for " << *Def << "\n");
        continue;
      }

      LLVM_DEBUG(dbgs() << "demote cross-call CUBE tile " << *Def
                        << " layout=" << P.Layout << "\n");
      demoteValue(Def, P);
      Changed = true;
    }
    return Changed;
  }
};

} // namespace

namespace llvm {

PreservedAnalyses LinxV5TileCarrierTransportPass::run(Function &F,
                                                      FunctionAnalysisManager &AM) {
  bool Changed = TileCarrierTransport(F).run();
  auto &DT = AM.getResult<DominatorTreeAnalysis>(F);
  Changed |= TileCallCrossing(F, DT).run();
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace llvm
