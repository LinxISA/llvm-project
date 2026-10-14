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
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/Support/MathExtras.h"
#include <llvm/CodeGen/LiveIntervals.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/VirtRegMap.h>
#include <llvm/IR/ConstantRange.h>
#include <llvm/IR/InlineAsm.h>
#include <functional>
#include <map>
#include <set>

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
  SmallVector<Register> findOpenAssembleParents(MachineFunction &MF);
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

static bool isAssemble(const MachineInstr &MI) {
  return MI.isInlineAsm() &&
         StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
             .contains("B.ASSEMBLE");
}

struct AssembleParentEvent {
  bool Defines = false;
  bool References = false;
  bool Last = false;
};

static AssembleParentEvent getAssembleParentEvent(MachineInstr &MI,
                                                  Register Parent) {
  AssembleParentEvent Event;
  if (!isAssemble(MI))
    return Event;

  for (MachineOperand *Def : getDefOperands(&MI))
    Event.Defines |= Def->getReg() == Parent;
  Event.References = LinxV5::getAssembleParentRef(MI) == Parent;
  if (!Event.Defines && !Event.References)
    return Event;

  Optional<bool> Init = LinxV5::getAssembleControlBit(MI, /*InitIdx=*/0);
  Optional<bool> Last = LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
  if (!Init || !Last)
    report_fatal_error(
        "B.ASSEMBLE Init/Last controls must be immediate operands");
  if (Event.Defines && !*Init)
    report_fatal_error("B.ASSEMBLE parent definition requires Init=1");
  if (Event.References && *Init)
    report_fatal_error("B.ASSEMBLE continuation requires Init=0");
  Event.Last = *Last;
  return Event;
}

enum class ParentPhaseResult { Disjoint, Overlap, Invalid, UnableToProve };

// Register IDs occupy 32 bits. Higher keys name compiler-private scalar spill
// locations; keeping both in the same lattice preserves facts across joins and
// applies the existing loop widening/work budget to memory-carried counters.
using ParentRangeLocation = uint64_t;
using ParentRangeEnvironment = std::map<ParentRangeLocation, ConstantRange>;
static constexpr ParentRangeLocation FirstParentSpillLocation = uint64_t(1) << 32;

struct ParentSpillAccess {
  int FI;
  int64_t Offset;
  bool operator<(const ParentSpillAccess &Other) const {
    return std::tie(FI, Offset) < std::tie(Other.FI, Other.Offset);
  }
};

static Optional<ParentSpillAccess>
getParentSpillAccess(const MachineInstr &MI, const MachineFrameInfo &MFI,
                     bool IsLoad) {
  if (MI.getOpcode() != (IsLoad ? LinxV5::LDI : LinxV5::SDI) ||
      !MI.hasOneMemOperand())
    return None;
  const MachineMemOperand *MMO = *MI.memoperands_begin();
  if ((IsLoad ? !MMO->isLoad() || MMO->isStore()
              : !MMO->isStore() || MMO->isLoad()) ||
      MMO->isAtomic() || MMO->isVolatile() || MMO->getSize() != 8 ||
      MMO->getAlign() < Align(8))
    return None;
  const auto *PSV =
      dyn_cast_or_null<FixedStackPseudoSourceValue>(MMO->getPseudoValue());
  if (!PSV || !MFI.isSpillSlotObjectIndex(PSV->getFrameIndex()) ||
      MFI.isDeadObjectIndex(PSV->getFrameIndex()) || PSV->mayAlias(&MFI))
    return None;
  int64_t ObjectSize = MFI.getObjectSize(PSV->getFrameIndex());
  if (ObjectSize < 8 || MMO->getOffset() < 0 ||
      MMO->getOffset() > ObjectSize - 8)
    return None;
  return ParentSpillAccess{PSV->getFrameIndex(), MMO->getOffset()};
}

static ConstantRange getParentLocationRange(
    const ParentRangeEnvironment &Environment, ParentRangeLocation Location) {
  auto It = Environment.find(Location);
  return It == Environment.end() ? ConstantRange::getFull(64) : It->second;
}

static void setParentLocationRange(ParentRangeEnvironment &Environment,
                                   ParentRangeLocation Location,
                                   const ConstantRange &Range) {
  if (Range.isFullSet())
    Environment.erase(Location);
  else {
    auto It = Environment.find(Location);
    if (It == Environment.end())
      Environment.emplace(Location, Range);
    else
      It->second = Range;
  }
}

struct ParentPhaseStateKey {
  MachineBasicBlock *MBB = nullptr;
  unsigned Open = 0;

  bool operator<(const ParentPhaseStateKey &Other) const {
    return std::tie(MBB, Open) < std::tie(Other.MBB, Other.Open);
  }
};

static ConstantRange getParentRange(const ParentRangeEnvironment &Environment,
                                    Register Reg) {
  if (Reg == LinxV5::R0)
    return ConstantRange(APInt(64, 0));
  return getParentLocationRange(Environment, Reg.id());
}

static void setParentRange(ParentRangeEnvironment &Environment, Register Reg,
                           const ConstantRange &Range) {
  setParentLocationRange(Environment, Reg.id(), Range);
}

// Missing entries mean Top.  Therefore a fact survives a join only when both
// incoming environments carry it.
static bool joinParentRanges(ParentRangeEnvironment &Into,
                             const ParentRangeEnvironment &From) {
  bool Changed = false;
  for (auto It = Into.begin(); It != Into.end();) {
    auto Other = From.find(It->first);
    if (Other == From.end()) {
      It = Into.erase(It);
      Changed = true;
      continue;
    }
    ConstantRange Joined = It->second.unionWith(Other->second);
    if (Joined != It->second) {
      It->second = std::move(Joined);
      Changed = true;
    }
    ++It;
  }
  return Changed;
}

