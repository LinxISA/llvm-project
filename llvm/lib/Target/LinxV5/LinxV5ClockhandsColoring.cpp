//===-- LinxV5ClockhandsColoring.cpp - Color ClockHands -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5Subtarget.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include <llvm/CodeGen/LiveIntervals.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/VirtRegMap.h>

using namespace llvm;

// Only control by wireless
static cl::opt<bool>
    EnableClockHandOpt("linxv5-enable-clock-hand-opt",
                       cl::desc("Clock Hand Register Allocation Optimization"),
                       cl::init(true), cl::Hidden);

static cl::opt<bool>
    EnableSpillCheck("linxv5-enable-clock-hand-spill-check",
                       cl::desc("Clock Hand Spill Check Option"),
                       cl::init(true), cl::Hidden);

static cl::opt<bool> EnableSIMTClockHand("linxv5-enable-simt-clock-hand",
                                         cl::desc("SIMT Clock Hand Option"),
                                         cl::init(true), cl::Hidden);

static cl::opt<bool> EnableTileClockHand("linxv5-enable-tile-clock-hand",
                                         cl::desc("Tile Clock Hand Option"),
                                         cl::init(true), cl::Hidden);

cl::opt<unsigned> RoundNum("linxv5-clock-hand-coloring-round-num",
                           cl::desc("Clock Hand Coloring Round Number"),
                           cl::init(5), cl::Hidden);

#define DEBUG_TYPE "linxv5-clockhands"

namespace {

enum RegUnitState {
  RU_New,
  RU_Spill,
  RU_Done,
};

enum AssignAlgorithm {
  AA_None,
  AA_Expert_Fast,
  AA_Final_Fast_Assign,
};

struct RegUnit {
  Register Reg;
  RegUnitState State;
  AssignAlgorithm AA;
  // DUL: Def and Use Length
  DenseMap<const TargetRegisterClass *, unsigned> DULs;

  RegUnit(Register reg, RegUnitState state, AssignAlgorithm aa)
      : Reg(reg), State(state), AA(aa) {}
};

class LinxV5ClockhandsColoring : public MachineFunctionPass {
public:
  static char ID;
  bool runOnMachineFunction(MachineFunction &MF) override;

  LinxV5ClockhandsColoring() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "ClockHands Coloring"; }

  void releaseMemory() override { clear(); }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
    AU.addPreserved<MachineLoopInfo>();
    AU.addRequired<LiveIntervals>();
    AU.addRequired<VirtRegMap>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  SmallVector<SmallVector<MachineInstr *>> getChains(MachineFunction &MF);

  void assignScalarReg(MachineFunction &MF);
  void assignSIMTReg(MachineFunction &MF);
  void assignTileReg(MachineFunction &MF);
  void assignMultiOutput(MachineFunction &MF);
  bool assign(Register OriReg, const TargetRegisterClass *RC);
  bool isLoopInvariant(MachineLoop *Loop, MachineInstr &MI);

  void rematAddrs(MachineFunction &MF);

private:
  MachineRegisterInfo *MRI;
  LiveIntervals *LIS;
  VirtRegMap *VRM;
  MachineLoopInfo *MLI;
  SmallVector<SmallVector<MachineInstr *>> Chains;

private:
  bool assignLoopInvar(MachineFunction &MF,
                       DenseSet<const TargetRegisterClass *> &OriRCs,
                       const TargetRegisterClass *AssignRC);
  bool assignAccrosChain(MachineFunction &MF,
                         DenseSet<const TargetRegisterClass *> &OriRCs,
                         const TargetRegisterClass *AssignRC);
  void clear() { Chains.clear(); }
};
} // namespace

char LinxV5ClockhandsColoring::ID = 0;

INITIALIZE_PASS(LinxV5ClockhandsColoring, DEBUG_TYPE, "ClockHands Coloring",
                false, false)

