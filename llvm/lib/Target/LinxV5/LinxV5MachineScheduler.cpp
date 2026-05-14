//===- LinxV5MachineScheduler.cpp - MI Scheduler for LinxV5 -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5MachineScheduler.h"
#include "LinxV5InstrInfo.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

SUnit *LinxV5PreRASchedStrategy::pickNode(bool &IsTopNode) {
  LastScheduledSU = GenericScheduler::pickNode(IsTopNode);
  return LastScheduledSU;
}

bool LinxV5PreRASchedStrategy::shouldFastForward(SchedCandidate &Cand) const {
  const MachineInstr *MI = Cand.SU->getInstr();

  if (MI->isCopy())
    return true;

  // TODO: setc Inst should fast-forward

  // TODO: live-out should fast-forward

  return false;
}

bool LinxV5PreRASchedStrategy::tryCluster(SchedCandidate &Cand,
                                          SchedCandidate &TryCand) const {

  bool ClusterCandAndLastSU = false, ClusterTryCandAndLastSU = false;

  for (SDep &Pred : Cand.SU->Preds) {
    if (Pred.getSUnit() == LastScheduledSU)
      ClusterCandAndLastSU = true;
  }
  for (SDep &Pred : TryCand.SU->Preds) {
    if (Pred.getSUnit() == LastScheduledSU)
      ClusterTryCandAndLastSU = true;
  }

  if (ClusterCandAndLastSU && !ClusterTryCandAndLastSU)
    return true;
  if (!ClusterCandAndLastSU && ClusterTryCandAndLastSU) {
    TryCand.Reason = Cluster;
    return true;
  }

  return false;
}

bool LinxV5PreRASchedStrategy::tryCandidate(SchedCandidate &Cand,
                                            SchedCandidate &TryCand,
                                            SchedBoundary *Zone) const {
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = NodeOrder;
    return true;
  }

  assert(Zone->isTop());

  // Always schedule Inst which needs fast-forward.
  if (shouldFastForward(Cand) && !shouldFastForward(TryCand))
    return false;
  if (!shouldFastForward(Cand) && shouldFastForward(TryCand)) {
    TryCand.Reason = Only1;
    return true;
  }

  // Cluster schedule DAG.
  if (tryCluster(Cand, TryCand))
    return TryCand.Reason != NoCand;

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical))
    return TryCand.Reason != NoCand;

  // Fall through to original instruction order.
  if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
      (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum)) {
    TryCand.Reason = NodeOrder;
    return true;
  }

  return false;
}

void LinxV5PreRASchedStrategy::initPolicy(MachineBasicBlock::iterator Begin,
                                          MachineBasicBlock::iterator End,
                                          unsigned NumRegionInstrs) {
  RegionPolicy.OnlyBottomUp = false;
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.ShouldTrackPressure = true;
}

bool LinxV5PreRASchedStrategy::tryPressure(
    const PressureChange &TryP, const PressureChange &CandP,
    GenericSchedulerBase::SchedCandidate &TryCand,
    GenericSchedulerBase::SchedCandidate &Cand,
    GenericSchedulerBase::CandReason Reason) const {
  // If one candidate decreases and the other increases, go with it.
  // Invalid candidates have UnitInc==0.
  if (tryGreater(TryP.getUnitInc() < 0, CandP.getUnitInc() < 0, TryCand, Cand,
                 Reason)) {
    return true;
  }
  // Do not compare the magnitude of pressure changes between top and bottom
  // boundary.
  if (Cand.AtTop != TryCand.AtTop)
    return false;

  // If both candidates affect the same set in the same boundary, go with the
  // smallest increase.
  unsigned TryPSet = TryP.getPSetOrMax();
  unsigned CandPSet = CandP.getPSetOrMax();
  if (TryPSet == CandPSet) {
    return tryLess(TryP.getUnitInc(), CandP.getUnitInc(), TryCand, Cand,
                   Reason);
  }

  return false;
}