static void invalidateParentRangeAliases(ParentRangeEnvironment &Environment,
                                         const TargetRegisterInfo &TRI,
                                         Register Def) {
  if (Def.isVirtual()) {
    Environment.erase(Def.id());
    return;
  }
  for (auto It = Environment.begin(); It != Environment.end();) {
    Register Known(static_cast<unsigned>(It->first));
    if (It->first < FirstParentSpillLocation && Known.isPhysical() &&
        TRI.regsOverlap(Known, Def))
      It = Environment.erase(It);
    else
      ++It;
  }
}

static Optional<std::pair<Register, bool>> findEquivalentBranchSource(
    MachineInstr &Branch, Register Condition, const TargetRegisterInfo &TRI) {
  SmallVector<Register, 8> LaterDefs;
  SmallVector<const MachineOperand *, 2> LaterRegMasks;
  SmallPtrSet<MachineBasicBlock *, 8> Visited;
  MachineBasicBlock *MBB = Branch.getParent();
  MachineInstr *StopAt = &Branch;

  while (MBB && Visited.insert(MBB).second) {
    bool BelowStop = StopAt != nullptr;
    for (MachineInstr &MI : llvm::reverse(*MBB)) {
      if (BelowStop) {
        if (&MI == StopAt)
          BelowStop = false;
        continue;
      }

      if (MI.definesRegister(Condition, &TRI)) {
        Register Source;
        bool NeedsCanonicalSignedWord = false;
        if (MI.getOpcode() == TargetOpcode::COPY &&
            MI.getNumOperands() >= 2 && MI.getOperand(1).isReg()) {
          Source = MI.getOperand(1).getReg();
        } else if (MI.getOpcode() == LinxV5::ORI &&
                   MI.getNumOperands() >= 3 && MI.getOperand(1).isReg() &&
                   MI.getOperand(2).isImm() &&
                   MI.getOperand(2).getImm() == 0) {
          Source = MI.getOperand(1).getReg();
        } else if (MI.getOpcode() == LinxV5::OR_SW &&
                   MI.getNumOperands() >= 4 && MI.getOperand(1).isReg() &&
                   MI.getOperand(1).getReg() == LinxV5::R0 &&
                   MI.getOperand(2).isReg() && MI.getOperand(3).isImm() &&
                   MI.getOperand(3).getImm() == 0) {
          Register LHS = MI.getOperand(1).getReg();
          Register RHS = MI.getOperand(2).getReg();
          Source = LHS == LinxV5::R0 ? RHS :
                   RHS == LinxV5::R0 ? LHS : Register();
          NeedsCanonicalSignedWord = true;
        }
        if (!Source)
          return None;
        for (Register Def : LaterDefs)
          if (Def == Source || (Def.isPhysical() && Source.isPhysical() &&
                                TRI.regsOverlap(Def, Source)))
            return None;
        for (const MachineOperand *Mask : LaterRegMasks)
          if (Source.isPhysical() && Mask->clobbersPhysReg(Source))
            return None;
        return std::make_pair(Source, NeedsCanonicalSignedWord);
      }

      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef())
          LaterDefs.push_back(MO.getReg());
        else if (MO.isRegMask())
          LaterRegMasks.push_back(&MO);
      }
    }
    if (BelowStop || MBB->pred_size() != 1)
      return None;
    MBB = *MBB->pred_begin();
    StopAt = nullptr;
  }
  return None;
}

static ConstantRange widenParentRangeAtLoopHeader(
    const ConstantRange &Old, const ConstantRange &Joined,
    ArrayRef<APInt> ComparisonEndpoints) {
  if (Old.contains(Joined))
    return Old;
  if (Old.isFullSet() || Joined.isFullSet() || Old.isWrappedSet() ||
      Joined.isWrappedSet())
    return ConstantRange::getFull(64);

  APInt Lower = Joined.getLower();
  APInt Upper = Joined.getUpper();
  if (Lower != Old.getLower()) {
    bool Found = false;
    APInt Candidate(64, 0);
    for (const APInt &Endpoint : ComparisonEndpoints) {
      if (Endpoint.ule(Lower) && (!Found || Candidate.ult(Endpoint))) {
        Candidate = Endpoint;
        Found = true;
      }
    }
    if (!Found)
      return ConstantRange::getFull(64);
    Lower = Candidate;
  }
  if (Upper != Old.getUpper()) {
    bool Found = false;
    APInt Candidate(64, 0);
    for (const APInt &Endpoint : ComparisonEndpoints) {
      if (Upper.ule(Endpoint) && (!Found || Endpoint.ult(Candidate))) {
        Candidate = Endpoint;
        Found = true;
      }
    }
    if (!Found)
      return ConstantRange::getFull(64);
    Upper = Candidate;
  }

  ConstantRange Widened(Lower, Upper);
  if (!Widened.contains(Joined))
    return ConstantRange::getFull(64);
  return Widened;
}