class ColoringInChain {
public:
  ColoringInChain(SmallVector<MachineInstr *> chain,
                  DenseSet<const TargetRegisterClass *> &oriRCs,
                  SmallVector<const TargetRegisterClass *> &curRCs,
                  MachineRegisterInfo *mri, LiveIntervals *lis, VirtRegMap *vrm)
      : Chain(chain), OriRCs(oriRCs), CurRCs(curRCs), MRI(mri), LIS(lis),
        VRM(vrm) {
    for (auto *RC : curRCs) {
      RCLIs.insert(std::make_pair(RC, DenseSet<LiveInterval *>()));
    }
    assert(!curRCs.empty() && "assign Register class can not empty");
    OneHandRegNum = curRCs[0]->getNumRegs();
  }

  void assign();
  bool assign(RegUnit *RU, const TargetRegisterClass *RC, bool Force = false);

private:
  SmallVector<MachineInstr *> Chain;
  DenseSet<const TargetRegisterClass *> OriRCs; // To Be Colored
  SmallVector<const TargetRegisterClass *> CurRCs;

  MachineRegisterInfo *MRI;
  LiveIntervals *LIS;
  VirtRegMap *VRM;

  MapVector<Register, RegUnit> RUs;
  DenseMap<const TargetRegisterClass *, DenseSet<LiveInterval *>> RCLIs;
  unsigned OneHandRegNum;

  void seedRegUnits();
  bool expertFastAssign(const TargetRegisterClass *RC);
  void updateRCSlotIndex(DenseMap<MachineInstr *, unsigned> &RCSlotIndex,
                         DenseMap<MachineOperand *, unsigned> &DefIndex,
                         const TargetRegisterClass *RC);
  unsigned getDUL(Register Reg,
                  DenseMap<MachineInstr *, unsigned> &RegSlotIndex,
                  DenseMap<MachineOperand *, unsigned> &DefIndex);

  void finalFastAssign();
  bool spillCheck(LiveInterval &TargetLI, DenseSet<LiveInterval *> &LIs);
  void ReportAssignResult(StringRef Prefix);
};

static StringRef GetRCName(const TargetRegisterClass *RC) {
  if (RC == &LinxV5::MixedGPRRegClass)
    return "(MixedGPRRegClass)";
  if (RC == &LinxV5::Tile_ABSRegClass)
    return "(TileRegClass)";
  if (RC == &LinxV5::MixedGPRNoRARegClass)
    return "(MixedGPRNoRARegClass)";
  if (RC == &LinxV5::SIMTCGVRegClass)
    return "(SIMTCGVRegClass)";
  if (RC == &LinxV5::LTRRegClass)
    return "(T)";
  if (RC == &LinxV5::LURRegClass)
    return "(U)";
  if (RC == &LinxV5::Tile_TRRegClass)
    return "(Tile_T)";
  if (RC == &LinxV5::Tile_URRegClass)
    return "(Tile_U)";
  if (RC == &LinxV5::Tile_MRRegClass)
    return "(Tile_M)";
  if (RC == &LinxV5::Tile_NRRegClass)
    return "(Tile_N)";
  if (RC == &LinxV5::SIMT_VTRRegClass)
    return "(SIMT_T)";
  if (RC == &LinxV5::SIMT_VURRegClass)
    return "(SIMT_U)";
  if (RC == &LinxV5::SIMT_VMRRegClass)
    return "(SIMT_M)";
  if (RC == &LinxV5::SIMT_VNRRegClass)
    return "(SIMT_N)";
  return "(Unknown)";
}

