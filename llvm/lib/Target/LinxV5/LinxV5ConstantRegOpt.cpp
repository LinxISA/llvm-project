// ===---- LinxV5ConstantRegOpt.cpp - Reg canonicalization ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===//
//
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-constant-reg-opt"
#define PASS_NAME "LinxV5 Constant Reg Propagation Optimization"

static cl::opt<bool> EnableLinxV5ConstantRegOpt(
    "enable-linxv5-constant-reg-opt", cl::init(true),
    cl::desc("Enable LinxV5 constant register optimization"));

namespace {

class LinxV5ConstantRegOpt : public FunctionPass {
public:
  static char ID;

  LinxV5ConstantRegOpt() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!EnableLinxV5ConstantRegOpt) {
      return false;
    }

    bool Changed = false;
    SmallVector<IntrinsicInst *> TargetCalls;
    for (Instruction &I : instructions(F)) {
      if (auto *II = dyn_cast<IntrinsicInst>(&I)) {
        if (isTargetIntrinsic(II)) {
          TargetCalls.push_back(II);
        }
      }
    }

    if (TargetCalls.empty())
      return false;

    for (IntrinsicInst *II : TargetCalls) {
      Changed |= processIntrinsicCall(II, F);
    }
    return Changed;
  }

private:
  bool isTargetIntrinsic(IntrinsicInst *II) {
    Intrinsic::ID ID = II->getIntrinsicID();
    return ID == Intrinsic::blkv_get_index_x ||
           ID == Intrinsic::blkv_get_index_y ||
           ID == Intrinsic::blkv_get_index_z ||
           ID == Intrinsic::blkv_get_tile_ptr;
  }

  // Find all call sites; if there are extended instructions, find the call
  // sites for those extended instructions; if there are no extended
  // instructions, find the call sites for intrinsics. Record the basic block
  // and call instruction information for all call sites.
  void collectAllUsePoints(
      IntrinsicInst *IntrinsicCall,
      DenseMap<BasicBlock *, SmallVector<Instruction *>> &DirectUses,
      DenseMap<BasicBlock *, SmallVector<std::pair<Instruction *, ZExtInst *>>>
          &ZExtUses) {
    for (User *U : IntrinsicCall->users()) {
      Instruction *UserInst = dyn_cast<Instruction>(U);
      if (!UserInst || UserInst == IntrinsicCall)
        continue;
      // Instructions for using zext
      if (isa<ZExtInst>(UserInst))
        collectZExtInstructionUses(cast<ZExtInst>(UserInst), DirectUses,
                                   ZExtUses);
      // Direct use of intrinsics
      else if (!isa<PHINode>(UserInst))
        DirectUses[UserInst->getParent()].push_back(UserInst);
    }
  }

  void collectZExtInstructionUses(
      ZExtInst *ZExt,
      DenseMap<BasicBlock *, SmallVector<Instruction *>> &DirectUses,
      DenseMap<BasicBlock *, SmallVector<std::pair<Instruction *, ZExtInst *>>>
          &ZExtUses) {

    for (User *ZU : ZExt->users()) {
      Instruction *ZExtUser = dyn_cast<Instruction>(ZU);
      if (!ZExtUser || ZExtUser == ZExt)
        continue;
      // Skip PHI Node
      if (isa<PHINode>(ZExtUser)) {
        DirectUses[ZExt->getParent()].push_back(ZExt);
        continue;
      }
      ZExtUses[ZExtUser->getParent()].push_back({ZExtUser, ZExt});
    }
  }

  // Copy this instruction at the beginning of the basic block for all call
  // sites: Clone the instruction at the block header:
  //   1. Clone the intrinsic instruction.
  //   2. Clone the relevant, non-repeating zext instructions called in the
  //   block.
  // Modify the operands:
  //   3. Replace the relevant operands in the call site with those in the
  //   cloned instruction.
  void copyToBlockBeginning(
      IntrinsicInst *IntrinsicCall, BasicBlock *BB,
      SmallVector<Instruction *> DirectUses,
      SmallVector<std::pair<Instruction *, ZExtInst *>> ZExtUses) {
    IRBuilder<> Builder(BB, BB->getFirstInsertionPt());

    // Clone intrinsic call
    IntrinsicInst *NewCall = cast<IntrinsicInst>(IntrinsicCall->clone());
    Builder.Insert(NewCall);

    // If exist, clone each zext type
    // include: zext to i64, zext to i32
    DenseMap<ZExtInst *, ZExtInst *> ZExtMap;
    if (!ZExtUses.empty()) {
      for (auto &UsePair : ZExtUses) {
        ZExtInst *ZExt = UsePair.second;
        if (!ZExtMap.count(ZExt)) {
          ZExtInst *NewZExt = cast<ZExtInst>(ZExt->clone());
          NewZExt->setOperand(0, NewCall);
          Builder.Insert(NewZExt);
          ZExtMap[ZExt] = NewZExt;
        }
      }
    }

    // replace
    // %0 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
    // %2 = getelementptr inbounds float, ptr %0, i64 %idxprom
    // to
    // %0 = tail call i16 @llvm.blkv.get.index.x()
    // %1 = zext i16 %0 to i64
    // %2 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
    // %3 = getelementptr inbounds float, ptr %2, i64 %1
    for (Instruction *UserInst : DirectUses) {
      for (unsigned i = 0; i < UserInst->getNumOperands(); ++i) {
        if (UserInst->getOperand(i) == IntrinsicCall)
          UserInst->setOperand(i, NewCall);
      }
    }

    if (ZExtUses.empty())
      return;
    for (auto &UsePair : ZExtUses) {
      Instruction *UserInst = UsePair.first;
      ZExtInst *ZExt = UsePair.second;
      if (!ZExtMap.count(ZExt))
        continue;
      for (unsigned i = 0; i < UserInst->getNumOperands(); ++i) {
        if (UserInst->getOperand(i) == ZExt)
          UserInst->setOperand(i, ZExtMap[ZExt]);
      }
    }
  }

  bool processIntrinsicCall(IntrinsicInst *IntrinsicCall, Function &F) {
    // The basic block where the intrinsic resides does not need to be
    // processed.
    BasicBlock *IntrinsicBlock = IntrinsicCall->getParent();
    DenseMap<BasicBlock *, SmallVector<Instruction *>> DirectUses;
    DenseMap<BasicBlock *, SmallVector<std::pair<Instruction *, ZExtInst *>>>
        ZExtUses;

    collectAllUsePoints(IntrinsicCall, DirectUses, ZExtUses);

    if (DirectUses.empty() && ZExtUses.empty()) {
      return false;
    }

    bool Changed = false;
    SmallPtrSet<BasicBlock *, 8> ProcessedBlocks;

    // Handling basic blocks with direct use
    for (auto &BlockEntry : DirectUses) {
      BasicBlock *BB = BlockEntry.first;
      if (BB == IntrinsicBlock)
        continue;
      if (ProcessedBlocks.insert(BB).second) {
        // Get the zext use of this basic block (if any)
        SmallVector<std::pair<Instruction *, ZExtInst *>> ZExtUsesInBB;
        auto it = ZExtUses.find(BB);
        if (it != ZExtUses.end()) {
          ZExtUsesInBB = it->second;
        }

        copyToBlockBeginning(IntrinsicCall, BB, BlockEntry.second,
                             ZExtUsesInBB);
        Changed = true;
      }
    }

    // Handling basic blocks used only by zext (not directly used)
    for (auto &BlockEntry : ZExtUses) {
      BasicBlock *BB = BlockEntry.first;
      if (BB == IntrinsicBlock)
        continue;
      if (ProcessedBlocks.insert(BB).second) {
        SmallVector<Instruction *> EmptyDirectUses;
        copyToBlockBeginning(IntrinsicCall, BB, EmptyDirectUses,
                             BlockEntry.second);
        Changed = true;
      }
    }

    cleanupOriginalInstructions(IntrinsicCall);

    return Changed;
  }

  void cleanupOriginalInstructions(IntrinsicInst *IntrinsicCall) {
    SmallVector<ZExtInst *> ZExtsToRemove;
    for (User *U : IntrinsicCall->users()) {
      if (auto *ZExt = dyn_cast<ZExtInst>(U)) {
        if (ZExt->use_empty()) {
          ZExtsToRemove.push_back(ZExt);
        }
      }
    }

    // Delete the zext instruction.
    for (ZExtInst *ZExt : ZExtsToRemove) {
      ZExt->eraseFromParent();
    }

    // Remove intrinsic calls (if not used elsewhere)
    if (IntrinsicCall->use_empty()) {
      IntrinsicCall->eraseFromParent();
    }
  }
};
} // namespace

char LinxV5ConstantRegOpt::ID = 0;

INITIALIZE_PASS(LinxV5ConstantRegOpt, DEBUG_TYPE, PASS_NAME, false, false)

FunctionPass *llvm::createLinxV5ConstantRegOptPass() {
  return new LinxV5ConstantRegOpt();
}