static ParentPhaseResult analyzeAssembleParentPhasesWithLoopScope(
    MachineFunction &MF, MachineLoopInfo &MLI, const TargetRegisterInfo &TRI,
    Register ParentA, Register ParentB, bool IncludeAncestorLoops) {
  std::set<ParentRangeLocation> RelevantLocations;
  std::map<ParentSpillAccess, ParentRangeLocation> SpillLocations;
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  std::map<ParentRangeLocation, SmallVector<APInt, 4>> ComparisonEndpoints;
  std::map<ParentRangeLocation, SmallVector<ParentRangeLocation, 2>> ScalarDependencies;
  SmallVector<MachineInstr *, 64> RangeTransferInstructions;
  // Every 64-bit value is a legal endpoint, including DenseMap's reserved
  // empty/tombstone keys (-1 and -2).
  std::map<ParentRangeLocation, std::set<uint64_t>> ComparisonEndpointMembership;
  constexpr unsigned MaxPhaseProofWork = 65536;
  unsigned PreprocessWork = 0;
  auto consumePreprocessWork = [&](size_t Work) {
    if (Work > MaxPhaseProofWork - PreprocessWork) {
      LLVM_DEBUG(dbgs() << "B.ASSEMBLE phase preprocessing cap for "
                        << printReg(ParentA) << " / " << printReg(ParentB)
                        << "\n");
      return false;
    }
    PreprocessWork += Work;
    return true;
  };
  auto addEndpoint = [&](ParentRangeLocation Reg, const APInt &Endpoint) {
    if (!ComparisonEndpointMembership[Reg]
             .insert(Endpoint.getZExtValue())
             .second)
      return false;
    ComparisonEndpoints[Reg].push_back(Endpoint);
    return true;
  };
  auto spillLocation = [&](const ParentSpillAccess &Access) {
    auto It = SpillLocations.find(Access);
    if (It != SpillLocations.end())
      return It->second;
    ParentRangeLocation Location =
        FirstParentSpillLocation + SpillLocations.size();
    SpillLocations.emplace(Access, Location);
    return Location;
  };

  // Range facts are needed only to prove termination through CFG cycles that
  // directly carry this parent's events.  Seed preprocessing from each
  // event's innermost loop.  The phase walk below still traverses the full CFG
  // and therefore checks an OPEN parent crossing an enclosing loop, while
  // unrelated enclosing-loop counters and reused physical registers cannot
  // pollute the event loop's comparison endpoints.
  DenseSet<MachineBasicBlock *> RangeProofBlocks;
  for (MachineBasicBlock &MBB : MF) {
    bool HasParentEvent = false;
    for (MachineInstr &MI : MBB) {
      AssembleParentEvent A = getAssembleParentEvent(MI, ParentA);
      AssembleParentEvent B = ParentB ? getAssembleParentEvent(MI, ParentB)
                                      : AssembleParentEvent();
      if (A.Defines || A.References || B.Defines || B.References) {
        HasParentEvent = true;
        break;
      }
    }
    if (!HasParentEvent)
      continue;
    MachineLoop *Loop = MLI.getLoopFor(&MBB);
    if (!Loop)
      continue;
    if (IncludeAncestorLoops)
      while (Loop->getParentLoop())
        Loop = Loop->getParentLoop();
    RangeProofBlocks.insert(Loop->block_begin(), Loop->block_end());
  }
  for (MachineBasicBlock &MBB : MF) {
    if (!RangeProofBlocks.count(&MBB))
      continue;
    for (MachineInstr &MI : MBB) {
      bool IsScalarTransfer =
          MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
          MI.getOperand(1).isReg() &&
          (MI.getOpcode() == TargetOpcode::COPY ||
           MI.getOpcode() == LinxV5::ADDI ||
           MI.getOpcode() == LinxV5::SUBI ||
           MI.getOpcode() == LinxV5::ORI ||
           MI.getOpcode() == LinxV5::OR_SW);
      Optional<ParentSpillAccess> Load =
          MI.getNumOperands() && MI.getOperand(0).isReg()
              ? getParentSpillAccess(MI, MFI, true)
              : None;
      Optional<ParentSpillAccess> Store =
          MI.getNumOperands() && MI.getOperand(0).isReg()
              ? getParentSpillAccess(MI, MFI, false)
              : None;
      bool IsComparison =
          (MI.getOpcode() == LinxV5::SETC_EQI_BR ||
           MI.getOpcode() == LinxV5::SETC_NEI_BR ||
           MI.getOpcode() == LinxV5::SETC_LTI_BR) &&
          MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
          MI.getOperand(1).isImm();

      // Tile-heavy straight-line bodies may contain tens of thousands of
      // instructions that cannot affect scalar range propagation.  Charge
      // only the scalar transfers, private spill accesses, and comparisons
      // that enter this proof.  The unchanged budget still bounds dependency
      // graphs and endpoint propagation rather than rejecting irrelevant Tile
      // work before analysis begins.
      if ((IsScalarTransfer || Load || Store || IsComparison) &&
          !consumePreprocessWork(5))
        return ParentPhaseResult::UnableToProve;
      if (IsScalarTransfer || Load || Store)
        RangeTransferInstructions.push_back(&MI);
      if (MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
          MI.getOperand(1).isReg()) {
        bool Copy = MI.getOpcode() == TargetOpcode::COPY;
        bool Immediate =
            (MI.getOpcode() == LinxV5::ADDI ||
             MI.getOpcode() == LinxV5::SUBI ||
             MI.getOpcode() == LinxV5::ORI) &&
            MI.getNumOperands() >= 3 && MI.getOperand(2).isImm() &&
            (MI.getOpcode() != LinxV5::ORI || MI.getOperand(2).getImm() == 0);
        if (Copy || Immediate)
          ScalarDependencies[MI.getOperand(0).getReg().id()].push_back(
              MI.getOperand(1).getReg().id());
        else if (MI.getOpcode() == LinxV5::OR_SW &&
                 MI.getNumOperands() >= 4 &&
                 MI.getOperand(1).getReg() == LinxV5::R0 &&
                 MI.getOperand(2).isReg() && MI.getOperand(3).isImm() &&
                 MI.getOperand(3).getImm() == 0)
          ScalarDependencies[MI.getOperand(0).getReg().id()].push_back(
              MI.getOperand(2).getReg().id());
      }
      if (MI.getNumOperands() && MI.getOperand(0).isReg()) {
        if (Load)
          ScalarDependencies[MI.getOperand(0).getReg().id()].push_back(
              spillLocation(*Load));
        if (Store)
          ScalarDependencies[spillLocation(*Store)].push_back(
              MI.getOperand(0).getReg().id());
      }
      if (IsComparison) {
        unsigned Reg = MI.getOperand(0).getReg().id();
        RelevantLocations.insert(Reg);
        APInt Endpoint(64, static_cast<uint64_t>(MI.getOperand(1).getImm()));
        addEndpoint(Reg, Endpoint);
        addEndpoint(Reg, Endpoint + 1);
      }
    }
  }

  std::map<std::pair<ParentRangeLocation, ParentRangeLocation>, bool>
      DependencyPathCache;
  auto hasDependencyPath = [&](ParentRangeLocation From,
                               ParentRangeLocation To) -> Optional<bool> {
    auto Key = std::make_pair(From, To);
    auto Cached = DependencyPathCache.find(Key);
    if (Cached != DependencyPathCache.end())
      return Cached->second;
    SmallVector<ParentRangeLocation, 16> Pending{From};
    std::set<ParentRangeLocation> Visited;
    while (!Pending.empty()) {
      if (!consumePreprocessWork(1))
        return None;
      ParentRangeLocation Reg = Pending.pop_back_val();
      if (Reg == To)
        return DependencyPathCache[Key] = true;
      if (!Visited.insert(Reg).second)
        continue;
      auto It = ScalarDependencies.find(Reg);
      if (It == ScalarDependencies.end())
        continue;
      if (!consumePreprocessWork(It->second.size()))
        return None;
      Pending.append(It->second.begin(), It->second.end());
    }
    return DependencyPathCache[Key] = false;
  };

  bool RelevantChanged = true;
  while (RelevantChanged) {
    RelevantChanged = false;
    for (MachineInstr *Transfer : RangeTransferInstructions) {
      MachineInstr &MI = *Transfer;
      if (!consumePreprocessWork(1))
        return ParentPhaseResult::UnableToProve;
      if (MI.getNumOperands() && MI.getOperand(0).isReg()) {
          Optional<ParentRangeLocation> Def, Src;
          if (auto Load = getParentSpillAccess(MI, MFI, true)) {
            Def = MI.getOperand(0).getReg().id();
            Src = spillLocation(*Load);
          } else if (auto Store = getParentSpillAccess(MI, MFI, false)) {
            Def = spillLocation(*Store);
            Src = MI.getOperand(0).getReg().id();
          }
          if (Def && RelevantLocations.count(*Def)) {
            RelevantChanged |= RelevantLocations.insert(*Src).second;
            if (!consumePreprocessWork(ComparisonEndpoints[*Def].size()))
              return ParentPhaseResult::UnableToProve;
            auto Endpoints = ComparisonEndpoints[*Def];
            for (const APInt &Endpoint : Endpoints)
              RelevantChanged |= addEndpoint(*Src, Endpoint);
          }
      }
      if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
          !RelevantLocations.count(MI.getOperand(0).getReg().id()))
        continue;
      unsigned DefReg = MI.getOperand(0).getReg().id();
      if ((MI.getOpcode() == TargetOpcode::COPY ||
             MI.getOpcode() == LinxV5::ORI ||
             MI.getOpcode() == LinxV5::ADDI ||
             MI.getOpcode() == LinxV5::SUBI) &&
            MI.getOperand(1).isReg()) {
          unsigned SrcReg = MI.getOperand(1).getReg().id();
          RelevantChanged |= RelevantLocations.insert(SrcReg).second;
          bool Propagate = MI.getOpcode() == TargetOpcode::COPY;
          APInt Adjustment(64, 0);
          if (MI.getOpcode() == LinxV5::ORI && MI.getNumOperands() >= 3 &&
              MI.getOperand(2).isImm())
            Propagate = MI.getOperand(2).getImm() == 0;
          else if ((MI.getOpcode() == LinxV5::ADDI ||
                    MI.getOpcode() == LinxV5::SUBI) &&
                   MI.getNumOperands() >= 3 && MI.getOperand(2).isImm()) {
            Propagate = true;
            Adjustment =
                APInt(64, static_cast<uint64_t>(MI.getOperand(2).getImm()));
          }
          // A self-update is the recurrence itself.  Its comparison endpoints
          // already describe the loop bound; repeatedly inverting ADDI/SUBI
          // would manufacture an unbounded endpoint sequence.
          if (Propagate && SrcReg != DefReg) {
            if (!Adjustment.isZero()) {
              Optional<bool> Cyclic = hasDependencyPath(SrcReg, DefReg);
              if (!Cyclic)
                return ParentPhaseResult::UnableToProve;
              // Thresholds are widening hints, not reachability facts.
              // Inverting a nonzero-offset cycle creates an unbounded set,
              // including when physical registers are reused by disjoint
              // loops. Drop that propagation conservatively; copy edges
              // still carry the original comparison boundaries.
              if (*Cyclic)
                continue;
            }
            // Charge endpoint propagation before copying or traversing the
            // list: a small instruction cycle can grow this list indefinitely.
            if (!consumePreprocessWork(ComparisonEndpoints[DefReg].size()))
              return ParentPhaseResult::UnableToProve;
            SmallVector<APInt, 4> DefEndpoints =
                ComparisonEndpoints[DefReg];
            for (const APInt &Endpoint : DefEndpoints) {
              APInt SourceEndpoint = Endpoint;
              if (MI.getOpcode() == LinxV5::ADDI)
                SourceEndpoint -= Adjustment;
              else if (MI.getOpcode() == LinxV5::SUBI)
                SourceEndpoint += Adjustment;
              RelevantChanged |= addEndpoint(SrcReg, SourceEndpoint);
            }
          }
      } else if (MI.getOpcode() == LinxV5::OR_SW &&
                 MI.getNumOperands() >= 4 && MI.getOperand(1).isReg() &&
                 MI.getOperand(1).getReg() == LinxV5::R0 &&
                 MI.getOperand(2).isReg() && MI.getOperand(3).isImm() &&
                 MI.getOperand(3).getImm() == 0) {
          Register LHS = MI.getOperand(1).getReg();
          Register RHS = MI.getOperand(2).getReg();
          RelevantChanged |= RelevantLocations.insert(LHS.id()).second;
          RelevantChanged |= RelevantLocations.insert(RHS.id()).second;
          Register Source = LHS == LinxV5::R0 ? RHS :
                            RHS == LinxV5::R0 ? LHS : Register();
          if (Source) {
            // Charge endpoint propagation before copying or traversing the
            // list: a small instruction cycle can grow this list indefinitely.
            if (!consumePreprocessWork(ComparisonEndpoints[DefReg].size()))
              return ParentPhaseResult::UnableToProve;
            SmallVector<APInt, 4> DefEndpoints =
                ComparisonEndpoints[DefReg];
            for (const APInt &Endpoint : DefEndpoints)
              if (Endpoint.trunc(32).sext(64) == Endpoint)
                RelevantChanged |= addEndpoint(Source.id(), Endpoint);
          }
      }
    }
  }

  SmallVector<ParentPhaseStateKey, 32> Worklist;
  std::map<ParentPhaseStateKey, ParentRangeEnvironment> EntryStates;
  ParentPhaseStateKey Entry{&MF.front(), 0};
  EntryStates.emplace(Entry, ParentRangeEnvironment());
  Worklist.push_back(Entry);
  constexpr unsigned MaxPathStates = MaxPhaseProofWork;
  unsigned ProcessedStates = 0;

  while (!Worklist.empty()) {
    ParentPhaseStateKey Key = Worklist.pop_back_val();
    ParentRangeEnvironment Environment = EntryStates.find(Key)->second;
    if (++ProcessedStates > MaxPathStates) {
      LLVM_DEBUG(dbgs() << "B.ASSEMBLE phase state cap for "
                        << printReg(ParentA) << " / " << printReg(ParentB)
                        << ", pending " << Worklist.size() << "\n");
      return ParentPhaseResult::UnableToProve;
    }
    if (Key.Open == 3)
      return ParentPhaseResult::Overlap;

    Optional<unsigned> ConditionalOpcode;
    Register ConditionalReg;
    Register ConditionalEquivalentReg;
    APInt ConditionalValue(64, 0);
    MachineBasicBlock *ConditionalTarget = nullptr;
    for (MachineInstr &MI : *Key.MBB) {
      AssembleParentEvent A = getAssembleParentEvent(MI, ParentA);
      AssembleParentEvent B = ParentB ? getAssembleParentEvent(MI, ParentB)
                                      : AssembleParentEvent();

      // ParentRef reads occur before a destination is published.
      if ((A.References && !(Key.Open & 1u)) ||
          (B.References && !(Key.Open & 2u))) {
        LLVM_DEBUG(dbgs() << "invalid B.ASSEMBLE phase at "
                          << printMBBReference(*Key.MBB)
                          << " open=" << Key.Open << "\n" << MI);
        return ParentPhaseResult::Invalid;
      }
      if (A.References && A.Last)
        Key.Open &= ~1u;
      if (B.References && B.Last)
        Key.Open &= ~2u;
      if (Key.Open == 3)
        return ParentPhaseResult::Overlap;
      if (A.Defines)
        Key.Open = A.Last ? Key.Open & ~1u : Key.Open | 1u;
      if (B.Defines)
        Key.Open = B.Last ? Key.Open & ~2u : Key.Open | 2u;
      if (Key.Open == 3)
        return ParentPhaseResult::Overlap;

      Optional<std::pair<Register, ConstantRange>> KnownDef;
      if (MI.getOpcode() == TargetOpcode::COPY &&
          MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
          MI.getOperand(1).isReg()) {
        KnownDef = std::make_pair(
            MI.getOperand(0).getReg(),
            getParentRange(Environment, MI.getOperand(1).getReg()));
      } else if ((MI.getOpcode() == LinxV5::ORI ||
                  MI.getOpcode() == LinxV5::ADDI ||
                  MI.getOpcode() == LinxV5::SUBI) &&
                 MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
                 MI.getOperand(1).isReg() && MI.getOperand(2).isImm()) {
        ConstantRange Src =
            getParentRange(Environment, MI.getOperand(1).getReg());
        ConstantRange Imm(
            APInt(64, static_cast<uint64_t>(MI.getOperand(2).getImm())));
        ConstantRange Value =
            MI.getOpcode() == LinxV5::ORI
                ? Src.binaryOr(Imm)
                : MI.getOpcode() == LinxV5::ADDI ? Src.add(Imm)
                                                 : Src.sub(Imm);
        KnownDef = std::make_pair(MI.getOperand(0).getReg(), Value);
      } else if (MI.getOpcode() == LinxV5::OR_SW &&
                 MI.getNumOperands() >= 4 && MI.getOperand(0).isReg() &&
                 MI.getOperand(1).isReg() &&
                 MI.getOperand(1).getReg() == LinxV5::R0 &&
                 MI.getOperand(2).isReg() && MI.getOperand(3).isImm() &&
                 MI.getOperand(3).getImm() == 0) {
        ConstantRange RHS =
            getParentRange(Environment, MI.getOperand(2).getReg());
        ConstantRange Value = RHS.truncate(32).signExtend(64);
        KnownDef = std::make_pair(MI.getOperand(0).getReg(), Value);
      }

      if (MI.getNumOperands() && MI.getOperand(0).isReg())
        if (auto Load = getParentSpillAccess(MI, MFI, true))
          KnownDef = std::make_pair(MI.getOperand(0).getReg(),
              getParentLocationRange(Environment, spillLocation(*Load)));

      Optional<std::pair<ParentRangeLocation, ConstantRange>> StoredRange;
      if (MI.getNumOperands() && MI.getOperand(0).isReg())
        if (auto Store = getParentSpillAccess(MI, MFI, false))
          StoredRange = std::make_pair(spillLocation(*Store),
              getParentRange(Environment, MI.getOperand(0).getReg()));
      // A partial or unmodeled write into a tracked private spill destroys its
      // entire value. Other memory cannot alias a non-address-taken spill slot.
      for (const MachineMemOperand *MMO : MI.memoperands()) {
        if (!MMO->isStore())
          continue;
        const auto *PSV =
            dyn_cast_or_null<FixedStackPseudoSourceValue>(MMO->getPseudoValue());
        if (!PSV)
          continue;
        uint64_t Size = MMO->getSize();
        int64_t WriteEnd = 0;
        bool UnknownRange =
            Size > uint64_t(INT64_MAX) ||
            AddOverflow(MMO->getOffset(), static_cast<int64_t>(Size), WriteEnd);
        int FI = PSV->getFrameIndex();
        // Out-of-object metadata cannot establish separation from another
        // frame object. Forget all spill facts rather than assume an FI fence.
        if (UnknownRange || MFI.isDeadObjectIndex(FI) ||
            MMO->getOffset() < 0 || WriteEnd > MFI.getObjectSize(FI)) {
          for (auto It = Environment.lower_bound(FirstParentSpillLocation);
               It != Environment.end();)
            It = Environment.erase(It);
          continue;
        }
        for (const auto &Spill : SpillLocations) {
          if (Spill.first.FI != FI)
            continue;
          int64_t SpillEnd;
          if (UnknownRange ||
              AddOverflow(Spill.first.Offset, int64_t(8), SpillEnd) ||
              (MMO->getOffset() < SpillEnd && Spill.first.Offset < WriteEnd))
            Environment.erase(Spill.second);
        }
      }
      if (StoredRange && RelevantLocations.count(StoredRange->first))
        setParentLocationRange(Environment, StoredRange->first,
                               StoredRange->second);
      // Do not rely on callees preserving frame contents, even for private
      // spill objects. Register/regmask invalidation below remains separate.
      if (MI.isCall())
        for (auto It = Environment.lower_bound(FirstParentSpillLocation);
             It != Environment.end();)
          It = Environment.erase(It);

      if ((MI.getOpcode() == LinxV5::SETC_EQI_BR ||
           MI.getOpcode() == LinxV5::SETC_NEI_BR ||
           MI.getOpcode() == LinxV5::SETC_LTI_BR) &&
          MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
          MI.getOperand(1).isImm() && MI.getOperand(2).isMBB()) {
        ConditionalOpcode = MI.getOpcode();
        ConditionalReg = MI.getOperand(0).getReg();
        Optional<std::pair<Register, bool>> Equivalent =
            findEquivalentBranchSource(MI, ConditionalReg, TRI);
        if (Equivalent) {
          Register Source = Equivalent->first;
          ConstantRange SourceRange = getParentRange(Environment, Source);
          bool CanonicalSignedWord =
              SourceRange.isAllNonNegative() &&
              SourceRange.getUnsignedMax().ult(APInt(64, uint64_t(1) << 31));
          if (!Equivalent->second || CanonicalSignedWord)
            ConditionalEquivalentReg = Source;
        }
        ConditionalValue =
            APInt(64, static_cast<uint64_t>(MI.getOperand(1).getImm()));
        ConditionalTarget = MI.getOperand(2).getMBB();
      }

      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef())
          invalidateParentRangeAliases(Environment, TRI, MO.getReg());
        else if (MO.isRegMask())
          for (auto It = Environment.begin(); It != Environment.end();) {
            Register Known(static_cast<unsigned>(It->first));
            if (It->first < FirstParentSpillLocation && Known.isPhysical() &&
                MO.clobbersPhysReg(Known))
              It = Environment.erase(It);
            else
              ++It;
          }
      }
      if (KnownDef && RelevantLocations.count(KnownDef->first.id()))
        setParentRange(Environment, KnownDef->first, KnownDef->second);
    }

    for (MachineBasicBlock *Succ : Key.MBB->successors()) {
      ParentRangeEnvironment SuccessorEnvironment = Environment;
      if (ConditionalTarget && ConditionalOpcode) {
        bool TrueEdge = Succ == ConditionalTarget;
        ConstantRange Current =
            getParentRange(SuccessorEnvironment, ConditionalReg);
        ConstantRange Singleton(ConditionalValue);
        ConstantRange Allowed = Singleton;
        if (*ConditionalOpcode == LinxV5::SETC_LTI_BR) {
          CmpInst::Predicate Predicate =
              TrueEdge ? CmpInst::ICMP_SLT : CmpInst::ICMP_SGE;
          Allowed =
              ConstantRange::makeExactICmpRegion(Predicate, ConditionalValue);
        } else {
          bool Equal = (*ConditionalOpcode == LinxV5::SETC_EQI_BR) == TrueEdge;
          Allowed = Equal ? Singleton : Singleton.inverse();
        }
        Optional<ConstantRange> Narrowed =
            Current.exactIntersectWith(Allowed);
        if (Narrowed && Narrowed->isEmptySet())
          continue;
        // Removing a point from the middle creates a hole.  Keep the original
        // over-approximation when ConstantRange cannot express it exactly.
        if (Narrowed)
          setParentRange(SuccessorEnvironment, ConditionalReg, *Narrowed);
        if (Narrowed && ConditionalEquivalentReg) {
          ConstantRange EquivalentCurrent = getParentRange(
              SuccessorEnvironment, ConditionalEquivalentReg);
          Optional<ConstantRange> EquivalentNarrowed =
              EquivalentCurrent.exactIntersectWith(Allowed);
          if (EquivalentNarrowed && EquivalentNarrowed->isEmptySet())
            continue;
          if (EquivalentNarrowed)
            setParentRange(SuccessorEnvironment, ConditionalEquivalentReg,
                           *EquivalentNarrowed);
        }
      }

      ParentPhaseStateKey Next{Succ, Key.Open};
      auto Existing = EntryStates.find(Next);
      if (Existing == EntryStates.end()) {
        EntryStates.emplace(Next, std::move(SuccessorEnvironment));
        Worklist.push_back(Next);
        continue;
      }

      ParentRangeEnvironment Joined = Existing->second;
      if (!joinParentRanges(Joined, SuccessorEnvironment))
        continue;
      MachineLoop *Loop = MLI.getLoopFor(Succ);
      if (Loop && Loop->getHeader() == Succ) {
        for (auto It = Joined.begin(); It != Joined.end();) {
          auto Old = Existing->second.find(It->first);
          if (Old == Existing->second.end()) {
            It = Joined.erase(It);
            continue;
          }
          It->second = widenParentRangeAtLoopHeader(
              Old->second, It->second, ComparisonEndpoints[It->first]);
          if (It->second.isFullSet())
            It = Joined.erase(It);
          else
            ++It;
        }
      }
      if (Joined == Existing->second)
        continue;
      Existing->second = std::move(Joined);
      Worklist.push_back(Next);
    }
  }
  return ParentPhaseResult::Disjoint;
}