void ColoringInChain::finalFastAssign() {
  for (auto &RU : RUs) {
    if (RU.second.State == RegUnitState::RU_Done)
      continue;
    // colelct all DUL in CurRCs
    SmallVector<std::pair<const TargetRegisterClass *, unsigned>> SortedRCs;
    for (auto *RC : CurRCs) {
      unsigned DUL = RU.second.DULs.lookup(RC);
      SortedRCs.emplace_back(RC, DUL);
    }
    llvm::sort(SortedRCs, [](auto &A, auto &B) { return A.second < B.second; });

    bool Assigned = false;
    // give priority to the least DUL RegisterClass assign to reduce "COPY"
    for (auto RC : SortedRCs) {
      Register OriReg = RU.second.Reg;
      auto assignedRC = MRI->getRegClass(RU.second.Reg);
      if (assign(&RU.second, RC.first, /*Force=*/false)) {
        Assigned = true;
        LLVM_DEBUG(dbgs() << "Final Assign " << printReg(OriReg)
                    << GetRCName(assignedRC) << " to "
                    << GetRCName(RC.first) << "\n");
        break;
      }
    }

    // assign all failed = force assign by the maximun DUL RegisterClass
    if (!Assigned) {
      LLVM_DEBUG(
          dbgs() << "Final Assign " << printReg(RU.second.Reg)
                 << " to any hands still failed, need force assign to spill\n");
      assign(&RU.second, SortedRCs.back().first, /*Force=*/true);
    }

    RU.second.AA = AssignAlgorithm::AA_Final_Fast_Assign;
  }
}

static SmallVector<MachineOperand *, 2> getDefOperands(MachineInstr *MI) {
  if (MI->isInlineAsm()) {
    LinxV5::SingleAsm SA = LinxV5::parseSingleAsm(MI);
    if (!SA.Defs.empty())
      return SA.Defs;
  }

  SmallVector<MachineOperand *, 2> Defs;
  for (MachineOperand &MO : MI->operands()) {
    if (MO.isReg() && MO.isDef() && MO.getReg().isVirtual())
      Defs.push_back(&MO);
  }

  return Defs;
}

void ColoringInChain::updateRCSlotIndex(
    DenseMap<MachineInstr *, unsigned> &RCSlotIndex,
    DenseMap<MachineOperand *, unsigned> &DefIndex,
    const TargetRegisterClass *RC) {
  RCSlotIndex.clear();
  unsigned Index = 1;
  LLVM_DEBUG(dbgs() << "RC Index of "
                    << MRI->getTargetRegisterInfo()->getRegClassName(RC)
                    << "\n");
  for (MachineInstr *MI : Chain) {
    RCSlotIndex.insert(std::make_pair(MI, Index));
    LLVM_DEBUG(dbgs() << "(" << Index << ") " << *MI);
    auto Defs = getDefOperands(MI);
    for (MachineOperand *MO : Defs) {
      Register Reg = MO->getReg();
      if (Reg.isPhysical())
        continue;
      auto RegRC = MRI->getRegClass(Reg);
      if (RegRC == RC || OriRCs.count(RegRC)) {
        DefIndex.insert(std::make_pair(MO, Index));
        ++Index;
      }
    }
  }
}

unsigned
ColoringInChain::getDUL(Register Reg,
                        DenseMap<MachineInstr *, unsigned> &RegSlotIndex,
                        DenseMap<MachineOperand *, unsigned> &DefIndex) {
  assert(MRI->hasOneDef(Reg));
  if (MRI->use_empty(Reg))
    return 0;
  MachineOperand *DefMO = MRI->getOneDef(Reg);
  MachineInstr *DefMI = DefMO->getParent();

  MachineInstr *UseMI = &*std::max_element(
      MRI->use_instructions(Reg).begin(), MRI->use_instructions(Reg).end(),
      [&](MachineInstr &LHS, MachineInstr &RHS) {
        return RegSlotIndex.lookup(&LHS) < RegSlotIndex.lookup(&RHS);
      });

  // UseMI may acrossCF in other Chain, not in current Chain
  if (!RegSlotIndex.count(UseMI))
    return 0;
  return RegSlotIndex.lookup(UseMI) - DefIndex.lookup(DefMO);
}

