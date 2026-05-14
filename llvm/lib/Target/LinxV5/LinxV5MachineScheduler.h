//===- LinxV5MachineScheduler.h - Custom LinxV5 MI scheduler --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Custom LinxV5 MI scheduler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINX_LINXV5MACHINESCHEDULER_H
#define LLVM_LIB_TARGET_LINX_LINXV5MACHINESCHEDULER_H

#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {
/// A MachineSchedStrategy implementation for LinxV5 pre RA scheduling.
class LinxV5PreRASchedStrategy : public GenericScheduler {
public:
  explicit LinxV5PreRASchedStrategy(const MachineSchedContext *C)
      : GenericScheduler(C) {}

  void initPolicy(MachineBasicBlock::iterator Begin,
                  MachineBasicBlock::iterator End,
                  unsigned NumRegionInstrs) override;

  SUnit *pickNode(bool &IsTopNode) override;

  bool tryPressure(const PressureChange &TryP, const PressureChange &CandP,
                   GenericSchedulerBase::SchedCandidate &TryCand,
                   GenericSchedulerBase::SchedCandidate &Cand,
                   GenericSchedulerBase::CandReason Reason) const;

protected:
  bool tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                    SchedBoundary *Zone) const override;

private:
  SUnit *LastScheduledSU = nullptr;

  bool shouldFastForward(SchedCandidate &Cand) const;

  bool tryCluster(SchedCandidate &Cand, SchedCandidate &TryCand) const;
};
} // end namespace llvm

#endif // LLVM_LIB_TARGET_LINX_LINXV5MACHINESCHEDULER_H
