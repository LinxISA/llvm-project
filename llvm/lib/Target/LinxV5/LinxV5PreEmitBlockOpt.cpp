// ===------------------------ LinxV5PreEmitBlockOpt.cpp
// ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Do some Block level optimize for LinxV5 Target before asm emit.
//
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "block-opt"
#define LINX_PRE_EMIT_BLOCK_OPTIMIZE "LinxV5 Pre Emit Block Optimize"

namespace {
class LinxV5PreEmitBlockOpt : public MachineFunctionPass {
public:
  static char ID;
  LinxV5PreEmitBlockOpt() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override {
    return LINX_PRE_EMIT_BLOCK_OPTIMIZE;
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  bool mergeDebugBlockToSuccessor(MachineFunction &MF);
  bool removeEmptyPrologueBlock(MachineFunction &MF);
};
} // namespace

char LinxV5PreEmitBlockOpt::ID = 0;

INITIALIZE_PASS_BEGIN(LinxV5PreEmitBlockOpt, DEBUG_TYPE,
                      LINX_PRE_EMIT_BLOCK_OPTIMIZE, false, false)
INITIALIZE_PASS_END(LinxV5PreEmitBlockOpt, DEBUG_TYPE,
                    LINX_PRE_EMIT_BLOCK_OPTIMIZE, false, false)

static void removeDeadBlock(MachineFunction &MF, MachineBasicBlock &MBB) {
  assert(MBB.pred_empty() && "MBB must be dead!");
  while (!MBB.succ_empty())
    MBB.removeSuccessor(MBB.succ_end() - 1);
  MF.erase(&MBB);
}

/// Note: empty prologue block is necessary for BlockFreqAnalyze Pass.
/// After Pre Emit Block Opt, we can not use BlockFreqAnalyze.
bool LinxV5PreEmitBlockOpt::removeEmptyPrologueBlock(MachineFunction &MF) {
  bool Changed = false;

  MachineBasicBlock &Prologue = *MF.begin();
  if (Prologue.empty() && !Prologue.isEHPad() && !Prologue.hasAddressTaken() &&
      Prologue.succ_size() == 1) {
    removeDeadBlock(MF, Prologue);
    Changed = true;
  }

  return Changed;
}

bool LinxV5PreEmitBlockOpt::mergeDebugBlockToSuccessor(MachineFunction &MF) {
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    if ((MBB.size() == 0) || (MBB.getFirstNonDebugInstr() != MBB.end()))
      continue;

    assert(MBB.succ_size() < 2 && "Less than two successor blocks are permitted");

    for (auto &MI : make_early_inc_range((reverse(MBB)))) {
      for (MachineBasicBlock *SuccBB : MBB.successors()) {
        SuccBB->splice(SuccBB->begin(), &MBB, &MI);
        Changed = true;
      }
    }
  }

  return Changed;
}

bool LinxV5PreEmitBlockOpt::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  Changed |= mergeDebugBlockToSuccessor(MF);
  Changed |= removeEmptyPrologueBlock(MF);

  return Changed;
}

FunctionPass *llvm::createLinxV5PreEmitBlockOptPass() {
  return new LinxV5PreEmitBlockOpt();
}