bool ColoringInChain::assign(RegUnit *RU, const TargetRegisterClass *RC,
                             bool Force) {
  Register OriReg = RU->Reg;
  LiveInterval &OriLI = LIS->getInterval(OriReg);
  DenseSet<LiveInterval *> LIs = RCLIs.lookup(RC);
  // normal assign should not make spill happen.
  if (!Force && spillCheck(OriLI, LIs)) {
    RU->State = RegUnitState::RU_Spill;
    return false;
  }

  Register NewReg = MRI->createVirtualRegister(RC);
  MRI->replaceRegWith(OriReg /*from*/, NewReg /*to*/);
  LiveInterval &NewLI = LIS->createAndComputeVirtRegInterval(NewReg);
  LIS->removeInterval(OriReg);
  RU->Reg = NewReg;
  if (Force)
    RU->State = RegUnitState::RU_Spill;
  else
    RU->State = RegUnitState::RU_Done;
  RCLIs[RC].insert(&NewLI);
  return true;
}

bool ColoringInChain::expertFastAssign(const TargetRegisterClass *RC) {
  DenseMap<MachineInstr *, unsigned> RCSlotIndex;
  DenseMap<MachineOperand *, unsigned> DefIndex;
  updateRCSlotIndex(RCSlotIndex, DefIndex, RC);

  SmallVector<RegUnit *> TBDRUs;
  for (auto &RUInfo : RUs) {
    RegUnit &RU = RUInfo.second;
    Register Reg = RU.Reg;
    if (RU.State == RegUnitState::RU_Done)
      continue;
    RU.DULs[RC] = getDUL(Reg, RCSlotIndex, DefIndex);
    TBDRUs.push_back(&RU);
  }

  bool Changed = false;
  for (RegUnit *RU : TBDRUs) {
    if (RU->DULs[RC] > OneHandRegNum)
      continue;
    Changed = true;
    if (assign(RU, RC))
      RU->AA = AssignAlgorithm::AA_Expert_Fast;
  }

  return Changed;
}

void ColoringInChain::seedRegUnits() {
  for (MachineInstr *MI : Chain) {
    for (MachineOperand &MO : MI->operands()) {
      if (!MO.isReg())
        continue;
      Register Reg = MO.getReg();
      if (RUs.count(Reg))
        continue;
      if (Reg.isVirtual() && OriRCs.count(MRI->getRegClass(Reg))) {
        // After RegisterCoalescer, some tile vregs may have multiple defs
        // (coalesced copies). Skip non-SSA regs instead of asserting; they
        // will be handled by the standard regalloc path, not Clockhands.
        if (!MRI->hasOneDef(Reg))
          continue;
        RegUnit RU = {Reg, RegUnitState::RU_New, AssignAlgorithm::AA_None};
        RUs.insert(std::make_pair(Reg, RU));
      }
    }
  }
}

void ColoringInChain::assign() {
  seedRegUnits();
  for (unsigned i = 2; i <= CurRCs.size(); ++i) {
    bool Changed;
    unsigned Round = 1;
    do {
      Changed = false;
      for (unsigned j = 0; j < i; ++j)
        Changed |= expertFastAssign(CurRCs[j]);
      ++Round;
    } while (Changed && Round <= RoundNum);
  }

  ReportAssignResult("ClockHand Coloring Expert Assign Report");
  finalFastAssign();
  ReportAssignResult("ClockHand Coloring Final Assign Report");
}

bool ColoringInChain::spillCheck(LiveInterval &TargetLI,
                                 DenseSet<LiveInterval *> &LIs) {
  if (!EnableSpillCheck)
    return false;
  unsigned MaxPressure = 0;
  for (SlotIndex Iter = TargetLI.beginIndex(); Iter != TargetLI.endIndex();) {
    unsigned CurPressure =
        llvm::count_if(LIs, [&](LiveInterval *LI) { return LI->liveAt(Iter); });
    MaxPressure = std::max(MaxPressure, CurPressure);
    Iter = Iter.getNextSlot();
  }
  return MaxPressure >= OneHandRegNum - 1;
}

