//===-- LinxV5ClockhandsColoring.cpp - Color ClockHands -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5Subtarget.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Statistic.h"
#include <llvm/CodeGen/LiveDebugVariables.h>
#include <llvm/CodeGen/LiveIntervals.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/VirtRegMap.h>

using namespace llvm;

#define DEBUG_TYPE "linxv5-clockhands"

namespace {
class LinxV5ClockhandsPreAlloc : public MachineFunctionPass {
public:
  static char ID;
  MachineRegisterInfo *MRI;
  LinxV5ClockhandsPreAlloc() : MachineFunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<VirtRegMap>();
    AU.addRequired<LiveDebugVariables>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "LinxV5 Clockhands PreAlloc";
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
  void recalculateReserves(MachineFunction &MF);
  void releaseLiveDebugVariables(MachineFunction &MF);
};
} // namespace

char LinxV5ClockhandsPreAlloc::ID = 0;

INITIALIZE_PASS(LinxV5ClockhandsPreAlloc, "linxv5-clockhands-prealloc",
                "LinxV5 Clockhands PreAlloc", false, false)

void LinxV5ClockhandsPreAlloc::recalculateReserves(MachineFunction &MF) {
  MF.getInfo<LinxV5MachineFunctionInfo>()->startClockhandsAllocation();
  MRI->freezeReservedRegs(MF);
}

void LinxV5ClockhandsPreAlloc::releaseLiveDebugVariables(MachineFunction &MF) {
  getAnalysis<LiveDebugVariables>().emitDebugValues(&getAnalysis<VirtRegMap>());
}

bool LinxV5ClockhandsPreAlloc::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  MRI = &MF.getRegInfo();
  recalculateReserves(MF);
  releaseLiveDebugVariables(MF);

  if (MF.getSubtarget<LinxV5Subtarget>().getRegisterInfo()->getSTDRC() !=
      &LinxV5::VBXTRRegClass)
    return false;
  // HACK: rename VBXTRRegClass to MixedGPRRegClass.
  for (unsigned i = 0; i < MRI->getNumVirtRegs(); ++i) {
    Register Reg = Register::index2VirtReg(i);
    if (MRI->reg_empty(Reg))
      continue;
    if (MRI->getRegClass(Reg) == &LinxV5::VBXTRRegClass)
      MRI->setRegClass(Reg, &LinxV5::MixedGPRRegClass);
    else
      assert(MRI->getRegClass(Reg) == &LinxV5::GRRegClass);
  }
  return true;
}

FunctionPass *llvm::createLinxV5ClockhandsPreAllocPass() {
  return new LinxV5ClockhandsPreAlloc;
}

namespace {
class LinxV5ClockhandsPostAlloc : public MachineFunctionPass {
public:
  static char ID;
  LinxV5ClockhandsPostAlloc() : MachineFunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return "LinxV5 Clockhands PostAlloc";
  }
  bool runOnMachineFunction(MachineFunction &MF) override;
};
} // namespace

char LinxV5ClockhandsPostAlloc::ID = 0;

INITIALIZE_PASS(LinxV5ClockhandsPostAlloc, "linxv5-clockhands-postalloc",
                "LinxV5 Clockhands PostAlloc", false, false)

bool LinxV5ClockhandsPostAlloc::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  MachineRegisterInfo *MRI = &MF.getRegInfo();
  MF.getInfo<LinxV5MachineFunctionInfo>()->exitClockhandsAllocation();
  MRI->freezeReservedRegs(MF);

  return true;
}

FunctionPass *llvm::createLinxV5ClockhandsPostAllocPass() {
  return new LinxV5ClockhandsPostAlloc;
}
