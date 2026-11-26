//===-- LinxV5PrivateObjectPromotion.cpp - Preserve private wrapper fields ---===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

using namespace llvm;

namespace {
struct Access {
  Instruction *I;
  int64_t Offset;
  uint64_t Bytes;
  Type *Ty;
  bool Scoped;
  bool Simple;
};

// Scoping can precede the inlining that proves a C++ wrapper does not escape.
// Recover only requests created by our implicit scalar sweep, and only after
// proving the entire object use graph and every overlapping byte access.
static bool promoteFields(Function &F, DominatorTree &DT) {
  const DataLayout &DL = F.getParent()->getDataLayout();
  SmallVector<AllocaInst *, 16> Objects;
  for (Instruction &I : F.getEntryBlock())
    if (auto *A = dyn_cast<AllocaInst>(&I))
      if (A->isStaticAlloca())
        Objects.push_back(A);
  SmallVector<AllocaInst *, 16> Slots;
  for (AllocaInst *Object : Objects) {
    SmallVector<Value *, 16> Work{Object};
    SmallPtrSet<Value *, 32> Seen;
    SmallVector<Access, 32> Accesses;
    bool Safe = true;
    while (!Work.empty() && Safe) {
      Value *Pointer = Work.pop_back_val();
      if (!Seen.insert(Pointer).second)
        continue;
      for (User *U : Pointer->users()) {
        if (isa<GetElementPtrInst>(U) || isa<BitCastInst>(U)) {
          Work.push_back(cast<Value>(U));
          continue;
        }
        auto *I = dyn_cast<Instruction>(U);
        auto *II = dyn_cast<IntrinsicInst>(U);
        if (II && (II->isLifetimeStartOrEnd() || isa<DbgInfoIntrinsic>(II)))
          continue;
        Value *Address = nullptr;
        Type *Ty = nullptr;
        uint64_t Bytes = 0;
        bool Scoped = false, Simple = false;
        if (auto *L = dyn_cast<LoadInst>(U)) {
          Address = L->getPointerOperand();
          Ty = L->getType();
          Simple = L->isSimple();
        } else if (auto *S = dyn_cast<StoreInst>(U)) {
          if (S->getValueOperand() == Pointer) {
            Safe = false;
            break;
          }
          Address = S->getPointerOperand();
          Ty = S->getValueOperand()->getType();
          Simple = S->isSimple();
        } else if (II && (II->getIntrinsicID() == Intrinsic::linx_blk_tload ||
                          II->getIntrinsicID() == Intrinsic::linx_blk_tstore)) {
          // A numeric carrier transport touches only the carrier byte range,
          // not the adjacent wrapper shape fields. Never promote the carrier
          // through this scalar path.
          bool Store = II->getIntrinsicID() == Intrinsic::linx_blk_tstore;
          Address = II->getArgOperand(Store ? 5 : 6);
          Ty = Store ? II->getArgOperand(7)->getType() : II->getType();
          auto *Cols = dyn_cast<ConstantInt>(II->getArgOperand(0));
          auto *Rows = dyn_cast<ConstantInt>(II->getArgOperand(1));
          auto *DType = dyn_cast<ConstantInt>(II->getArgOperand(3));
          auto *Layout = dyn_cast<ConstantInt>(II->getArgOperand(Store ? 4 : 5));
          auto *Stride = dyn_cast<ConstantInt>(II->getArgOperand(Store ? 6 : 7));
          unsigned Bits = DType && DType->getValue().ule(31)
                              ? LinxV5::getTileDataTypeBits(DType->getZExtValue())
                              : 0;
          // The vector type alone does not bound strided memory accesses.
          // Unknown shapes, packed types and other layouts stay in memory.
          if (!Cols || !Rows || !Stride || !Layout || Bits < 8 ||
              !(Layout->isZero() ||
                (Store ? LinxV5Op::isCubeStoreConversion(Layout->getZExtValue())
                       : LinxV5Op::isCubeLoadConversion(Layout->getZExtValue()))) ||
              Cols->getValue().ugt(UINT64_MAX / (Bits / 8)) || Rows->isZero()) {
            Safe = false;
            break;
          }
          uint64_t RowBytes = Cols->getZExtValue() * (Bits / 8);
          uint64_t Pitch = Stride->getZExtValue();
          uint64_t RowCount = Rows->getZExtValue();
          if (Pitch < RowBytes ||
              (Pitch && RowCount - 1 > (UINT64_MAX - RowBytes) / Pitch) ||
              !Ty->isSized() || DL.getTypeStoreSize(Ty).isScalable()) {
            Safe = false;
            break;
          }
          Bytes = std::max(DL.getTypeStoreSize(Ty).getFixedValue(),
                           (RowCount - 1) * Pitch + RowBytes);
        } else if (II && II->getMetadata("linx.scalar.implicit") &&
                   (II->getIntrinsicID() == Intrinsic::linx_scalar_load ||
                    II->getIntrinsicID() == Intrinsic::linx_scalar_store)) {
          bool Store = II->getIntrinsicID() == Intrinsic::linx_scalar_store;
          auto *Width = dyn_cast<ConstantInt>(II->getArgOperand(Store ? 2 : 1));
          auto *Scope = dyn_cast<ConstantInt>(II->getArgOperand(Store ? 3 : 2));
          if (Width && Scope && Scope->getZExtValue() == 1) {
            Bytes = Width->getZExtValue();
            if (Bytes == 1 || Bytes == 2 || Bytes == 4 || Bytes == 8) {
              Address = II->getArgOperand(0);
              Ty = IntegerType::get(F.getContext(), Bytes * 8);
              Scoped = Simple = true;
            }
          }
        }
        int64_t Offset = 0;
        if (!I || !Address || Address != Pointer ||
            GetPointerBaseWithConstantOffset(Address, Offset, DL) != Object ||
            Offset < 0 || !Ty || !Ty->isSized() ||
            DL.getTypeStoreSize(Ty).isScalable()) {
          Safe = false;
          break;
        }
        if (!Bytes)
          Bytes = DL.getTypeStoreSize(Ty).getFixedValue();
        auto ObjectSize = Object->getAllocationSizeInBits(DL);
        if (!ObjectSize || ObjectSize->isScalable() ||
            uint64_t(Offset) > ObjectSize->getFixedValue() / 8 ||
            Bytes > ObjectSize->getFixedValue() / 8 - uint64_t(Offset)) {
          Safe = false;
          break;
        }
        Accesses.push_back({I, Offset, Bytes, Ty, Scoped, Simple});
      }
    }
    if (!Safe)
      continue;
    SmallSet<int64_t, 16> Done;
    for (const Access &Candidate : Accesses) {
      if (!Candidate.Scoped || !Done.insert(Candidate.Offset).second)
        continue;
      SmallVector<const Access *, 8> Matching;
      bool Exact = true;
      for (const Access &A : Accesses) {
        if (uint64_t(A.Offset) >=
                uint64_t(Candidate.Offset) + Candidate.Bytes ||
            uint64_t(Candidate.Offset) >= uint64_t(A.Offset) + A.Bytes)
          continue;
        if (A.Offset != Candidate.Offset || A.Bytes != Candidate.Bytes ||
            A.Ty != Candidate.Ty || !A.Simple) {
          Exact = false;
          break;
        }
        Matching.push_back(&A);
      }
      if (!Exact)
        continue;
      auto *Slot = new AllocaInst(Candidate.Ty, DL.getAllocaAddrSpace(),
                                  nullptr, Align(1), "linx.private.field",
                                  &*F.getEntryBlock().getFirstInsertionPt());
      for (const Access *A : Matching) {
        if (!A->Scoped) {
          if (auto *L = dyn_cast<LoadInst>(A->I))
            L->setOperand(0, Slot);
          else
            cast<StoreInst>(A->I)->setOperand(1, Slot);
          continue;
        }
        auto *II = cast<IntrinsicInst>(A->I);
        IRBuilder<> B(II);
        B.SetCurrentDebugLocation(II->getDebugLoc());
        if (II->getIntrinsicID() == Intrinsic::linx_scalar_load) {
          Value *V = B.CreateAlignedLoad(Candidate.Ty, Slot, Align(1));
          II->replaceAllUsesWith(B.CreateZExtOrTrunc(V, II->getType()));
        } else {
          B.CreateAlignedStore(
              B.CreateTruncOrBitCast(II->getArgOperand(1), Candidate.Ty), Slot,
              Align(1));
        }
        II->eraseFromParent();
      }
      Slots.push_back(Slot);
    }
  }
  if (Slots.empty())
    return false;
  PromoteMemToReg(Slots, DT);
  return true;
}
} // namespace

