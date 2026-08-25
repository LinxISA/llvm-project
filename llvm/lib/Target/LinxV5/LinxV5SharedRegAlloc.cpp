//===-- LinxV5SharedRegAlloc.cpp - Allocate Shared registers -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-shared-regalloc"

namespace {

class LinxV5SharedRegAlloc : public MachineFunctionPass {
public:
  static char ID;

  LinxV5SharedRegAlloc() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "LinxV5 Shared Register Allocation";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.addRequired<VirtRegMap>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // namespace

char LinxV5SharedRegAlloc::ID = 0;

INITIALIZE_PASS(LinxV5SharedRegAlloc, DEBUG_TYPE,
                "LinxV5 Shared Register Allocation", false, false)

bool LinxV5SharedRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  LiveIntervals &LIS = getAnalysis<LiveIntervals>();
  VirtRegMap &VRM = getAnalysis<VirtRegMap>();

  SmallVector<LiveInterval *, 16> SharedIntervals;
  for (unsigned Index = 0; Index < MRI.getNumVirtRegs(); ++Index) {
    Register Reg = Register::index2VirtReg(Index);
    if (MRI.reg_nodbg_empty(Reg) ||
        MRI.getRegClass(Reg) != &LinxV5::Shared_ABSRegClass)
      continue;
    if (!LIS.hasInterval(Reg))
      report_fatal_error("LinxV5 Shared register has no live interval");
    SharedIntervals.push_back(&LIS.getInterval(Reg));
  }

  llvm::sort(SharedIntervals, [](const LiveInterval *LHS,
                                 const LiveInterval *RHS) {
    return LHS->beginIndex() < RHS->beginIndex();
  });

  SmallVector<SmallVector<LiveInterval *, 4>, 64> Assigned(64);
  for (LiveInterval *LI : SharedIntervals) {
    unsigned SharedIndex = 0;
    for (; SharedIndex < Assigned.size(); ++SharedIndex) {
      if (llvm::none_of(Assigned[SharedIndex],
                        [LI](const LiveInterval *Other) {
                          return LI->overlaps(*Other);
                        }))
        break;
    }

    if (SharedIndex == Assigned.size())
      report_fatal_error(
          "LinxV5 Shared register allocation exhausted S0..S63");

    MCPhysReg PhysReg = LinxV5::Shared_ABSRegClass.getRegister(SharedIndex);
    VRM.assignVirt2Phys(LI->reg(), PhysReg);
    Assigned[SharedIndex].push_back(LI);
  }

  return !SharedIntervals.empty();
}

FunctionPass *llvm::createLinxV5SharedRegAllocPass() {
  return new LinxV5SharedRegAlloc();
}
