// ===---- LinxV5RebindGetTilePTR.cpp - Reg canonicalization ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===//
//
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-rebind-get-tile-ptr"
#define PASS_NAME "LinxV5 Rebind blkv_get_tile_ptr"

namespace {
class LinxV5RebindGetTilePTR : public FunctionPass {
public:
  static char ID;
  Function *F;
  LinxV5RebindGetTilePTR() : FunctionPass(ID) {}
  bool runOnFunction(Function &f) override {
    F = &f;
    bool Changed = false;
    SmallVector<IntrinsicInst *> GTPs;
    for (Instruction &I : instructions(F)) {
      if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
        if (II->getIntrinsicID() == Intrinsic::blkv_get_tile_ptr) {
          GTPs.push_back(II);
        }
      }
    }

    for (IntrinsicInst *II : GTPs) {
      Changed |= maybeRebindGTP(II);
    }
    return Changed;
  }

  bool maybeRebindGTP(llvm::IntrinsicInst *II) {
    Value *Op = II->getOperand(0);
    if (isa<Argument>(Op))
      return false;
    if (SelectInst *SI = dyn_cast<SelectInst>(Op)) {
      // replace
      //   %0 = select %cond, <512 x float> %true, <512 x float> %false
      //   %1 = call ptr @llvm.blkv.get.tile.ptr.v512f32(<512 x float> %0)
      // to
      //   %0 = call ptr @llvm.blkv.get.tile.ptr.v512f32(<512 x float> %true)
      //   %1 = call ptr @llvm.blkv.get.tile.ptr.v512f32(<512 x float> %false)
      //   %2 = select %cond, ptr %0, ptr %1
      IRBuilder<> IRB(SI);
      Function *IntID = II->getCalledFunction();
      Instruction *I0 = IRB.CreateCall(IntID, {SI->getTrueValue()});
      Instruction *I1 = IRB.CreateCall(IntID, {SI->getFalseValue()});
      Value *Sel = IRB.CreateSelect(SI->getCondition(), I0, I1,
                                    SI->getName() + ".rebind", SI);
      II->replaceAllUsesWith(Sel);
      II->eraseFromParent();
      SI->eraseFromParent();
      return true;
    }
    llvm_unreachable("unexpected blkv_get_tile_ptr expr!");
  }
};
} // namespace

INITIALIZE_PASS(LinxV5RebindGetTilePTR, DEBUG_TYPE, PASS_NAME, false, false)
FunctionPass *llvm::createLinxV5RebindGetTilePTRPass() {
  return new LinxV5RebindGetTilePTR();
}

char LinxV5RebindGetTilePTR::ID = 0;