static ParentPhaseResult analyzeAssembleParentPhases(
    MachineFunction &MF, MachineLoopInfo &MLI, const TargetRegisterInfo &TRI,
    Register ParentA, Register ParentB = Register()) {
  ParentPhaseResult Local = analyzeAssembleParentPhasesWithLoopScope(
      MF, MLI, TRI, ParentA, ParentB, /*IncludeAncestorLoops=*/false);
  if (Local == ParentPhaseResult::Disjoint)
    return Local;

  // Omitting enclosing-loop facts is conservative: it can leave infeasible
  // paths in the phase walk, but cannot manufacture a proof of disjointness.
  // Retry a non-proof with every enclosing event loop before diagnosing the
  // parent.  This preserves outer-loop and preheader-dependent legality while
  // keeping the common self-contained inner loop independent of sibling-loop
  // physical-register reuse.
  return analyzeAssembleParentPhasesWithLoopScope(
      MF, MLI, TRI, ParentA, ParentB, /*IncludeAncestorLoops=*/true);
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
        // RegisterCoalescer may turn a Tile value crossing a CFG join into a
        // non-SSA virtual register with multiple definitions. Clockhands
        // assignment relies on a single defining value; leave such registers
        // to the standard register allocator instead of replacing their
        // register class here.
        if (!MRI->hasOneDef(Reg))
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

SmallVector<Register>
LinxV5ClockhandsColoring::findOpenAssembleParents(MachineFunction &MF) {
  SmallVector<Register> Parents;
  DenseSet<Register> Seen;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!isAssemble(MI))
        continue;

      Optional<bool> Init =
          LinxV5::getAssembleControlBit(MI, /*InitIdx=*/0);
      Optional<bool> Last =
          LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
      if (!Init || !Last)
        report_fatal_error(
            "B.ASSEMBLE Init/Last controls must be immediate operands");

      SmallVector<MachineOperand *, 2> Defs = getDefOperands(&MI);
      Register ParentRef = LinxV5::getAssembleParentRef(MI);
      if (ParentRef != LinxV5::NoRegister &&
          (*Init || !Defs.empty()))
        report_fatal_error(
            "B.ASSEMBLE continuation must be destination-free with Init=0");

      for (MachineOperand *Def : Defs) {
        Register Reg = Def->getReg();
        if (!Reg.isVirtual() ||
            MRI->getRegClass(Reg) != &LinxV5::Tile_ABSRegClass ||
            Seen.count(Reg))
          continue;
        if (!*Init)
          report_fatal_error("B.ASSEMBLE parent definition requires Init=1");
        if (*Last)
          continue;

        // INIT_LAST has no continuation and needs no reserved hand.  An open
        // parent is the Tile result of an INIT writer that is subsequently
        // consumed as ParentRef by a destination-free B.ASSEMBLE
        // continuation.  A completed tile used later as another assembly's
        // numeric writer fragment is not a parent.
        bool HasContinuation = llvm::any_of(
            MRI->use_instructions(Reg), [&](const MachineInstr &UseMI) {
              if (&UseMI == &MI)
                return false;
              return LinxV5::getAssembleParentRef(UseMI) == Reg;
            });
        if (HasContinuation) {
          Seen.insert(Reg);
          Parents.push_back(Reg);
        }
      }
    }
  }
  const TargetRegisterInfo &TRI = *MRI->getTargetRegisterInfo();
  for (Register Parent : Parents) {
    ParentPhaseResult Result =
        analyzeAssembleParentPhases(MF, *MLI, TRI, Parent);
    if (Result == ParentPhaseResult::Invalid)
      report_fatal_error(
          "B.ASSEMBLE continuation may execute without an OPEN parent");
    if (Result == ParentPhaseResult::UnableToProve)
      report_fatal_error(
          "unable to prove bounded B.ASSEMBLE parent phase behavior");
  }
  return Parents;
}