PreservedAnalyses
LinxV5PrivateObjectPromotionPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  if (F.hasFnAttribute("linx-scalar-shareability"))
    return PreservedAnalyses::all();
  bool Changed = false;
  const DataLayout &DL = F.getParent()->getDataLayout();
  for (Instruction &I : make_early_inc_range(instructions(F))) {
    auto *II = dyn_cast<IntrinsicInst>(&I);
    if (!II || II->getIntrinsicID() != Intrinsic::linx_scalar_load ||
        !II->getMetadata("linx.scalar.implicit"))
      continue;
    auto *Pointer = dyn_cast<Constant>(II->getArgOperand(0));
    auto *Width = dyn_cast<ConstantInt>(II->getArgOperand(1));
    auto *Scope = dyn_cast<ConstantInt>(II->getArgOperand(2));
    if (!Pointer || !Width || !Scope || Scope->getZExtValue() != 1)
      continue;
    unsigned Bytes = Width->getZExtValue();
    if (Bytes != 1 && Bytes != 2 && Bytes != 4 && Bytes != 8)
      continue;
    Type *Ty = IntegerType::get(F.getContext(), Bytes * 8);
    if (Constant *V = ConstantFoldLoadFromConstPtr(Pointer, Ty, DL)) {
      if (isa<UndefValue>(V) || isa<PoisonValue>(V))
        continue;
      II->replaceAllUsesWith(ConstantExpr::getZExtOrBitCast(V, II->getType()));
      II->eraseFromParent();
      Changed = true;
    }
  }
  Changed |= promoteFields(F, AM.getResult<DominatorTreeAnalysis>(F));
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