static void pushMBBToChain(MachineBasicBlock &MBB,
                           SmallVector<MachineInstr *> &Chain) {
  for (MachineInstr &MI : MBB)
    Chain.push_back(&MI);
}

static bool isSingleSuccThruEmpty(MachineBasicBlock *MBB,
                                  MachineBasicBlock *Succ) {
  while (true) {
    MBB = MBB->getSingleSuccessor();
    if (MBB == Succ)
      return true;
    if (!MBB || !MBB->empty())
      return false;
  }
}

/// Chain: Link fall-through blocks into chain.
/// A and B can be Chain means A's single succ is B, and B's single pred is A.
SmallVector<SmallVector<MachineInstr *>>
LinxV5ClockhandsColoring::getChains(MachineFunction &MF) {
  SmallVector<MachineInstr *> Chain;
  for (MachineBasicBlock &MBB : MF) {
    if (Chain.empty()) {
      pushMBBToChain(MBB, Chain);
      continue;
    }
    MachineBasicBlock *Back = Chain.back()->getParent();
    if (Back->succ_size() == 1 && MBB.pred_size() == 1 &&
        isSingleSuccThruEmpty(Back, &MBB)) {
      pushMBBToChain(MBB, Chain);
    } else {
      Chains.push_back(Chain);
      Chain.clear();
      pushMBBToChain(MBB, Chain);
    }
  }
  if (!Chain.empty()) {
    Chains.push_back(Chain);
  }

  return Chains;
}

bool LinxV5ClockhandsColoring::assign(Register OriReg,
                                      const TargetRegisterClass *RC) {
  Register NewReg = MRI->createVirtualRegister(RC);
  MRI->replaceRegWith(OriReg /*from*/, NewReg /*to*/);
  LIS->createAndComputeVirtRegInterval(NewReg);
  LIS->removeInterval(OriReg);
  LLVM_DEBUG(dbgs() << "Assign " << printReg(OriReg) << " "
                    << GetRCName(MRI->getRegClass(OriReg)) << " to "
                    << printReg(NewReg) << GetRCName(MRI->getRegClass(NewReg))
                    << " success\n");
  return true;
}

void LinxV5ClockhandsColoring::assignScalarReg(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "\nAssignScalarReg Start\n");
  DenseSet<const TargetRegisterClass *> OriRCs = {
      &LinxV5::MixedGPRRegClass, &LinxV5::MixedGPRNoRARegClass};
  SmallVector<const TargetRegisterClass *> CurRCs = {&LinxV5::LTRRegClass,
                                                     &LinxV5::LURRegClass};
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT()) {
    bool LoopAssignRes = assignLoopInvar(MF, OriRCs, &LinxV5::LURRegClass);
    assignAccrosChain(MF, OriRCs,
                    LoopAssignRes ? &LinxV5::LTRRegClass
                                  : &LinxV5::LURRegClass);
  }
  for (auto Chain : Chains) {
    ColoringInChain CIC(Chain, OriRCs, CurRCs, MRI, LIS, VRM);
    CIC.assign();
  }
  LLVM_DEBUG(dbgs() << "\nAssignScalarReg End\n");
}

// TODO: maybe we have better API or method to determine isLoopInvariant later.
// So far, We assume that a non-single-def register is DEFINITELY NOT a loop invariant.
bool LinxV5ClockhandsColoring::isLoopInvariant(MachineLoop *Loop, MachineInstr &MI) {
  for (MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && !MRI->hasOneDef(MO.getReg()))
      return false;
  }
  return Loop->isLoopInvariant(MI);
}