static SmallVector<Register>
findLoopInvariantN8Tiles(MachineFunction &MF, MachineLoopInfo &MLI) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<Register> Tiles;
  for (unsigned I = 0, E = MRI.getNumVirtRegs(); I != E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (MRI.getRegClassOrNull(Reg) != &LinxV5::Tile_ABSRegClass ||
        !MRI.hasOneDef(Reg))
      continue;
    MachineInstr *Def = MRI.getOneDef(Reg)->getParent();
    Optional<LinxV5::TileDescriptor> Descriptor =
        LinxV5::tryGetTileRegDescriptor(
            *Def->getParent(), std::next(Def->getIterator()), Reg);
    if (!Descriptor ||
        LinxV5Op::tileDataLayoutCubeLayout(Descriptor->Layout) !=
            LinxV5Op::CubeLayoutClass::N8)
      continue;
    bool LoopInvariant = llvm::any_of(
        MRI.use_instructions(Reg), [&](const MachineInstr &Use) {
          MachineLoop *Loop = MLI.getLoopFor(Use.getParent());
          return Loop && !Loop->contains(Def->getParent());
        });
    if (LoopInvariant)
      Tiles.push_back(Reg);
  }
  return Tiles;
}

void LinxV5ClockhandsColoring::assignTileReg(MachineFunction &MF) {
  SmallVector<Register> OpenParents = findOpenAssembleParents(MF);
  SmallVector<Register> LoopInvariantN8 =
      findLoopInvariantN8Tiles(MF, *MLI);
  if (!EnableTileClockHand && OpenParents.empty() &&
      LoopInvariantN8.empty()) {
    assignMultiOutput(MF);
    return;
  }
  LLVM_DEBUG(dbgs() << "\nAssignTileReg Start\n");
  DenseSet<const TargetRegisterClass *> OriRCs = {&LinxV5::Tile_ABSRegClass};

  SmallVector<const TargetRegisterClass *> CurRCs = {
      &LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
      &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass};

  if (!LoopInvariantN8.empty()) {
    for (Register Reg : LoopInvariantN8)
      assign(Reg, &LinxV5::Tile_NRRegClass);
    llvm::erase_if(CurRCs, [](const TargetRegisterClass *RC) {
      return RC == &LinxV5::Tile_NRRegClass;
    });
  }

  if (!OpenParents.empty()) {
    // Keep one hand available for writer temporaries.  Parent values with
    // nonoverlapping architectural OPEN phases may reuse a reserved hand even
    // when the completed tile remains ordinarily live.  Full LiveIntervals
    // still govern allocation inside each selected hand.
    ArrayRef<const TargetRegisterClass *> ParentRCs(CurRCs.data(), 3);
    const unsigned ParentCount = OpenParents.size();
    SmallVector<SmallVector<uint8_t, 8>, 8> Interferes(
        ParentCount, SmallVector<uint8_t, 8>(ParentCount, 0));
    const TargetRegisterInfo &TRI = *MRI->getTargetRegisterInfo();
    for (unsigned I = 0; I < ParentCount; ++I) {
      for (unsigned J = I + 1; J < ParentCount; ++J) {
        ParentPhaseResult Result = analyzeAssembleParentPhases(
            MF, *MLI, TRI, OpenParents[I], OpenParents[J]);
        if (Result == ParentPhaseResult::UnableToProve)
          report_fatal_error(
              "unable to prove bounded B.ASSEMBLE parent phase behavior");
        if (Result == ParentPhaseResult::Invalid)
          report_fatal_error(
              "B.ASSEMBLE continuation may execute without an OPEN parent");
        if (Result == ParentPhaseResult::Overlap) {
          Interferes[I][J] = Interferes[J][I] = 1;
          LLVM_DEBUG(dbgs() << "OPEN parent conflict "
                            << printReg(OpenParents[I]) << " / "
                            << printReg(OpenParents[J]) << "\n");
        }
      }
    }

    SmallVector<int, 8> Colors(ParentCount, -1);
    std::function<bool(unsigned)> ColorParents = [&](unsigned Colored) {
      if (Colored == ParentCount)
        return true;

      int Best = -1;
      unsigned BestSaturation = 0;
      unsigned BestDegree = 0;
      for (unsigned I = 0; I < ParentCount; ++I) {
        if (Colors[I] != -1)
          continue;
        bool NeighborColors[3] = {false, false, false};
        unsigned Degree = 0;
        for (unsigned J = 0; J < ParentCount; ++J) {
          if (!Interferes[I][J])
            continue;
          ++Degree;
          if (Colors[J] != -1)
            NeighborColors[Colors[J]] = true;
        }
        unsigned Saturation = NeighborColors[0] + NeighborColors[1] +
                              NeighborColors[2];
        if (Best == -1 || Saturation > BestSaturation ||
            (Saturation == BestSaturation && Degree > BestDegree)) {
          Best = I;
          BestSaturation = Saturation;
          BestDegree = Degree;
        }
      }

      constexpr int ColorPreference[3] = {0, 2, 1};
      for (int Attempt = 0; Attempt != 3; ++Attempt) {
        int Color = ColorPreference[Attempt];
        bool Conflict = false;
        for (unsigned J = 0; J < ParentCount; ++J)
          Conflict |= Interferes[Best][J] && Colors[J] == Color;
        if (Conflict)
          continue;
        Colors[Best] = Color;
        if (ColorParents(Colored + 1))
          return true;
        Colors[Best] = -1;
      }
      return false;
    };
    if (!ColorParents(0))
      report_fatal_error(
          "more than three overlapping OPEN B.ASSEMBLE parents leave no "
          "Local tile hand for writer temporaries");

    DenseSet<const TargetRegisterClass *> UsedParentRCs;
    for (unsigned I = 0; I < ParentCount; ++I) {
      const TargetRegisterClass *RC = ParentRCs[Colors[I]];
      UsedParentRCs.insert(RC);
      assign(OpenParents[I], RC);
    }
    llvm::erase_if(CurRCs, [&](const TargetRegisterClass *RC) {
      return UsedParentRCs.count(RC);
    });
  }

  if (CurRCs.empty())
    report_fatal_error(
        "Local tile hand reservations leave no writer hand");

  const TargetRegisterClass *LoopRC = CurRCs.back();
  const TargetRegisterClass *AcrossRC =
      CurRCs.size() > 1 ? CurRCs[CurRCs.size() - 2] : CurRCs.back();
  bool LoopAssignRes = assignLoopInvar(MF, OriRCs, LoopRC);
  assignAccrosChain(MF, OriRCs, LoopAssignRes ? AcrossRC : LoopRC);
  for (auto Chain : Chains) {
    ColoringInChain CIC(Chain, OriRCs, CurRCs, MRI, LIS, VRM);
    CIC.assign();
  }
  // Register coalescing can leave ordinary Tile_ABS values with multiple
  // definitions.  ColoringInChain deliberately skips those non-SSA values,
  // but standard RA must not place them in a hand reserved by an OPEN parent.
  // N is excluded from parent coloring above and remains the dedicated writer
  // hand for every such residual value.
  if (!OpenParents.empty() || !LoopInvariantN8.empty()) {
    assert(!CurRCs.empty() && "OPEN parents must leave a writer hand");
    const TargetRegisterClass *WriterRC = CurRCs.back();
    for (unsigned I = 0, E = MRI->getNumVirtRegs(); I != E; ++I) {
      Register Reg = Register::index2VirtReg(I);
      if (MRI->getRegClassOrNull(Reg) == &LinxV5::Tile_ABSRegClass)
        MRI->setRegClass(Reg, WriterRC);
    }
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