bool LinxV5ClockhandsColoring::assignLoopInvar(
    MachineFunction &MF, DenseSet<const TargetRegisterClass *> &OriRCs,
    const TargetRegisterClass *AssignRC) {
  LLVM_DEBUG(dbgs() << "\nAssignLoopInvar Start\n");
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    if (MachineLoop *Loop = MLI->getLoopFor(&MBB)) {
      for (MachineInstr &MI : MBB) {
        if (!isLoopInvariant(Loop, MI))
          continue;
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isDef())
            continue;
          Register Reg = MO.getReg();
          if (!Reg.isVirtual())
            continue;
          if (OriRCs.count(MRI->getRegClass(Reg))) {
            Changed = true;
            assign(Reg, AssignRC);
          }
        }
      }
    }
  }
  LLVM_DEBUG(dbgs() << "\nAssignLoopInvar End\n");
  return Changed;
}

bool LinxV5ClockhandsColoring::assignAccrosChain(
    MachineFunction &MF, DenseSet<const TargetRegisterClass *> &OriRCs,
    const TargetRegisterClass *AssignRC) {
  LLVM_DEBUG(dbgs() << "\nAssignAccrosChain Start\n");
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isDef())
          continue;
        Register Reg = MO.getReg();
        if (!Reg.isVirtual())
          continue;
        if (OriRCs.count(MRI->getRegClass(Reg)) == 0)
          continue;
        LiveInterval &LI = LIS->getInterval(Reg);
        if (LI.size() <= 1)
          continue;
        Changed = true;
        assign(Reg, AssignRC);
      }
    }
  }
  LLVM_DEBUG(dbgs() << "\nAssignAccrosChain End\n");
  return Changed;
}

void LinxV5ClockhandsColoring::assignMultiOutput(MachineFunction &MF) {
  const TargetRegisterClass *RCs[] = {
      &LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
      &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass};
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      auto Defs = getDefOperands(&MI);
      if (Defs.size() > 1) {
        for (int i = 0; i < Defs.size(); ++i) {
          if (MRI->getRegClass(Defs[i]->getReg()) == &LinxV5::Tile_ABSRegClass)
            MRI->setRegClass(Defs[i]->getReg(), RCs[i]);
        }
      }
    }
  }
}

void LinxV5ClockhandsColoring::assignTileReg(MachineFunction &MF) {
  if (!EnableTileClockHand) {
    assignMultiOutput(MF);
    return;
  }
  LLVM_DEBUG(dbgs() << "\nAssignTileReg Start\n");
  DenseSet<const TargetRegisterClass *> OriRCs = {&LinxV5::Tile_ABSRegClass};

  bool LoopAssignRes = assignLoopInvar(MF, OriRCs, &LinxV5::Tile_NRRegClass);
  assignAccrosChain(MF, OriRCs,
                    LoopAssignRes ? &LinxV5::Tile_MRRegClass
                                  : &LinxV5::Tile_NRRegClass);
  SmallVector<const TargetRegisterClass *> CurRCs = {
      &LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
      &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass};
  for (auto Chain : Chains) {
    ColoringInChain CIC(Chain, OriRCs, CurRCs, MRI, LIS, VRM);
    CIC.assign();
  }
  LLVM_DEBUG(dbgs() << "\nAssignTileReg End\n");
}

void gatherAddrForm(SmallVectorImpl<MachineInstr *> &Form, MachineOperand &MO,
                    MachineFunction &MF) {
  auto &MRI = MF.getRegInfo();
}

void LinxV5ClockhandsColoring::rematAddrs(MachineFunction &MF) {
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.mayLoad() || MI.mayStore()) {
        SmallVector<MachineInstr *, 2> Form;
      }
    }
  }
}

void LinxV5ClockhandsColoring::assignSIMTReg(MachineFunction &MF) {
  // SIMT ClockhandsColor may cause more spill than RAGreedy
  if (!EnableSIMTClockHand) {
    return;
  }
  LLVM_DEBUG(dbgs() << "\nAssignSIMTReg Start\n");
  DenseSet<const TargetRegisterClass *> OriRCs = {&LinxV5::SIMTCGVLRegClass,
                                                  &LinxV5::SIMTCGVRegClass};

  bool LoopAssignRes = assignLoopInvar(MF, OriRCs, &LinxV5::SIMT_VNRRegClass);
  assignAccrosChain(MF, OriRCs,
                    LoopAssignRes ? &LinxV5::SIMT_VMRRegClass
                                  : &LinxV5::SIMT_VNRRegClass);
  SmallVector<const TargetRegisterClass *> CurRCs = {
      &LinxV5::SIMT_VTRRegClass, &LinxV5::SIMT_VURRegClass,
      &LinxV5::SIMT_VMRRegClass, &LinxV5::SIMT_VNRRegClass};
  for (auto Chain : Chains) {
    ColoringInChain CIC(Chain, OriRCs, CurRCs, MRI, LIS, VRM);
    CIC.assign();
  }
  LLVM_DEBUG(dbgs() << "\nAssignSIMTReg End\n");
}

void ColoringInChain::ReportAssignResult(StringRef Prefix) {
#ifndef NDEBUG
  if (RUs.empty())
    return;
  const DenseMap<unsigned, std::string> StateStrMap = {
      {RegUnitState::RU_New, "New"},
      {RegUnitState::RU_Spill, "Spill"},
      {RegUnitState::RU_Done, "Done"},
  };

  const DenseMap<unsigned, std::string> AlgorithmStrMap = {
      {AssignAlgorithm::AA_None, "None"},
      {AssignAlgorithm::AA_Expert_Fast, "Expert_Fast"},
      {AssignAlgorithm::AA_Final_Fast_Assign, "Final_Fast_Assign"},
  };

  auto getDULsInfo = [&](DenseMap<const TargetRegisterClass *, unsigned> DULs,
                         raw_ostream &output) {
    for (auto DUL : DULs)
      LLVM_DEBUG(output << "\tDUL" << GetRCName(DUL.first) << ":" << DUL.second);
  };

  LLVM_DEBUG(dbgs() << "\n==============" << Prefix << "==============\n");
  unsigned DoneNum = 0;
  for (auto &RUInfo : RUs) {
    Register OriReg = RUInfo.first;
    RegUnit &RU = RUInfo.second;
    raw_ostream &output = dbgs();
    LLVM_DEBUG(output << "Assign " << printReg(OriReg)
                      << GetRCName(MRI->getRegClass(OriReg)) << " to "
                      << printReg(RU.Reg) << GetRCName(MRI->getRegClass(RU.Reg))
                      << "\tReg State: " << StateStrMap.lookup(RU.State)
                      << ",\tAA: " << AlgorithmStrMap.lookup(RU.AA) << ",\t");
    LLVM_DEBUG(getDULsInfo(RU.DULs, output));
    if (RU.State == RegUnitState::RU_Done)
      ++DoneNum;
    LLVM_DEBUG(dbgs() << "\n");
  }

  LLVM_DEBUG(dbgs() << Prefix << " Total <" << RUs.size() << "> register, has assigned <"
         << DoneNum << ">, not assigned (" << RUs.size() - DoneNum << ").\n");

  LLVM_DEBUG(dbgs() << "\n");
#endif
}

bool LinxV5ClockhandsColoring::runOnMachineFunction(MachineFunction &MF) {

  MRI = &MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervals>();
  VRM = &getAnalysis<VirtRegMap>();
  MLI = &getAnalysis<MachineLoopInfo>();

  if (!EnableClockHandOpt)
    return false;
  getChains(MF);
  assignScalarReg(MF);
  assignSIMTReg(MF);
  assignTileReg(MF);

  VRM->grow();
  return true;
}

FunctionPass *llvm::createLinxV5ClockhandsColoringPass() {
  return new LinxV5ClockhandsColoring;
}
