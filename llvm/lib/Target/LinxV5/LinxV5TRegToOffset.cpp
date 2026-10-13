//===-- LinxV5TRegToOffsetOpt.cpp - Rewrite Treg to offset -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass is to convert MIR from Register format to Offset format. We do this
// in two steps. Step 1 inserts copy to make sure any use instr is no more than
// 8 instrs away from its def instr. Step 2 changes register to offset.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/MathExtras.h"
#include <set>

#define DEBUG_TYPE "linxv5-treg-to-offset"
#define LINX_TREG_TO_OFFSET_NAME "LinxV5 Treg to Offset"

using namespace llvm;

STATISTIC(NumCopy, "number copys inserted");
STATISTIC(TileLongCopy, "Number jcore-tile long live range copys");
STATISTIC(TileCFGCopy, "Number jcore-tile control flow copys");
STATISTIC(NumSyncConflictRC,
          "Number of regclass liveout sync conflicts (offset rewrite skipped)");

static cl::opt<bool> EnableGlobalDataSync(
    "linxv5-enable-global-sync",
    cl::desc("Enable Global Data sync for SIMT Clock-Hand."), cl::init(true),
    cl::Hidden);

static cl::opt<bool>
    EnableReg2Offset("linxv5-enable-reg-to-offset",
                     cl::desc("Enable absolute register to offset register"),
                     cl::init(true), cl::Hidden);

namespace llvm {

struct LinxRegOp {
  unsigned Reg;
  unsigned Size;
  Optional<LinxV5::TileDescriptor> Descriptor;

  LinxRegOp()
      : Reg(MCRegister::NoRegister),
        Size(LinxV5::SIMTRegSize::SIMT_REG_SIZE_D) {}
  LinxRegOp(unsigned reg, unsigned size) : Reg(reg), Size(size) {}
  bool operator==(const LinxRegOp &other) const {
    // if we want check size, do this work at other pass.
    return Reg == other.Reg;
  }
};

struct TRLiveRange {
  MachineInstr *MI;
  MachineInstr *LastUse;
  MachineOperand *DefMO;
  unsigned DefIdx;
  bool IsLiveout;
  bool IsAssembleParent;
  TRLiveRange(MachineInstr *I)
      : MI(I), LastUse(nullptr), DefMO(nullptr), DefIdx(-1u), IsLiveout(false),
        IsAssembleParent(false) {
  }
};

struct TRCopyRange {
  LinxRegOp RegOp;
  MachineInstr *LastUse;
  bool IsLiveout;
  bool IsAssembleParent;
  size_t Idx;
  TRCopyRange()
      : LastUse(nullptr), IsLiveout(false), IsAssembleParent(false), Idx(0) {}
  TRCopyRange(LinxRegOp &RO, bool liveout)
      : RegOp(RO), LastUse(nullptr), IsLiveout(liveout),
        IsAssembleParent(false), Idx(0) {}
};

struct SlotStatus {
  SmallVector<TRCopyRange> TRCopyRanges;
  SmallVector<LinxRegOp> LiveOuts;
  size_t HeadIdx, TailIdx;

  SlotStatus() : HeadIdx(0), TailIdx(0) {}
  SlotStatus(SmallVector<TRCopyRange> trcopyranges,
             SmallVector<LinxRegOp> liveouts, size_t head, size_t tail)
      : TRCopyRanges(trcopyranges), LiveOuts(liveouts), HeadIdx(head),
        TailIdx(tail) {}
};

struct RCCompare {
  bool operator()(const TargetRegisterClass *a,
                  const TargetRegisterClass *b) const {
    return a->getID() < b->getID();
  }
};

class LinxV5TRegToOffsetOpt : public MachineFunctionPass {
public:
  const LinxV5InstrInfo *TII;
  MachineRegisterInfo *MRI;
  const TargetRegisterInfo *TRI;
  static char ID;
  LinxV5TRegToOffsetOpt() : MachineFunctionPass(ID) {
    initializeLinxV5TRegToOffsetOptPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return LINX_TREG_TO_OFFSET_NAME; }

private:
  SmallVector<MachineBasicBlock *> TBDoneMBBs;
  SmallVector<MachineBasicBlock *> DoneMBBs;
  SmallVector<TRCopyRange> LiveSlots;
  std::map<const TargetRegisterClass *, SmallVector<Register, 2>, RCCompare>
      RCsInfo;
  DenseMap<MachineBasicBlock *, DenseSet<MachineBasicBlock *>> GlobalSyncupMap;
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *, SlotStatus>>
      GlobalSlotsInfo;
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *,
                    std::pair<DenseMap<Register, unsigned int>, int>>>
      RewriteTRDefInfo;
  // Issue #60: register classes whose done-successor liveout queues cannot be
  // tail-aligned unified (e.g. an unrolled if-select where each arm defines a
  // different tile). Phase-1 copy insertion stays correct for them, but the
  // Phase-2 offset rewrite assumes every predecessor leaves the window in the
  // same state, so those classes keep absolute registers.
  DenseSet<const TargetRegisterClass *> SyncConflictRCs;
  // Clockhands reserves each RC/hand that carries an OPEN B.ASSEMBLE parent.
  // Keep that hand's queue order exact through Phase 1: CFG rolling would
  // require copying the in-progress parent or changing its ParentRef distance.
  DenseSet<const TargetRegisterClass *> PinnedAssembleParentRCs;
  // Exact post-RA INIT definitions for OPEN B.ASSEMBLE parents.  Phase 1 may
  // insert copies, so this map is populated only after copy insertion and
  // after validating that the reserved hand contains no other allocation.
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      AssembleParentUseDistances;

  bool canSkipMI(MachineInstr &MI);
  bool isDistanceReg(Register Reg);

  void releaseMemory() override {
    TBDoneMBBs.clear();
    DoneMBBs.clear();
    LiveSlots.clear();
    RCsInfo.clear();
    GlobalSyncupMap.clear();
    GlobalSlotsInfo.clear();
    RewriteTRDefInfo.clear();
    SyncConflictRCs.clear();
    PinnedAssembleParentRCs.clear();
    AssembleParentUseDistances.clear();
  }

  // Phase-1
  bool isInSyncGroup(MachineBasicBlock *MBB);
  void updateGlobalSyncup(MachineFunction &MF);
  MachineBasicBlock *getNextBlock();
  SmallVector<LinxRegOp> getLiveoutLimits(MachineBasicBlock &MBB,
                                          const TargetRegisterClass *RC);
  SlotStatus *getInitStatus(MachineBasicBlock &MBB,
                            DenseMap<Register, MachineInstr *> LiveInsLastUse,
                            DenseSet<Register> RestLiveouts,
                            const TargetRegisterClass *RC);
  void insertCopy(MachineBasicBlock &MBB);

  void legalityCheck(MachineFunction &MF);
  void solveAssembleParentDataflow(MachineFunction &MF);

  // Phase-2
  void rewriteMBB(MachineBasicBlock &MBB);
  void rewriteMBB(MachineBasicBlock &MBB,
                  DenseMap<Register, unsigned int> &TRegIdx, unsigned &CurIdx,
                  const TargetRegisterClass *RC, Register Out,
                  Register OffsetBase);
};
} // namespace llvm
char LinxV5TRegToOffsetOpt::ID = 0;

INITIALIZE_PASS(LinxV5TRegToOffsetOpt, DEBUG_TYPE, LINX_TREG_TO_OFFSET_NAME,
                false, false)

FunctionPass *llvm::createLinxV5TRegToOffsetOptPass() {
  return new LinxV5TRegToOffsetOpt();
}

static bool isTileReg(const TargetRegisterClass *RC) {
  if (RC == &LinxV5::Tile_TRRegClass || RC == &LinxV5::Tile_URRegClass ||
      RC == &LinxV5::Tile_MRRegClass || RC == &LinxV5::Tile_NRRegClass ||
      RC == &LinxV5::TILE_ABS_ACCRegClass)
    return true;
  return false;
}

static bool isScalarReg(const TargetRegisterClass *RC) {
  if (RC == &LinxV5::LTRRegClass || RC == &LinxV5::LURRegClass)
    return true;
  if (RC == &LinxV5::SIMT_VTRRegClass || RC == &LinxV5::SIMT_VURRegClass ||
      RC == &LinxV5::SIMT_VMRRegClass || RC == &LinxV5::SIMT_VNRRegClass ||
      RC == &LinxV5::Tile_TRRegClass || RC == &LinxV5::Tile_URRegClass ||
      RC == &LinxV5::Tile_MRRegClass || RC == &LinxV5::Tile_NRRegClass ||
      RC == &LinxV5::TILE_ABS_ACCRegClass)
    return false;
  assert(0 && "Reg Class is not offset register!");
}

static bool isRC(Register Reg, const TargetRegisterClass *RC,
                 const MachineRegisterInfo *MRI) {
  return Reg.isVirtual() ? MRI->getRegClass(Reg) == RC : RC->contains(Reg);
}

static DenseSet<Register>
collectSuccessorLiveInsForRC(MachineBasicBlock &MBB,
                             const TargetRegisterClass *RC,
                             const MachineRegisterInfo *MRI) {
  DenseSet<Register> LiveIns;
  for (MachineBasicBlock *Succ : MBB.successors()) {
    for (const auto &LiveIn : Succ->liveins()) {
      Register Reg = LiveIn.PhysReg;
      if (isRC(Reg, RC, MRI))
        LiveIns.insert(Reg);
    }
  }
  return LiveIns;
}

static void initSlot(TRCopyRange *Slot) {
  Slot->RegOp = LinxRegOp();
  Slot->IsLiveout = false;
  Slot->IsAssembleParent = false;
  Slot->LastUse = nullptr;
}

static bool isAssembleParentDef(const TRLiveRange &LR) {
  if (!LR.MI || !LR.MI->isInlineAsm() || !LR.DefMO)
    return false;
  if (!StringRef(LR.MI->getOperand(InlineAsm::MIOp_AsmString)
                     .getSymbolName())
           .contains("B.ASSEMBLE"))
    return false;
  // Only an OPEN generation pins the parent identity. When the defining
  // bundle already carries Last=1 (a single-fragment TileArray slot), the
  // generation opens and closes in this instruction and the tile is
  // ordinary data immediately after. Templates whose control operands
  // cannot be resolved positionally keep the conservative open marking.
  Optional<bool> Last = LinxV5::getAssembleControlBit(*LR.MI, /*LastIdx=*/1);
  return !Last || !*Last;
}

static unsigned getTSlotOccupation(MachineInstr &MI,
                                   const TargetRegisterClass *RC) {
  // PseudoEmptyTile expands to a zero-mask NoPE bundle.  Its dead destination
  // is only a compiler bookkeeping operand; the instruction neither
  // allocates nor publishes a Local tile queue entry.
  if (MI.getOpcode() == LinxV5::PseudoEmptyTile)
    return 0;

  unsigned Occ = 0;
  for (auto &MO : MI.operands()) {
    if (!MO.isReg() || MO.isUse())
      continue;
    Register Reg = MO.getReg();
    assert(Reg.isPhysical() && "Unexpected virtual Reg");
    if (RC->contains(Reg)) {
      Occ++;
    }
  }
  return Occ;
}

namespace {
class SlotCalc {
public:
  MachineBasicBlock *MBB;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  const TargetRegisterClass *RC;
  Register ReserveReg;
  bool IsScalarRC;
  bool IsTileRC;
  size_t HeadIdx, TailIdx, PreIdx;
  SmallVector<TRCopyRange> LiveSlots;
  uint64_t NrCopy;
  unsigned TRNumber;
  SlotCalc(MachineBasicBlock *mbb, const TargetInstrInfo *tii,
           const TargetRegisterClass *rc, bool iScalarRC, bool isTileRC)
      : MBB(mbb), TII(tii), RC(rc), IsScalarRC(iScalarRC), IsTileRC(isTileRC),
        HeadIdx(0), TailIdx(0), PreIdx(0), NrCopy(0),
        TRNumber(RC->getNumRegs()) {
    TRI = MBB->getParent()->getSubtarget().getRegisterInfo();
  }
  void init(SlotStatus *SS);
  SmallVector<TRCopyRange *>
  allocNewSlot(SmallVectorImpl<const TRLiveRange *> &LRs);
  SmallVector<LinxRegOp> getLiveouts(bool EnableVerify);
  void BuildLiveoutAndEraseFrom(TRCopyRange &Slot, LinxRegOp RegOp,
                                SmallVector<LinxRegOp> &CurLiveouts);
  void rollingLiveouts(SmallVector<LinxRegOp> needSyncTo);
  void insertCopys(const SmallVector<TRLiveRange> &OriginalInstrs,
                   Register ReservedReg, SlotStatus *SS, bool IsInSyncGroup,
                   SmallVector<LinxRegOp> needSyncTo, SlotStatus &ResultSS);
  bool BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp);
  bool SlotCopy(MachineBasicBlock::iterator Before, TRCopyRange &Slot);
  void PreAlloc(TRCopyRange Slot);
  void printLiveSlots();
  unsigned tailsi() { return TailIdx % TRNumber; }
  unsigned headsi() { return HeadIdx % TRNumber; }
  unsigned firstsi() { return (HeadIdx - 1) % TRNumber; }
  // WeakAlloc SlotIdx
  unsigned presi() { return PreIdx % TRNumber; }
  TRCopyRange &head() { return LiveSlots[headsi()]; }
  TRCopyRange &first() { return LiveSlots[firstsi()]; }
  TRCopyRange &tail() { return LiveSlots[tailsi()]; }
  TRCopyRange &pre() { return LiveSlots[presi()]; }
  void expire() { ++TailIdx; }
  void prealloc() { ++PreIdx; }
  void advance() {
    ++HeadIdx;
    if (PreIdx < HeadIdx)
      PreIdx = HeadIdx;
  }
  bool has_pre() { return PreIdx > HeadIdx; }
  void shift() {
    ++TailIdx;
    ++HeadIdx;
    ++PreIdx;
  }
  unsigned free() { return TRNumber - (HeadIdx - TailIdx); }
  bool isKill(TRCopyRange &Slot, const MachineInstr *RR);
  bool isIdle(TRCopyRange &Slot, const MachineInstr *RR);
  unsigned getRegSize(const TRLiveRange &RR);
  MachineBasicBlock::iterator
  findTailCopyInsertPosition(MachineBasicBlock &MBB) {
    if (LinxV5::Tile_ABSRegClass.hasSubClassEq(RC)) {
      // find Last tile op
      auto I = MBB.end(), B = MBB.begin();
      for (; I != B; --I) {
        MachineInstr &MI = *std::prev(I);
        if (LinxV5::isTileOp(MI))
          break;
      }
      return I;
    } else {
      return MBB.getFirstTerminator();
    }
  }
};
} // namespace

void SlotCalc::init(SlotStatus *SS) {
  if (SS) {
    LLVM_DEBUG(dbgs() << "init with pred tail " << SS->TailIdx << " head "
                      << SS->HeadIdx << "\n");
    LiveSlots = SS->TRCopyRanges;
    HeadIdx = SS->HeadIdx;
    PreIdx = HeadIdx;
    TailIdx = SS->TailIdx;
  } else {
    LLVM_DEBUG(dbgs() << "init clear\n");
    LiveSlots.clear();
    LiveSlots.resize(TRNumber);
    HeadIdx = TRNumber;
    PreIdx = HeadIdx;
    TailIdx = 0;
  }
}

bool SlotCalc::isKill(TRCopyRange &Slot, const MachineInstr *MI) {
  if (Slot.IsLiveout)
    return false;
  if (Slot.LastUse == MI)
    return true;
  return false;
}

bool SlotCalc::isIdle(TRCopyRange &Slot, const MachineInstr *MI) {
  if (Slot.IsLiveout)
    return false;
  if (Slot.LastUse == nullptr)
    return true;
  if (Slot.LastUse == MI)
    return true;
  return false;
}

bool SlotCalc::BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp) {
  Register TR = RegOp.Reg;

  if (Before != MBB->begin()) {
    while (std::prev(Before)->getOpcode() == LinxV5::PseudoLABEL ||
           std::prev(Before)->isLabel())
      --Before;
  }

  if (TR == LinxV5::NoRegister) {
    if (IsTileRC) {
      // A queue-alignment hole must consume a real architectural Tile entry.
      // PseudoEmptyTile is a zero-mask NoPE and cannot serve this purpose.
      // Duplicate any closed live value into the reserved dead register; the
      // full descriptor keeps the filler a legal identity transport while
      // its payload remains semantically unused.
      const TRCopyRange *Donor = nullptr;
      Optional<LinxV5::TileDescriptor> DonorDescriptor;
      bool SawLiveValue = false;
      for (unsigned I = TailIdx; I < HeadIdx; ++I) {
        const TRCopyRange &Candidate = LiveSlots[I % TRNumber];
        SawLiveValue |= Candidate.RegOp.Reg != LinxV5::NoRegister;
        if (Candidate.RegOp.Reg != LinxV5::NoRegister &&
            Candidate.RegOp.Descriptor && !Candidate.IsAssembleParent) {
          // Slot metadata is inherited across CFG edges and may name a prior
          // closed generation after the same physical register is redefined.
          // Re-prove the source descriptor at this exact insertion point so a
          // dead alignment filler cannot reinterpret live payload bits using
          // a stale dtype or shape.
          Optional<LinxV5::TileDescriptor> Current =
              LinxV5::tryGetTileRegDescriptor(*MBB, Before,
                                              Candidate.RegOp.Reg);
          if (!Current || Current->SizeCode != Candidate.RegOp.Size)
            continue;
          Donor = &Candidate;
          DonorDescriptor = *Current;
          break;
        }
      }
      if (!Donor) {
        if (SawLiveValue)
          report_fatal_error(
              "cannot materialize a Tile queue-alignment entry without a "
              "closed descriptor-bearing source");
        // An actually empty queue needs no physical alignment command. The
        // caller must leave the software queue unchanged as well.
        return false;
      }
      const LinxV5::TileDescriptor &Descriptor = *DonorDescriptor;
      BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::PseudoTCOPYDesc))
          .addReg(ReserveReg, RegState::Define | RegState::Dead)
          .addImm(Descriptor.SizeCode)
          .addReg(Donor->RegOp.Reg)
          .addImm(Descriptor.ValidCol)
          .addImm(Descriptor.ValidRow)
          .addImm(Descriptor.PhysicalCol)
          .addImm(Descriptor.DataType)
          .addImm(Descriptor.Layout);
    } else {
      if (LinxV5::SIMTCGSRegClass.contains(ReserveReg)) {
        BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::SIMT_ORI_SCAR))
            .addReg(ReserveReg, RegState::Define | RegState::Dead)
            .addImm(LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
            .addReg(LinxV5::R0)
            .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD)
            .addImm(0);
      } else {
        BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::SIMT_MOV))
            .addReg(ReserveReg, RegState::Define | RegState::Dead)
            .addImm(LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
            .addReg(LinxV5::R0)
            .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD);
      }
    }
  } else {
    if (!IsScalarRC) {
      if (LinxV5::Tile_ABS_CGRegClass.contains(RegOp.Reg)) {
        unsigned RegSizeCode = RegOp.Size;
        // This pass creates PseudoTCOPYDesc directly, bypassing
        // LinxV5InstrInfo::copyPhysReg().  PseudoTCOPYDesc is a Local-tile
        // operation; Shared tile handles must only be moved by the explicit
        // Shared transfer pseudos.  Keep this check next to the construction
        // so a future register-class change cannot silently lower a
        // Shared->Local copy to TCOPY/TMOV.
        if (!LinxV5::Tile_ABSRegClass.contains(TR))
          report_fatal_error(
              "LinxV5 TRegToOffset attempted PseudoTCOPY with non-Local tile");
        if (!RegOp.Descriptor)
          report_fatal_error(
              "cannot prove a complete Local tile descriptor for window COPY");
        LinxV5::TileDescriptor Descriptor = *RegOp.Descriptor;
        if (Descriptor.SizeCode != RegSizeCode)
          report_fatal_error(
              "Local tile COPY descriptor SizeCode disagrees with allocator");
        BuildMI(*MBB, Before, DebugLoc(),
                TII->get(LinxV5::PseudoTCOPYDesc))
            .addReg(TR, RegState::Define)
            .addImm(RegSizeCode)
            .addReg(TR, RegState::Kill)
            .addImm(Descriptor.ValidCol)
            .addImm(Descriptor.ValidRow)
            .addImm(Descriptor.PhysicalCol)
            .addImm(Descriptor.DataType)
            .addImm(Descriptor.Layout);
      } else {
        BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::SIMT_MOV))
            .addReg(TR, RegState::Define)
            .addImm(LinxV5::getSIMTDstTypeFromSize(RegOp.Size))
            .addReg(TR, RegState::Kill)
            .addImm(LinxV5::getSIMTSrcTypeFromSize(RegOp.Size));
      }
    } else {
      BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::ORI))
          .addReg(TR, RegState::Define)
          .addReg(TR, RegState::Kill)
          .addImm(0);
    }
  }
  LLVM_DEBUG(dbgs() << "build copy " << *std::prev(Before));
  return true;
}

void SlotCalc::PreAlloc(TRCopyRange Slot) {
  pre() = Slot;
  pre().Idx = PreIdx;
  expire();
  prealloc();
}

bool SlotCalc::SlotCopy(MachineBasicBlock::iterator Before, TRCopyRange &Slot) {
  if (Slot.IsAssembleParent)
    report_fatal_error(
        "cannot copy a live B.ASSEMBLE parent: the INIT parent must retain "
        "its tile identity through the complete generation");

  LLVM_DEBUG(dbgs() << "slot copy for " << Slot.Idx << " head " << HeadIdx
                    << "(" << headsi() << ")"
                    << " limit " << TRNumber << "\n");
  LLVM_DEBUG(dbgs() << "will expire " << TailIdx << "(" << tailsi() << ") Reg "
                    << printReg(tail().RegOp.Reg, TRI) << "\n");

  // Do Copy for PreAlloc Slots, no expire, only advance
  //  . . [x o p p] . .
  // -->
  //  . . [x o x x] . .
  while (has_pre()) {
    LLVM_DEBUG(dbgs() << "assign pre-alloc as copy " << HeadIdx << "\n");
    if (!BuildCopy(Before, head().RegOp))
      return false;
    advance();
  }

  if (!BuildCopy(Before, Slot.RegOp))
    return false;
  head() = Slot;
  head().Idx = HeadIdx;
  if (&Slot != &head())
    initSlot(&Slot);
  if (&tail() != &head()) {
    initSlot(&tail());
  }
  shift();
  return true;
}

static char printHelper(TRCopyRange &RR) {
  if (RR.IsLiveout)
    return '&';
  if (RR.LastUse != nullptr)
    return 'x';
  return 'o';
}

void SlotCalc::printLiveSlots() {
  dbgs() << "Tail: " << TailIdx << " Head: " << HeadIdx << "\n";
  dbgs() << "[";
  for (unsigned i = TailIdx; i < PreIdx; ++i) {
    if (i != TailIdx)
      dbgs() << " ";
    if (i >= HeadIdx)
      dbgs() << "p";
    else
      dbgs() << printHelper(LiveSlots[i % TRNumber]);
  }
  dbgs() << "]\n";

  dbgs() << "[";
  for (unsigned i = TailIdx; i < PreIdx; ++i) {
    if (i != TailIdx)
      dbgs() << " ";
    dbgs() << printReg(LiveSlots[i % TRNumber].RegOp.Reg, TRI);
  }
  dbgs() << "]\n";

  dbgs() << "[";
  for (unsigned i = TailIdx; i < PreIdx; ++i) {
    if (i != TailIdx)
      dbgs() << " ";
    dbgs() << LiveSlots[i % TRNumber].Idx;
  }
  dbgs() << "]\n";
}

SmallVector<TRCopyRange *>
SlotCalc::allocNewSlot(SmallVectorImpl<const TRLiveRange *> &LRs) {
  unsigned i = 0;
  SmallVector<TRCopyRange *> NewSlots;
  auto *MI = LRs.front()->MI;
  auto tailIdle = [&](MachineInstr *MI) {
    unsigned num = 0;
    for (unsigned i = TailIdx; i < HeadIdx; ++i) {
      if (isIdle(LiveSlots[i % TRNumber], MI))
        ++num;
      else
        break;
    }
    return num;
  };
  auto findFirstKill = [&](MachineInstr *MI) {
    for (unsigned i = TailIdx; i < HeadIdx; ++i) {
      if (isKill(LiveSlots[i % TRNumber], MI))
        return &LiveSlots[i % TRNumber];
    }
  };
  auto findFirstLive = [&](MachineInstr *MI) {
    for (unsigned i = TailIdx; i < HeadIdx; ++i) {
      if (!isIdle(LiveSlots[i % TRNumber], MI))
        return &LiveSlots[i % TRNumber];
    }
  };
  while (free() < LRs.size()) {
    assert(i++ < TRNumber * TRNumber);
    LLVM_DEBUG(dbgs() << "try find idle slot at " << TailIdx << "(" << tailsi()
                      << ") liveout " << tail().IsLiveout << "\n");
    // if (isKill(Slot, MI)) {
    //   // a kill slot is not a pure idle. should copy before any later copy
    //   // inserts
    //   PreAlloc(Slot);
    //   LLVM_DEBUG(printLiveSlots());
    //   continue;
    // } else if (isIdle(Slot, MI)) {
    //   unsigned Nr
    //   LLVM_DEBUG(printLiveSlots());
    //   continue;
    // }
    if (tailIdle(MI) + free() >= LRs.size()) {
      expire();
      LLVM_DEBUG(printLiveSlots());
      continue;
    }
    TRCopyRange *Slot = &tail();
    // real idle
    if (!isKill(*Slot, MI) && isIdle(*Slot, MI)) {
      LLVM_DEBUG(dbgs() << "first is "
                        << (isIdle(first(), MI) ? "idle" : "live") << "\n");
      Slot = isIdle(first(), MI) ? findFirstKill(MI) : findFirstLive(MI);
    }
    unsigned Copys = PreIdx - HeadIdx + 1;
    NrCopy += Copys;
    NumCopy += Copys;
    if (isTileReg(RC))
      TileLongCopy += Copys;
    if (!SlotCopy(MI, *Slot))
      report_fatal_error(
          "Tile allocation pressure requires an unrepresentable empty queue "
          "alignment entry");
    LLVM_DEBUG(printLiveSlots());
  }

  for (int n = 0; n < LRs.size(); ++n) {
    auto *LR = LRs[n];
    TRCopyRange &Slot = head();
    LLVM_DEBUG(dbgs() << "assign at " << HeadIdx << "(" << headsi() << ") idle "
                      << isIdle(Slot, MI) << "\n");
    assert(isIdle(Slot, MI));
    initSlot(&Slot);
    Slot.IsLiveout |= LR->IsLiveout;
    Slot.Idx = HeadIdx;
    advance();
    NewSlots.push_back(&Slot);
  }

  return NewSlots;
}

SmallVector<LinxRegOp> SlotCalc::getLiveouts(bool EnableVerify = true) {
  unsigned index = tailsi();
  bool findFirstLiveout = false;
  SmallVector<LinxRegOp> CurLiveouts;
  for (unsigned i = 0; i != TRNumber; ++i) {
    TRCopyRange &Slot = LiveSlots[index];
    if (Slot.IsLiveout && !findFirstLiveout)
      findFirstLiveout = true;
    if (findFirstLiveout) {
      LinxRegOp RegOp = Slot.RegOp;
      if (!Slot.IsLiveout)
        RegOp.Reg = LinxV5::NoRegister;
      CurLiveouts.push_back(RegOp);
    }
    index = index == TRNumber - 1 ? 0 : index + 1;
  }
  LLVM_DEBUG(dbgs() << "current liveouts are:\n"; printLiveSlots();
             dbgs() << "\n");
  if (EnableVerify)
    assert(CurLiveouts.size() <= TRNumber); // The ACC register has only one.
  return CurLiveouts;
}

void SlotCalc::BuildLiveoutAndEraseFrom(TRCopyRange &Slot, LinxRegOp RegOp,
                                        SmallVector<LinxRegOp> &CurLiveouts) {
  auto I = llvm::find(CurLiveouts, RegOp);
  if (I != CurLiveouts.end())
    CurLiveouts.erase(I);
  auto EndI = findTailCopyInsertPosition(*MBB);
  if (isTileReg(RC))
    ++TileCFGCopy;
  auto tmp = TRCopyRange(RegOp, RegOp.Reg != LinxV5::NoRegister);
  // Rolling a live-out changes only its queue position.  Preserve the OPEN
  // B.ASSEMBLE-parent identity from the source slot so SlotCopy cannot bypass
  // the live-parent copy guard, and so later rolls still know that the
  // generation is open.
  if (RegOp.Reg != LinxV5::NoRegister) {
    for (const TRCopyRange &LiveSlot : LiveSlots) {
      if (LiveSlot.RegOp.Reg == RegOp.Reg && LiveSlot.IsAssembleParent) {
        tmp.IsAssembleParent = true;
        break;
      }
    }
  }
  if (!SlotCopy(EndI, tmp))
    return;
  if (RegOp.Reg != LinxV5::NoRegister) {
    // update: release liveout flag.
    for (size_t j = 0; j < TRNumber; ++j) {
      if (LiveSlots[j].IsLiveout && LiveSlots[j].RegOp.Reg == RegOp.Reg)
        LiveSlots[j].IsLiveout = false;
    }
    Slot.IsLiveout = true;
  }
  Slot.RegOp = RegOp;
  return;
}

/// Rolling RegQ(Insert copy) to satisfy liveouts order.
/// Note:
/// * Previous progress ensure liveouts rolling must start at empty slot.
/// * We reserve one register for liveouts rolling.
/// E.g: [t1, t2, t3, t4, empty, t5, t6, t7]
///                         ^
///                    ^    |
///                    | Tail
///                rolling end
/// First, we start empty solt for rolling. At this rolling,
/// the worst rolling result is only satisfied one register order.
/// However, at next rolling, at least one more register can be order(previous
/// rolling end must before previous Tail). Finally, we can re-order all
/// liveouts, and the worst scenario is use (regNum-1)*(regNum-1) copy.
void SlotCalc::rollingLiveouts(SmallVector<LinxRegOp> needSyncTo) {
  if (needSyncTo.empty())
    return;

  // An OPEN assembly parent pins this architectural queue. Reordering any
  // liveout across it would either copy the parent payload or change the
  // continuation's ParentRef distance. Preserve the actual queue state;
  // ordinary operands are still rewritten from that state in Phase 2, while
  // solveAssembleParentDataflow owns the exact parent distances and join
  // rejection.
  if (llvm::any_of(LiveSlots,
                   [](const TRCopyRange &Slot) {
                     return Slot.IsAssembleParent;
                   }))
    return;

  LLVM_DEBUG(dbgs() << "Starting regQ rolling!\n");
  LLVM_DEBUG(dbgs() << "need sync to liveouts:\n";
             for (LinxRegOp &RO
                  : needSyncTo) { dbgs() << printReg(RO.Reg, TRI) << " "; };
             dbgs() << "\n");

  int i = 0;
  (void)i;
  while (true) {
    assert(i++ < TRNumber * TRNumber);
    LLVM_DEBUG(dbgs() << "rolling: \n");
    LLVM_DEBUG(printLiveSlots(); dbgs() << "\n");

    // get current liveouts
    SmallVector<LinxRegOp> CurLiveouts(getLiveouts());
    LLVM_DEBUG(dbgs() << "current liveouts:\n";
               for (LinxRegOp &RO
                    : CurLiveouts) { dbgs() << printReg(RO.Reg, TRI) << " "; };
               dbgs() << "\n");

    // TODO: Opt here. Consider current liveouts is {t1, t2, t3, t4}
    // need sync-up to {t1, t2, t4, t3}.
    // rolling Index can begin at t4 not t1, which save 2 copyies.
    unsigned rollingI = 0;
    for (; rollingI != needSyncTo.size(); ++rollingI) {
      TRCopyRange &Slot = tail();
      LLVM_DEBUG(dbgs() << "sync reg "
                        << printReg(needSyncTo[rollingI].Reg, TRI) << "\n");
      if (Slot.IsLiveout) {
        if (Slot.RegOp.Reg == needSyncTo[rollingI].Reg) {
          // success
          assert(Slot.RegOp.Size == needSyncTo[rollingI].Size);
          BuildLiveoutAndEraseFrom(Slot, needSyncTo[rollingI], CurLiveouts);
        } else {
          // fail: give-up current rolling by push the rest register.
          while (!CurLiveouts.empty()) {
            TRCopyRange &TmpSlot = tail();
            BuildLiveoutAndEraseFrom(TmpSlot, CurLiveouts[0], CurLiveouts);
          }
          break;
        }
      } else {
        // success. Empty slot is used by swap liveouts order.
        BuildLiveoutAndEraseFrom(Slot, needSyncTo[rollingI], CurLiveouts);
      }
    }

    if (rollingI == needSyncTo.size()) // finish
      break;
  }
}

unsigned SlotCalc::getRegSize(const TRLiveRange &RR) {
  // for scalar reg, return SIZE_D
  if (IsScalarRC || !RR.DefMO /*without def*/)
    return LinxV5::SIMTRegSize::SIMT_REG_SIZE_D;

  MachineInstr *MI = RR.MI;
  if (LinxV5::isTileOp(*MI)) {
    return LinxV5::getTileOpRegSize(*MI, RR.DefMO->getReg());
  } else {
    int DefIdx = MI->findRegisterDefOperandIdx(RR.DefMO->getReg());
    unsigned DstTypeEnum = MI->getOperand(DefIdx + 1).getImm();
    return LinxV5::getSizeFromSIMTType(DstTypeEnum);
  }
}

void SlotCalc::insertCopys(const SmallVector<TRLiveRange> &OriginalInstrs,
                           Register ReservedReg, SlotStatus *SS,
                           bool IsInSyncGroup,
                           SmallVector<LinxRegOp> needSyncTo,
                           SlotStatus &ResultSS) {
  init(SS);
  ReserveReg = ReservedReg;
  LLVM_DEBUG(
      MBB->printName(dbgs(), MachineBasicBlock::PrintNameIr |
                                 MachineBasicBlock::PrintNameAttributes));
  LLVM_DEBUG(dbgs() << "\n");
  LLVM_DEBUG(dbgs() << "For RC: " << TRI->getRegClassName(RC) << " dosync "
                    << IsInSyncGroup << "\n");
  LLVM_DEBUG(printLiveSlots(); dbgs() << "\n");
  auto OII = OriginalInstrs.begin();
  const auto OIE = OriginalInstrs.end();
  for (; OII != OIE; ) {
    const TRLiveRange &OI = *OII;
    MachineInstr *MI = OI.MI;
    LLVM_DEBUG(dbgs() << "iterating " << *MI);
    // SIMT Block will insert rolling liveout copy before terminator Inst.
    if (MI->isTerminator())
      break;
    if (MI->isImplicitDef()) {
      ++OII;
      continue;
    }
    unsigned Occ = getTSlotOccupation(*MI, RC);
    LLVM_DEBUG(dbgs() << "Occupation " << Occ << "\n");
    if (Occ >= 1) {
      SmallVector<const TRLiveRange *> LRs;
      for (auto &MO : MI->operands()) {
        if (!MO.isReg() || !MO.isDef())
          continue;
        auto &LR = *OII++;
        if (!RC->contains(LR.DefMO->getReg()))
          continue;
        assert(LR.MI == MI);
        LRs.push_back(&LR);
      }
      assert(LRs.size() == Occ);
      size_t AllocFromIdx = HeadIdx;
      SmallVector<TRCopyRange *> NewSlots = allocNewSlot(LRs);
      assert(NewSlots.size() == Occ);

      // apply
      for (int i = 0; i < Occ; ++i) {
        NewSlots[i]->LastUse = LRs[i]->LastUse;
        NewSlots[i]->RegOp.Size = getRegSize(*LRs[i]);
        NewSlots[i]->IsAssembleParent = isAssembleParentDef(*LRs[i]);
        if (LRs[i]->LastUse != nullptr || LRs[i]->IsLiveout) {
          NewSlots[i]->RegOp.Reg = LRs[i]->DefMO->getReg();
          NewSlots[i]->RegOp.Descriptor =
              LinxV5::tryGetTileRegDescriptor(
                  *MBB, std::next(LRs[i]->MI->getIterator()),
                  LRs[i]->DefMO->getReg());
        }
      }
      LLVM_DEBUG(dbgs() << "emit " << *OI.MI);
    } else {
      ++OII;
    }

    // A B.ASSEMBLE writer carrying Last=1 closes its generation: the parent
    // tile becomes ordinary published data and later window copies of it
    // are legal (issue #112). Clear the open-generation protection on the
    // live slot holding the referenced parent register; slots of other
    // registers and already-closed parents are unaffected.
    Optional<bool> ClosedLast = LinxV5::getAssembleControlBit(*MI, /*LastIdx=*/1);
    if (ClosedLast && *ClosedLast) {
      for (size_t j = 0; j < TRNumber; ++j) {
        if (!LiveSlots[j].IsAssembleParent)
          continue;
        Register R = LiveSlots[j].RegOp.Reg;
        if (R == LinxV5::NoRegister)
          continue;
        for (const MachineOperand &MO : MI->operands()) {
          if (MO.isReg() && !MO.isDef() && MO.getReg() == R) {
            LiveSlots[j].IsAssembleParent = false;
            break;
          }
        }
      }
    }

    // try release live slots
    for (size_t j = 0; j < TRNumber; ++j) {
      if (LiveSlots[j].LastUse == OI.MI) {
        LiveSlots[j].LastUse = nullptr;
      }
    }
    LLVM_DEBUG(printLiveSlots(); dbgs() << "\n");
  }
  assert((HeadIdx - TailIdx == TRNumber) && "invalid SlotCalc exit status");

  // Let's do something about SIMT.
  SmallVector<LinxRegOp> CurLiveouts(getLiveouts(false));
  SmallVector<LinxRegOp> FakeLiveous;
  for (size_t j = 0; j < TRNumber; ++j) {
    if (LiveSlots[j].LastUse != nullptr &&
        llvm::find(CurLiveouts, LiveSlots[j].RegOp) == CurLiveouts.end()) {
      // fake liveouts, release latter
      LiveSlots[j].IsLiveout = true;
      FakeLiveous.push_back(LiveSlots[j].RegOp);
    }
  }
  // Add copy until sync-up register less than 8.
  // Note: We call only sync-up 7 register for MBB liveouts.
  // So, if RegQ has bubble(Non-Liveout define),
  // we may need sync up 8 register, which may cause dead lock. E.g:
  // [&   &   &   O   O   O   &   &]
  // [t1  t2  t3  O   O   O   t6  t7]
  //      ^
  //  Head,Tail
  // sync-up order(Top-Down): [t2, t3, O, O, O, t6, t7, t1]
  // ==> after rolling
  // [&   &   &   O   O   O   &   &]
  // [t1  t2  t3  O   O   O   t6  t7]
  //              ^
  //          Head,Tail
  // sync-up order(Top-Down):: [t6, t7, t1, t2, t3]
  auto EndI = findTailCopyInsertPosition(*MBB);
  while (true) {
    if (!IsInSyncGroup)
      break;
    TRCopyRange &RR = tail();
    if (RR.RegOp.Reg == LinxV5::Tile_ACC1)
      break;
    if (RR.IsLiveout) {
      LLVM_DEBUG(dbgs() << "Try to make liveouts less than RegNum...\n");
      if (isTileReg(RC))
        ++TileCFGCopy;
      if (!SlotCopy(EndI, RR))
        break;
      LLVM_DEBUG(printLiveSlots(); dbgs() << "\n");
    } else {
      break;
    }
  }

  // Rolling RegQ(Insert copy) to satisfy liveouts order.
  // Firstly, do some fix-up for input liveouts itself.
  // consider scenario for block predecessors liveout not same:
  //     bb.0(def t1) --> bb.1(use t1[kill])
  //           \            /
  //           bb.2(redef t1)
  // bb.2 predecessors = {bb0, bb1}
  // bb.0 liveouts = {t1, t2, t3}
  // bb.1 liveouts = {t3, t2}
  // In this case, if bb.0 done firstly.
  // When handle bb.2, it's liveouts should sync-up to bb.0
  // Cause bb.0 liveouts without t1, liveouts order should:
  //   {t1, t2, t3} == changed to ==> {no-reg, t2, t3}
  if (!needSyncTo.empty()) {
    // Note: MBB.liveouts interface may give same register.
    SmallVector<LinxRegOp> CurLiveouts;
    for (size_t j = 0; j < TRNumber; ++j) {
      if (LiveSlots[j].IsLiveout) {
        CurLiveouts.push_back(LiveSlots[j].RegOp);
      }
    }

    // CurBlock is bb.1, liveouts = {t3, t2}
    // Should syn-up to bb.0 liveouts = {t1, t2, t3}
    // Should syn-up liveouts should fix-up:
    //   from {t1, t2, t3} to {no-reg, t2, t3}
    for (LinxRegOp &RegOp : needSyncTo) {
      // needSyncTo has no-reg means liveouts have bubble.
      // E.g:
      //   ...
      //   t1(liveout) =
      //   t2 = // ==> liveout bubble
      //   t3(liveout) = use t2
      if (RegOp.Reg != LinxV5::NoRegister) {
        auto I = llvm::find(CurLiveouts, RegOp);
        if (I == CurLiveouts.end())
          RegOp = LinxRegOp();
        else
          CurLiveouts.erase(I);
      }
    }

    // CurBlock is bb.0, liveouts = {t1, t2, t3}
    // Should syn-up to bb.1 liveouts = {t3, t2}
    // Should syn-up liveouts should fix-up:
    //   from {t3, t2} to {t1, t3, t2}
    // Note: t1 should insert at begin of liveouts.
    if (!CurLiveouts.empty()) {
      for (LinxRegOp &RegOp : llvm::reverse(needSyncTo)) {
        // fully use the liveouts bubble
        if (RegOp.Reg == LinxV5::NoRegister && !CurLiveouts.empty()) {
          RegOp = CurLiveouts.back();
          CurLiveouts.erase(std::prev(CurLiveouts.end()));
        }
      }
      if (!CurLiveouts.empty()) {
        for (LinxRegOp RegOp : CurLiveouts)
          needSyncTo.insert(needSyncTo.begin(), RegOp);
      }
    }
  }

  rollingLiveouts(needSyncTo);

  for (size_t j = 0; j < TRNumber; ++j) {
    if (LiveSlots[j].IsLiveout &&
        llvm::find(FakeLiveous, LiveSlots[j].RegOp) != FakeLiveous.end()) {
      // release fake liveouts
      LiveSlots[j].IsLiveout = false;
    }
  }

  CurLiveouts = getLiveouts();

  for (size_t j = 0; j < TRNumber; ++j) {
    LiveSlots[j].IsLiveout = false;
    LiveSlots[j].LastUse = nullptr;
  }
  LLVM_DEBUG(dbgs() << "save tail " << TailIdx << " head " << HeadIdx << "\n");
  ResultSS = SlotStatus(LiveSlots, CurLiveouts, HeadIdx, TailIdx);

  return;
}

static bool isLinxV5BlockTerminator(MachineInstr *MI) {
  return MI->isTerminator() || LinxV5::isIsolateInstr(*MI);
}

static void __verifyGPR(MachineBasicBlock &MBB) {
  DenseSet<Register> Defs;
  for (auto &MI : MBB) {
    if (isLinxV5BlockTerminator(&MI)) {
      // Starts a new linx block.
      Defs.clear();
    }

    if (MI.isImplicitDef() || MI.isKill() || MI.isCall() || MI.isDebugInstr())
      continue;

    for (auto &MO : MI.defs()) {
      if (!MO.isReg() || MO.isImplicit())
        continue;
      Register Reg = MO.getReg();
      if (LinxV5::GRRegClass.contains(Reg)) {
        if (Defs.count(Reg) == 0) {
          Defs.insert(Reg);
        } else {
          // The fast register allocator (used at -O0) legitimately assigns
          // several virtual registers to the same physical GPR inside one
          // Linx block, so a second physical def is not by itself an error.
          // Only a re-definition of the same virtual value would violate
          // the tile-to-offset bookkeeping; that cannot happen post-RA, so
          // keep this as an observation instead of an assertion.
          LLVM_DEBUG(dbgs() << MBB << "\n");
          LLVM_DEBUG(dbgs() << "bgpr multi set inst: " << MI << "\n");
        }
      }
    }
  }
}

static void verifyGPR(MachineFunction &MF) {
  for (auto &MBB : MF) {
    __verifyGPR(MBB);
  }
}

bool LinxV5TRegToOffsetOpt::canSkipMI(MachineInstr &MI) {
  if (MI.isCFIInstruction() || MI.isDebugInstr() || MI.isLabel() ||
      MI.isKill() || MI.isCall())
    return true;
  return false;
}

bool LinxV5TRegToOffsetOpt::isDistanceReg(Register Reg) {
  for (auto RCInfo : RCsInfo) {
    const TargetRegisterClass *RC = RCInfo.first;
    if (isRC(Reg, RC, MRI))
      return true;
  }
  return false;
}

bool LinxV5TRegToOffsetOpt::isInSyncGroup(MachineBasicBlock *MBB) {
  if (!GlobalSyncupMap.count(MBB))
    return false;
  return !(GlobalSyncupMap[MBB].size() == 1 &&
           *GlobalSyncupMap[MBB].begin() == MBB);
}

/// Generally, if BB1 has predecessors: {BB0, BB2}, BB0 and BB2 liveouts should
/// sync-up. But when exist BB4 has predecessors: {BB2, BB3}, only sync-up BB2
/// and BB3 liveouts is not enough. At this time, {BB0, BB2, BB3} liveouts
/// should sync-up for each others.
void LinxV5TRegToOffsetOpt::updateGlobalSyncup(MachineFunction &MF) {
  for (MachineBasicBlock &MBB : MF) {
    if (MBB.succ_empty())
      continue;
    for (MachineBasicBlock *Succ : MBB.successors()) {
      for (MachineBasicBlock *Pred : Succ->predecessors()) {
        GlobalSyncupMap[&MBB].insert(Pred);
      }
    }
  }

  // Maybe SIMT do not need handle complex CFG?
  if (!EnableGlobalDataSync)
    return;

  // Global sync up!
  for (auto GlobalSyncup : GlobalSyncupMap) {
    for (MachineBasicBlock *MBB : GlobalSyncup.second)
      GlobalSyncupMap[MBB].insert(GlobalSyncup.second.begin(),
                                  GlobalSyncup.second.end());
  }

  LLVM_DEBUG(dbgs() << MF.getName() << " Global sync up result:\n";
             for (auto GlobalSync
                  : GlobalSyncupMap) {
               GlobalSync.first->printName(dbgs());
               dbgs() << " <==========> ";
               for (MachineBasicBlock *MBB : GlobalSync.second) {
                 MBB->printName(dbgs());
                 dbgs() << ", ";
               }
               dbgs() << "\n";
             };
             dbgs() << "\n");
}

MachineBasicBlock *LinxV5TRegToOffsetOpt::getNextBlock() {
  for (MachineBasicBlock *MBB : TBDoneMBBs) {
    if (MBB->pred_empty())
      return MBB;
    if (MBB->pred_size() == 1) {
      MachineBasicBlock *SinglePred = *(MBB->pred_begin());
      if (SinglePred == MBB)
        return MBB;
    }
    if (llvm::any_of(MBB->predecessors(), [&](const MachineBasicBlock *Pred) {
          return llvm::find(DoneMBBs, Pred) != DoneMBBs.end();
        }))
      return MBB;
  }

  return TBDoneMBBs[0];
}

SlotStatus *LinxV5TRegToOffsetOpt::getInitStatus(
    MachineBasicBlock &MBB, DenseMap<Register, MachineInstr *> LiveInsLastUse,
    DenseSet<Register> RestLiveouts, const TargetRegisterClass *RC) {
  SmallVector<SlotStatus *> DoneMBBsSlotStatus;
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    if (llvm::find(DoneMBBs, Pred) != DoneMBBs.end()) {
      LLVM_DEBUG(dbgs() << "done pred " << printMBBReference(*Pred) << " tail "
                        << GlobalSlotsInfo[Pred][RC].TailIdx << " head "
                        << GlobalSlotsInfo[Pred][RC].HeadIdx << "\n");
      DoneMBBsSlotStatus.push_back(&GlobalSlotsInfo[Pred][RC]);
    }
  }

  // A physical Local-tile live-in denotes one SSA/PHI value at this join.
  // Queue position reconciliation alone is insufficient: two predecessors
  // can carry the same allocated register with incompatible descriptors.  A
  // size-only comparison would then silently publish path-dependent dtype or
  // CUBE geometry.  Require every completed predecessor that carries the
  // live-in to agree on the complete descriptor before selecting a queue
  // state to inherit.
  if (isTileReg(RC) && DoneMBBsSlotStatus.size() > 1) {
    for (const TRCopyRange &Reference :
         DoneMBBsSlotStatus.front()->TRCopyRanges) {
      if (!Reference.IsLiveout ||
          !MBB.isLiveIn(Reference.RegOp.Reg))
        continue;
      if (!Reference.RegOp.Descriptor)
        report_fatal_error(
            "unknown Local tile descriptor at COPY/PHI control-flow join");
      for (SlotStatus *Incoming : drop_begin(DoneMBBsSlotStatus)) {
        const TRCopyRange *Match = nullptr;
        for (const TRCopyRange &Candidate : Incoming->TRCopyRanges)
          if (Candidate.IsLiveout &&
              Candidate.RegOp.Reg == Reference.RegOp.Reg) {
            Match = &Candidate;
            break;
          }
        if (!Match || !Match->RegOp.Descriptor)
          report_fatal_error(
              "unknown Local tile descriptor at COPY/PHI control-flow join");
        if (!(*Reference.RegOp.Descriptor == *Match->RegOp.Descriptor))
          report_fatal_error(
              "conflicting Local tile descriptors at COPY/PHI control-flow join");
      }
    }
  }

  SlotStatus *SS = nullptr;
  if (!DoneMBBsSlotStatus.empty()) {
    SS = &GlobalSlotsInfo[&MBB][RC];
    SS->TailIdx = DoneMBBsSlotStatus[0]->TailIdx;
    SS->HeadIdx = DoneMBBsSlotStatus[0]->HeadIdx;
    SS->TRCopyRanges = DoneMBBsSlotStatus[0]->TRCopyRanges;

    // update liveins
    // evict not liveins
    //       pred liveouts: t0, t1
    // other liveins: t0, t1  | my liveins: t0
    // should evict t1
    for (size_t i = 0; i < RC->getNumRegs(); ++i) {
      auto &Slot = SS->TRCopyRanges[i];
      if (Slot.IsLiveout && !MBB.isLiveIn(Slot.RegOp.Reg))
        Slot.IsLiveout = false;
    }

    // update live-thrus
    size_t j = SS->TailIdx % RC->getNumRegs();
    for (size_t i = 0; i < RC->getNumRegs(); ++i) {
      // back-ward travel means mbb reverse order.
      j = j == 0 ? RC->getNumRegs() - 1 : j - 1;
      TRCopyRange &Slot = SS->TRCopyRanges[j];
      // update live-in info.
      if (LiveInsLastUse.count(Slot.RegOp.Reg)) {
        Slot.LastUse = LiveInsLastUse[Slot.RegOp.Reg];
        LiveInsLastUse.erase(Slot.RegOp.Reg);
      }
      // update live-through info.
      if (RestLiveouts.count(Slot.RegOp.Reg)) {
        Slot.IsLiveout = true;
        RestLiveouts.erase(Slot.RegOp.Reg);
      }
    }
    LLVM_DEBUG(for (auto &Slot
                    : SS->TRCopyRanges) {
      dbgs() << printReg(Slot.RegOp.Reg, TRI)
             << (Slot.IsLiveout ? "(&), " : "(o), ");
    } dbgs() << "\n");
  }

  return SS;
}

/// If one Block successors have Done predecessors,
/// current Block liveout order should sync to that one.
SmallVector<LinxRegOp>
LinxV5TRegToOffsetOpt::getLiveoutLimits(MachineBasicBlock &MBB,
                                        const TargetRegisterClass *RC) {
  SmallVector<LinxRegOp> Res;
  if (MBB.succ_empty())
    return Res;
  DenseSet<Register> SuccLiveIns = collectSuccessorLiveInsForRC(MBB, RC, MRI);
  if (SuccLiveIns.empty())
    return Res;
  if (GlobalSyncupMap.count(&MBB) == 0)
    return Res;
  SmallVector<MachineBasicBlock *> needSyncToMBBs;
  for (MachineBasicBlock *needSyncTo : GlobalSyncupMap[&MBB]) {
    if (llvm::find(DoneMBBs, needSyncTo) != DoneMBBs.end())
      needSyncToMBBs.push_back(needSyncTo);
  }
  if (needSyncToMBBs.empty())
    return Res;

  // consider current bb.2 need sync-up to bb.0, bb.1, E.g:
  // bb.0.liveouts = {t1, no-reg, t3, no-reg, t5}
  // bb.1.liveouts = {t1, t2, no-reg, t4, t5}
  // The bb.2 liveous should consider both of them
  // bb.2.liveouts limit = bb.0.liveouts U bb.1.liveouts
  //  ==> {t1, t2, t3, t4, t5}
  MachineBasicBlock *MaxSizeMBB =
      *std::max_element(needSyncToMBBs.begin(), needSyncToMBBs.end(),
                        [&](MachineBasicBlock *LHS, MachineBasicBlock *RHS) {
                          return GlobalSlotsInfo[LHS][RC].LiveOuts.size() <
                                 GlobalSlotsInfo[RHS][RC].LiveOuts.size();
                        });
  unsigned MaxSize = GlobalSlotsInfo[MaxSizeMBB][RC].LiveOuts.size();
  Res.assign(MaxSize, LinxRegOp());
  // Positions where the done successors demanded different registers at the
  // same tail-aligned depth. No single window order satisfies all of them.
  SmallVector<bool, 16> Blocked(MaxSize, false);
  bool HasLiveoutLimit = false;
  for (unsigned i = 0; i != MaxSize; ++i) {
    for (MachineBasicBlock *needSyncToMBB : needSyncToMBBs) {
      SmallVector<LinxRegOp> CurLiveouts(
          GlobalSlotsInfo[needSyncToMBB][RC].LiveOuts);
      if (CurLiveouts.size() <= i)
        continue;
      LinxRegOp CurRegOp = CurLiveouts[CurLiveouts.size() - 1 - i];
      LinxRegOp ResRegOp = Res[MaxSize - 1 - i];
      if (CurRegOp.Reg == LinxV5::NoRegister)
        continue;
      if (!SuccLiveIns.count(CurRegOp.Reg))
        continue;
      if (Blocked[MaxSize - 1 - i])
        continue;
      if (ResRegOp.Reg == LinxV5::NoRegister) {
        Res[MaxSize - 1 - i] = CurRegOp;
        HasLiveoutLimit = true;
      } else if (ResRegOp == CurRegOp) {
        HasLiveoutLimit = true;
      } else {
        // Issue #60: an unrolled if-select can leave a different tile
        // live-out at the same tail-aligned queue depth per successor arm.
        // Drop the constraint for this depth and mark the register class:
        // Phase-1 only inserts copies, which stay correct, but Phase-2 must
        // not rewrite this class to window offsets because predecessors now
        // disagree on the window state.
        LLVM_DEBUG(dbgs() << "liveout sync conflict at depth " << i << ": "
                          << printReg(ResRegOp.Reg, TRI) << " vs "
                          << printReg(CurRegOp.Reg, TRI) << " in "
                          << TRI->getRegClassName(RC) << "\n");
        Res[MaxSize - 1 - i] = LinxRegOp();
        Blocked[MaxSize - 1 - i] = true;
        if (SyncConflictRCs.insert(RC).second)
          ++NumSyncConflictRC;
      }
    }
  }

  if (!HasLiveoutLimit)
    return SmallVector<LinxRegOp>();
  return Res;
}

void LinxV5TRegToOffsetOpt::insertCopy(MachineBasicBlock &MBB) {
  // calc liverange.
  SmallVector<TRLiveRange> OriginalInstrs;
  DenseMap<Register, unsigned> TRDefs;  // the index of each reg
  DenseMap<Register, MachineInstr *> LiveInsLastUse; // used to update regQ
  OriginalInstrs.reserve(MBB.size());
  for (auto &MI : MBB) {
    if (canSkipMI(MI))
      continue;
    // if (MI.isImplicitDef())
    //   continue;
    size_t Idx = OriginalInstrs.size();
    SmallVector<MachineOperand *, 2> DefMOs;
    for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
      auto &MO = MI.getOperand(i);
      if (!MO.isReg())
        continue;
      if (!isDistanceReg(MO.getReg()))
        continue;
      if (MO.isDef()) {
        OriginalInstrs.emplace_back(&MI);
        OriginalInstrs.back().DefMO = &MO;
        OriginalInstrs.back().DefIdx = i;
        DefMOs.push_back(&MO);
      }
      // find the last use of each MI/LiveIns
      if (MO.isUse()) {
        if (TRDefs.count(MO.getReg())) {
          OriginalInstrs[TRDefs[MO.getReg()]].LastUse = &MI;
        } else {
          LiveInsLastUse[MO.getReg()] = &MI;
        }
      }
    }
    for (int i = 0; i < DefMOs.size(); ++i) {
      auto *Def = DefMOs[i];
      TRDefs[Def->getReg()] = Idx + i;
    }
    if (DefMOs.empty())
      OriginalInstrs.emplace_back(&MI);
  }

  // for each RegClass, mark the liveout MI of that RegClass
  for (auto &RCInfo : RCsInfo) {
    const TargetRegisterClass *RC = RCInfo.first;
    bool IsScalarRC = isScalarReg(RC);
    bool IsTileRC = isTileReg(RC);
    // Reg with liveout flag means can not pop from regQ.
    DenseSet<Register> RestLiveouts;

    if (RCInfo.first == &LinxV5::TILE_ABS_ACCRegClass)
      // As the acc register has only one index slot, we can treate it as an
      // absolute index register.
      continue;

    for (const auto &LiveReg : MBB.liveouts()) {
      Register Reg = LiveReg.PhysReg;
      if (!isRC(Reg, RC, MRI))
        continue;
      if (TRDefs.count(Reg) && OriginalInstrs[TRDefs[Reg]].MI->getOpcode()
          == TargetOpcode::IMPLICIT_DEF)
        continue;
      if (TRDefs.count(Reg))
        OriginalInstrs[TRDefs[Reg]].IsLiveout = true;
      else if (MBB.isLiveIn(Reg))
        // live-through scenario, used to update regQ.
        RestLiveouts.insert(Reg);
    }

    // insert copy
    SlotCalc SC(&MBB, TII, RC, IsScalarRC, IsTileRC);
    SlotStatus ResultSS;
    const bool CanSynchronizeQueue =
        !SyncConflictRCs.count(RC) && !PinnedAssembleParentRCs.count(RC);
    SC.insertCopys(OriginalInstrs, RCsInfo[RC][2],
                   getInitStatus(MBB, LiveInsLastUse, RestLiveouts, RC),
                   CanSynchronizeQueue && isInSyncGroup(&MBB),
                   CanSynchronizeQueue ? getLiveoutLimits(MBB, RC)
                                       : SmallVector<LinxRegOp>(),
                   ResultSS);
    GlobalSlotsInfo[&MBB][RC] = ResultSS; // commit status
  }
}

void dumpLiveouts(SmallVector<LinxRegOp> &LiveOuts, const TargetRegisterInfo* TRI) {
  dbgs() << "[";
  for (auto &Reg : LiveOuts)
    dbgs() << printReg(Reg.Reg, TRI) << " ";
  dbgs() << "]\n";
}

void LinxV5TRegToOffsetOpt::legalityCheck(MachineFunction &MF) {
  // Block liveouts check.
  // For each group of sync-up MBB, check for each RegClass
  // whether these MBBs are synced
  return;
  for (auto &GS : GlobalSyncupMap) {
    for (auto RCInfo : RCsInfo) {
      const TargetRegisterClass *RC = RCInfo.first;
      LLVM_DEBUG(dbgs() << "check sync group on " << TRI->getRegClassName(RC)
                        << ":\n");
      if (GS.second.size() <= 1)
        continue;
      SmallVector<LinxRegOp> GoldenLiveouts(RC->getNumRegs(), LinxRegOp());
      for (MachineBasicBlock *TBSync : GS.second) {
        LLVM_DEBUG(dbgs() << "check " << printMBBReference(*TBSync) << "\n");
        SmallVector<LinxRegOp> CurLiveouts(
            GlobalSlotsInfo[TBSync][RC].LiveOuts);
        LLVM_DEBUG(dbgs() << "GoldenLiveouts: ";);
        LLVM_DEBUG(dumpLiveouts(GoldenLiveouts, TRI););
        LLVM_DEBUG(dbgs() << "CurLiveouts: ";);
        LLVM_DEBUG(dumpLiveouts(CurLiveouts, TRI););
        unsigned CheckSize =
            std::min(CurLiveouts.size(), GoldenLiveouts.size());
        for (unsigned i = 0, e = CheckSize; i != e; ++i) {
          LinxRegOp &GoldenRegOp =
              GoldenLiveouts[GoldenLiveouts.size() - 1 - i];
          LinxRegOp CurRegOp = CurLiveouts[CurLiveouts.size() - 1 - i];
          if (CurRegOp.Reg == LinxV5::NoRegister)
            continue;
          if (GoldenRegOp.Reg == LinxV5::NoRegister) {
            GoldenRegOp = CurRegOp;
            continue;
          } else {
            assert(GoldenRegOp == CurRegOp && "liveouts not syn-up!");
          }
        }
      }
    }
  }
}

void LinxV5TRegToOffsetOpt::solveAssembleParentDataflow(MachineFunction &MF) {
  AssembleParentUseDistances.clear();
  SmallVector<std::pair<Register, MachineInstr *>, 4> ParentEpochs;
  DenseMap<unsigned, unsigned> ParentEpochCounts;
  DenseSet<const MachineInstr *> ParentRefUses;
  DenseMap<const MachineInstr *, unsigned> ParentRefProofCounts;
  DenseMap<const MachineInstr *, unsigned> ParentRefAnalysisCounts;
  DenseSet<const MachineInstr *> ReachedUnprovedParentRefs;
  DenseSet<const MachineInstr *> IncompleteParentRefProofs;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isInlineAsm() ||
          !StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
               .contains("B.ASSEMBLE"))
        continue;
      if (LinxV5::getAssembleParentRef(MI) != LinxV5::NoRegister)
        ParentRefUses.insert(&MI);
      Optional<bool> Last = LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
      if (!Last || *Last)
        continue;
      for (MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && MO.getReg().isPhysical() &&
            LinxV5::Tile_ABSRegClass.contains(MO.getReg())) {
          Register Parent = MO.getReg();
          ParentEpochs.emplace_back(Parent, &MI);
          ++ParentEpochCounts[Parent.id()];
        }
      }
    }
  }

  for (const auto &Entry : ParentEpochs) {
    Register Parent = Entry.first;
    MachineInstr *InitMI = Entry.second;
    const TargetRegisterClass *ParentRC = nullptr;
    for (const TargetRegisterClass *RC :
         {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
          &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
      if (RC->contains(Parent)) {
        ParentRC = RC;
        break;
      }
    }
    if (!ParentRC)
      continue;

    enum class StateKind {
      Unknown,
      Absent,
      ExplicitUndef,
      Parent,
      Other,
      Conflict
    };
    enum class ParentPhase { None, Open, Closed, Maybe };
    struct ParentState {
      StateKind Kind = StateKind::Unknown;
      MachineInstr *Def = nullptr;
      unsigned Distance = 0;
      ParentPhase Phase = ParentPhase::None;
    };
    auto sameState = [](const ParentState &A, const ParentState &B) {
      return A.Kind == B.Kind && A.Def == B.Def && A.Distance == B.Distance &&
             A.Phase == B.Phase;
    };
    auto unknownState = []() { return ParentState(); };
    auto absentState = []() {
      ParentState S;
      S.Kind = StateKind::Absent;
      return S;
    };
    auto conflictState = []() {
      ParentState S;
      S.Kind = StateKind::Conflict;
      return S;
    };
    DenseMap<MachineBasicBlock *, ParentState> InState;
    DenseMap<MachineBasicBlock *, ParentState> OutState;
    bool SawOutOfWindow = false;
    for (MachineBasicBlock &MBB : MF) {
      InState[&MBB] = unknownState();
      OutState[&MBB] = unknownState();
    }

    // Prove parent continuations and ambiguous ordinary uses with an
    // edge-sensitive execution of the
    // small scalar subset that controls canonical counted loops.  The proof
    // is deliberately fail-closed: unrecognized definitions invalidate their
    // aliases, regmasks invalidate every clobbered constant, unknown branches
    // explore every successor.  A continuation requires the exact INIT epoch
    // to be OPEN at one uniform queue distance on every path.  An ordinary
    // use requires uniformly Closed or the same overwriting definition.
    // uint64_t arithmetic models the target GR width and therefore preserves
    // modular overflow exactly.
    struct UseProof {
      bool Proven = false;
      bool Reached = false;
      bool Complete = false;
      unsigned Distance = 0;
    };
    DenseMap<const MachineInstr *, UseProof> UseProofs;
    auto proveUse = [&](MachineInstr *UseMI, bool RequireOpenParent) {
      auto Cached = UseProofs.find(UseMI);
      if (Cached != UseProofs.end())
        return Cached->second;

      struct SpillSlotKey {
        int FI = 0;
        int64_t Offset = 0;
        uint64_t Size = 0;

        bool operator<(const SpillSlotKey &Other) const {
          return std::tie(FI, Offset, Size) <
                 std::tie(Other.FI, Other.Offset, Other.Size);
        }
      };
      struct PathState {
        MachineBasicBlock *MBB = nullptr;
        StateKind Kind = StateKind::Absent;
        ParentPhase Phase = ParentPhase::None;
        MachineInstr *Def = nullptr;
        unsigned Distance = 0;
        std::map<unsigned, uint64_t> Constants;
        std::map<SpillSlotKey, uint64_t> SpillConstants;
      };
      SmallVector<PathState, 32> Worklist;
      PathState Entry;
      Entry.MBB = &MF.front();
      Worklist.push_back(std::move(Entry));
      std::set<std::string> Visited;
      bool SawUse = false;
      Optional<StateKind> ReachingUseKind;
      MachineInstr *ReachingOtherDef = nullptr;
      Optional<unsigned> ReachingParentDistance;
      constexpr unsigned MaxPathStates = 65536;
      unsigned ProcessedStates = 0;

      auto getConstant = [](const PathState &State,
                            Register Reg) -> Optional<uint64_t> {
        if (Reg == LinxV5::R0)
          return uint64_t(0);
        auto It = State.Constants.find(Reg.id());
        if (It == State.Constants.end())
          return None;
        return It->second;
      };
      auto invalidateAliases = [&](PathState &State, Register Def) {
        if (!Def.isPhysical())
          return;
        for (auto It = State.Constants.begin(); It != State.Constants.end();) {
          if (TRI->regsOverlap(Register(It->first), Def))
            It = State.Constants.erase(It);
          else
            ++It;
        }
      };
      auto makeKey = [](const PathState &State) {
        std::string Key = std::to_string(State.MBB->getNumber()) + ":" +
                          std::to_string(static_cast<unsigned>(State.Kind)) +
                          ":" +
                          std::to_string(static_cast<unsigned>(State.Phase)) +
                          ":" + std::to_string(
                                      reinterpret_cast<uintptr_t>(State.Def)) +
                          ":" + std::to_string(State.Distance);
        for (const auto &Entry : State.Constants)
          Key += ":" + std::to_string(Entry.first) + "=" +
                 std::to_string(Entry.second);
        for (const auto &Entry : State.SpillConstants)
          Key += ":s" + std::to_string(Entry.first.FI) + ":" +
                 std::to_string(Entry.first.Offset) + ":" +
                 std::to_string(Entry.first.Size) + "=" +
                 std::to_string(Entry.second);
        return Key;
      };

      const MachineFrameInfo &MFI = MF.getFrameInfo();
      auto getExactSpillAccess = [&](const MachineInstr &MI,
                                     bool IsLoad) -> Optional<SpillSlotKey> {
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
        if (!PSV)
          return None;
        int FI = PSV->getFrameIndex();
        if (!MFI.isSpillSlotObjectIndex(FI) || PSV->mayAlias(&MFI))
          return None;
        return SpillSlotKey{FI, MMO->getOffset(), MMO->getSize()};
      };
      auto invalidateSpillWrites = [&](PathState &State,
                                       const MachineInstr &MI) {
        for (const MachineMemOperand *MMO : MI.memoperands()) {
          if (!MMO->isStore())
            continue;
          const auto *PSV = dyn_cast_or_null<FixedStackPseudoSourceValue>(
              MMO->getPseudoValue());
          if (!PSV)
            continue;
          int FI = PSV->getFrameIndex();
          uint64_t Size = MMO->getSize();
          int64_t WriteEnd = 0;
          bool UnknownRange =
              Size == ~UINT64_C(0) || Size > uint64_t(INT64_MAX) ||
              AddOverflow(MMO->getOffset(), static_cast<int64_t>(Size),
                          WriteEnd);
          for (auto It = State.SpillConstants.begin();
               It != State.SpillConstants.end();) {
            if (It->first.FI != FI) {
              ++It;
              continue;
            }
            int64_t SlotEnd = 0;
            bool SlotOverflow =
                AddOverflow(It->first.Offset,
                            static_cast<int64_t>(It->first.Size), SlotEnd);
            if (UnknownRange || SlotOverflow ||
                (MMO->getOffset() < SlotEnd && It->first.Offset < WriteEnd))
              It = State.SpillConstants.erase(It);
            else
              ++It;
          }
        }
      };

      // Keep only the scalar values that can affect an explored CFG edge.
      // Address and payload temporaries otherwise multiply the path-state
      // space even though they cannot change reachability.
      std::set<unsigned> RelevantRegs;
      std::set<SpillSlotKey> RelevantSpills;
      for (const MachineBasicBlock &MBB : MF) {
        for (const MachineInstr &MI : MBB) {
          if ((MI.getOpcode() == LinxV5::BEQ ||
               MI.getOpcode() == LinxV5::BNE) &&
              MI.getNumOperands() >= 2) {
            for (unsigned I = 0; I != 2; ++I)
              if (MI.getOperand(I).isReg())
                RelevantRegs.insert(MI.getOperand(I).getReg().id());
          } else if ((MI.getOpcode() == LinxV5::SETC_EQI ||
                      MI.getOpcode() == LinxV5::SETC_NEI ||
                      MI.getOpcode() == LinxV5::SETC_GEI) &&
                     MI.getNumOperands() >= 1 && MI.getOperand(0).isReg()) {
            RelevantRegs.insert(MI.getOperand(0).getReg().id());
          }
        }
      }
      bool RelevantChanged = true;
      while (RelevantChanged) {
        RelevantChanged = false;
        for (const MachineBasicBlock &MBB : MF) {
          for (const MachineInstr &MI : MBB) {
            if (MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
                RelevantRegs.count(MI.getOperand(0).getReg().id()) &&
                (MI.getOpcode() == LinxV5::ORI ||
                 MI.getOpcode() == LinxV5::ADDI)) {
              RelevantChanged |=
                  RelevantRegs.insert(MI.getOperand(1).getReg().id()).second;
            } else if (MI.getNumOperands() >= 3 &&
                       MI.getOperand(0).isReg() &&
                       RelevantRegs.count(MI.getOperand(0).getReg().id()) &&
                       MI.getOpcode() == LinxV5::OR_SW) {
              RelevantChanged |=
                  RelevantRegs.insert(MI.getOperand(1).getReg().id()).second;
              RelevantChanged |=
                  RelevantRegs.insert(MI.getOperand(2).getReg().id()).second;
            }
            Optional<SpillSlotKey> Load =
                getExactSpillAccess(MI, /*IsLoad=*/true);
            if (Load && MI.getNumOperands() >= 1 &&
                MI.getOperand(0).isReg() &&
                RelevantRegs.count(MI.getOperand(0).getReg().id()))
              RelevantChanged |= RelevantSpills.insert(*Load).second;
            Optional<SpillSlotKey> Store =
                getExactSpillAccess(MI, /*IsLoad=*/false);
            if (Store && RelevantSpills.count(*Store) &&
                MI.getNumOperands() >= 1 && MI.getOperand(0).isReg())
              RelevantChanged |=
                  RelevantRegs.insert(MI.getOperand(0).getReg().id()).second;
          }
        }
      }

      while (!Worklist.empty()) {
        if (++ProcessedStates > MaxPathStates) {
          LLVM_DEBUG(dbgs() << "B.ASSEMBLE scalar proof state cap for "
                            << printReg(Parent, TRI) << " init " << *InitMI
                            << "use " << *UseMI);
          UseProofs[UseMI] = UseProof();
          return UseProof();
        }
        PathState State = Worklist.pop_back_val();
        if (!Visited.insert(makeKey(State)).second)
          continue;

        Optional<bool> CArg;
        Optional<bool> ConditionalTaken;
        MachineBasicBlock *ConditionalTarget = nullptr;
        bool EndedAtUse = false;
        for (MachineInstr &MI : *State.MBB) {
          if (&MI == UseMI) {
            SawUse = true;
            StateKind UseKind = State.Kind;
            if (RequireOpenParent) {
              if (UseKind != StateKind::Parent ||
                  State.Phase != ParentPhase::Open || State.Def != InitMI) {
                LLVM_DEBUG(dbgs()
                           << "reached continuation without exact OPEN epoch "
                           << printReg(Parent, TRI) << " init " << *InitMI
                           << "state kind " << static_cast<unsigned>(UseKind)
                           << " phase "
                           << static_cast<unsigned>(State.Phase)
                           << " distance " << State.Distance << " use "
                           << MI);
                UseProof Failed;
                Failed.Reached = true;
                Failed.Complete = true;
                UseProofs[UseMI] = Failed;
                return Failed;
              }
              if (!ReachingParentDistance)
                ReachingParentDistance = State.Distance;
              else if (*ReachingParentDistance != State.Distance) {
                UseProof Failed;
                Failed.Reached = true;
                Failed.Complete = true;
                UseProofs[UseMI] = Failed;
                return Failed;
              }
            } else {
              if (!((UseKind == StateKind::Parent &&
                     State.Phase == ParentPhase::Closed) ||
                    UseKind == StateKind::Other)) {
                UseProofs[UseMI] = UseProof();
                return UseProof();
              }
              if (!ReachingUseKind) {
                ReachingUseKind = UseKind;
                ReachingOtherDef = State.Def;
              } else if (*ReachingUseKind != UseKind ||
                         (UseKind == StateKind::Other &&
                          ReachingOtherDef != State.Def)) {
                UseProofs[UseMI] = UseProof();
                return UseProof();
              }
            }
            EndedAtUse = true;
            break;
          }

          const bool IsParentRef =
              LinxV5::getAssembleParentRef(MI) == Parent;
          bool KillsTrackedParent = false;
          for (const MachineOperand &MO : MI.operands())
            if (MO.isReg() && MO.isUse() && !MO.isImplicit() &&
                MO.getReg() == Parent)
              KillsTrackedParent |= MO.isKill();
          if (IsParentRef && State.Kind == StateKind::Parent) {
            Optional<bool> Last =
                LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
            if (Last && *Last)
              State.Phase = ParentPhase::Closed;
          }

          Optional<std::pair<Register, uint64_t>> KnownDef;
          if (MI.getOpcode() == LinxV5::ORI && MI.getNumOperands() >= 3 &&
              MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
              MI.getOperand(2).isImm()) {
            Optional<uint64_t> Src =
                getConstant(State, MI.getOperand(1).getReg());
            if (Src)
              KnownDef = std::make_pair(
                  MI.getOperand(0).getReg(),
                  *Src | static_cast<uint64_t>(MI.getOperand(2).getImm()));
          } else if (MI.getOpcode() == LinxV5::ADDI &&
                     MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
                     MI.getOperand(1).isReg() && MI.getOperand(2).isImm()) {
            Optional<uint64_t> Src =
                getConstant(State, MI.getOperand(1).getReg());
            if (Src)
              KnownDef = std::make_pair(
                  MI.getOperand(0).getReg(),
                  *Src + static_cast<uint64_t>(MI.getOperand(2).getImm()));
          } else if (MI.getOpcode() == LinxV5::OR_SW &&
                     MI.getNumOperands() >= 4 && MI.getOperand(0).isReg() &&
                     MI.getOperand(1).isReg() && MI.getOperand(2).isReg()) {
            Optional<uint64_t> LHS =
                getConstant(State, MI.getOperand(1).getReg());
            Optional<uint64_t> RHS =
                getConstant(State, MI.getOperand(2).getReg());
            if (LHS && RHS) {
              uint32_t Word = static_cast<uint32_t>(*LHS | *RHS);
              KnownDef = std::make_pair(
                  MI.getOperand(0).getReg(),
                  static_cast<uint64_t>(
                      static_cast<int64_t>(static_cast<int32_t>(Word))));
            }
          }

          Optional<SpillSlotKey> ExactSpillLoad =
              getExactSpillAccess(MI, /*IsLoad=*/true);
          if (ExactSpillLoad && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isReg()) {
            auto It = State.SpillConstants.find(*ExactSpillLoad);
            if (It != State.SpillConstants.end())
              KnownDef = std::make_pair(MI.getOperand(0).getReg(), It->second);
          }
          Optional<SpillSlotKey> ExactSpillStore =
              getExactSpillAccess(MI, /*IsLoad=*/false);
          Optional<uint64_t> SpillStoreValue;
          if (ExactSpillStore && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isReg())
            SpillStoreValue =
                getConstant(State, MI.getOperand(0).getReg());

          bool DefinesCArg = false;
          Optional<bool> NewCArg;
          const bool ModelsCArg =
              MI.getOpcode() == LinxV5::SETC_EQI ||
              MI.getOpcode() == LinxV5::SETC_NEI ||
              MI.getOpcode() == LinxV5::SETC_GEI;
          if (ModelsCArg &&
              MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isImm()) {
            Optional<uint64_t> Src =
                getConstant(State, MI.getOperand(0).getReg());
            if (Src) {
              int64_t Imm = MI.getOperand(1).getImm();
              bool Equal = *Src == static_cast<uint64_t>(Imm);
              if (MI.getOpcode() == LinxV5::SETC_EQI)
                NewCArg = Equal;
              else if (MI.getOpcode() == LinxV5::SETC_NEI)
                NewCArg = !Equal;
              else
                NewCArg = static_cast<int64_t>(*Src) >= Imm;
            }
          }

          if ((MI.getOpcode() == LinxV5::BEQ ||
               MI.getOpcode() == LinxV5::BNE) &&
              MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isReg() && MI.getOperand(2).isMBB()) {
            ConditionalTarget = MI.getOperand(2).getMBB();
            Optional<uint64_t> LHS =
                getConstant(State, MI.getOperand(0).getReg());
            Optional<uint64_t> RHS =
                getConstant(State, MI.getOperand(1).getReg());
            if (LHS && RHS) {
              bool Equal = *LHS == *RHS;
              ConditionalTaken =
                  MI.getOpcode() == LinxV5::BEQ ? Equal : !Equal;
            }
          } else if (MI.getOpcode() == LinxV5::PseudoBRCondCARG &&
                     MI.getNumOperands() >= 1 && MI.getOperand(0).isMBB()) {
            ConditionalTarget = MI.getOperand(0).getMBB();
            ConditionalTaken = CArg;
          }

          invalidateSpillWrites(State, MI);
          if (ExactSpillStore && RelevantSpills.count(*ExactSpillStore) &&
              SpillStoreValue)
            State.SpillConstants[*ExactSpillStore] = *SpillStoreValue;

          for (const MachineOperand &MO : MI.operands()) {
            if (MO.isReg() && MO.isDef()) {
              invalidateAliases(State, MO.getReg());
              if (MO.getReg().isPhysical() &&
                  TRI->regsOverlap(MO.getReg(), LinxV5::CARG))
                DefinesCArg = true;
            } else if (MO.isRegMask()) {
              for (auto It = State.Constants.begin();
                   It != State.Constants.end();) {
                if (MO.clobbersPhysReg(Register(It->first)))
                  It = State.Constants.erase(It);
                else
                  ++It;
              }
              DefinesCArg |= MO.clobbersPhysReg(LinxV5::CARG);
            }
          }
          if (KnownDef)
            if (RelevantRegs.count(KnownDef->first.id()))
              State.Constants[KnownDef->first.id()] = KnownDef->second;
          // SETC is the only modeled producer.  Every other explicit or
          // implicit CARG definition, including a regmask clobber, destroys
          // the previous predicate so a later branch explores both edges.
          if (DefinesCArg)
            CArg = None;
          if (ModelsCArg)
            CArg = NewCArg;

          if (MI.isImplicitDef()) {
            for (const MachineOperand &MO : MI.operands())
              if (MO.isReg() && MO.isDef() && MO.getReg() == Parent) {
                State.Kind = StateKind::ExplicitUndef;
                State.Phase = ParentPhase::None;
                State.Def = nullptr;
                State.Distance = 0;
              }
            continue;
          }
          if (MI.getOpcode() != LinxV5::PseudoEmptyTile) {
            for (const MachineOperand &MO : MI.operands()) {
              if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
                continue;
              Register Def = MO.getReg();
              if (!Def.isPhysical() || !ParentRC->contains(Def))
                continue;
              if (&MI == InitMI && Def == Parent) {
                State.Kind = StateKind::Parent;
                State.Phase = ParentPhase::Open;
                State.Def = InitMI;
                State.Distance = 1;
              } else if (Def == Parent) {
                State.Kind = StateKind::Other;
                State.Phase = ParentPhase::None;
                State.Def = &MI;
                State.Distance = 1;
              } else if (State.Kind == StateKind::Parent) {
                if (++State.Distance > ParentRC->getNumRegs()) {
                  UseProofs[UseMI] = UseProof();
                  return UseProof();
                }
              }
            }
          }
          if (KillsTrackedParent && State.Kind == StateKind::Parent &&
              State.Phase == ParentPhase::Closed) {
            State.Kind = StateKind::Absent;
            State.Phase = ParentPhase::None;
            State.Def = nullptr;
            State.Distance = 0;
          }
        }
        if (EndedAtUse)
          continue;

        for (MachineBasicBlock *Succ : State.MBB->successors()) {
          if (ConditionalTarget && ConditionalTaken) {
            if (*ConditionalTaken && Succ != ConditionalTarget)
              continue;
            if (!*ConditionalTaken && Succ == ConditionalTarget)
              continue;
          }
          PathState Next = State;
          Next.MBB = Succ;
          Worklist.push_back(std::move(Next));
        }
      }
      UseProof Proof;
      Proof.Proven = SawUse;
      Proof.Reached = SawUse;
      Proof.Complete = true;
      if (ReachingParentDistance)
        Proof.Distance = *ReachingParentDistance;
      UseProofs[UseMI] = Proof;
      return Proof;
    };

    auto transfer = [&](MachineBasicBlock &MBB, ParentState State,
                        bool RecordUses) -> ParentState {
      for (MachineInstr &MI : MBB) {
        // Resolve sources against the pre-instruction queue. INIT may reuse
        // the physical register number that still names an older source.
        const bool UsesParentRef =
            LinxV5::getAssembleParentRef(MI) == Parent;
        bool UsesParentRegister = false;
        bool KillsParent = false;
        for (const MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isUse() || MO.isImplicit() ||
              MO.getReg() != Parent)
            continue;
          UsesParentRegister = true;
          KillsParent |= MO.isKill();
        }
        if (RecordUses && UsesParentRef) {
          UseProof Proof = proveUse(&MI, /*RequireOpenParent=*/true);
          ++ParentRefAnalysisCounts[&MI];
          if (!Proof.Complete)
            IncompleteParentRefProofs.insert(&MI);
          if (Proof.Reached && !Proof.Proven)
            ReachedUnprovedParentRefs.insert(&MI);
          if (Proof.Proven) {
            if (++ParentRefProofCounts[&MI] != 1)
              report_fatal_error(
                  "B.ASSEMBLE continuation matches multiple parent epochs");
            AssembleParentUseDistances[&MI][Parent] = Proof.Distance;
          }
        }

        // A TCOPY/TMOV consumes its source as ordinary published tile data.
        // An OPEN parent still names the in-progress assembly generation, so
        // copying it would manufacture a second architectural identity even
        // when the destination uses another physical hand.  A Maybe phase is
        // equally unsafe because at least one predecessor keeps it OPEN.
        if (UsesParentRegister &&
            (MI.getOpcode() == LinxV5::PseudoTCOPY ||
             MI.getOpcode() == LinxV5::PseudoTCOPYDesc) &&
            State.Kind == StateKind::Parent &&
            State.Phase == ParentPhase::Open)
          report_fatal_error(
              "cannot copy an OPEN B.ASSEMBLE parent source before LAST");

        // A physical hand may be reused by several independent assembly
        // generations.  Only the B.ASSEMBLE ParentRef operand continues the
        // generation tracked by this dataflow instance.  Every other use is
        // an ordinary tile source and is legal only after every reaching path
        // has LAST-closed the tracked generation.  This keeps a closed child
        // usable as the writer for its next hierarchy level without mistaking
        // that source for the child's old ParentRef epoch.
        if (RecordUses && UsesParentRegister && !UsesParentRef &&
            !proveUse(&MI, /*RequireOpenParent=*/false).Proven) {
          LLVM_DEBUG(dbgs() << "OPEN assembly parent ordinary use for epoch "
                            << printReg(Parent, TRI) << " defined by "
                            << *InitMI << "at " << MI);
          if (State.Kind == StateKind::Conflict)
            report_fatal_error(
                "B.ASSEMBLE parent has conflicting relative distances at a "
                "CFG join");
          report_fatal_error(
              "cannot use an OPEN B.ASSEMBLE parent as an ordinary tile "
              "source before LAST");
        }

        // LAST consumes the OPEN parent at its pre-state distance and only
        // then publishes it as ordinary data.
        if (LinxV5::getAssembleParentRef(MI) == Parent &&
            State.Kind == StateKind::Parent &&
            State.Phase != ParentPhase::Closed) {
          Optional<bool> Last = LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
          if (Last && *Last)
            State.Phase = ParentPhase::Closed;
        }

        if (MI.isImplicitDef()) {
          for (const MachineOperand &MO : MI.operands()) {
            if (MO.isReg() && MO.isDef() && MO.getReg() == Parent) {
              State = ParentState();
              State.Kind = StateKind::ExplicitUndef;
            }
          }
          continue;
        }

        if (MI.getOpcode() != LinxV5::PseudoEmptyTile) {
          for (const MachineOperand &MO : MI.operands()) {
            if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
              continue;
            Register Def = MO.getReg();
            if (!Def.isPhysical() || !ParentRC->contains(Def))
              continue;

            if (&MI == InitMI && Def == Parent) {
              State.Kind = StateKind::Parent;
              State.Def = InitMI;
              State.Distance = 1;
              State.Phase = ParentPhase::Open;
              continue;
            }

            if (Def == Parent) {
              if (State.Kind == StateKind::Parent &&
                  State.Phase != ParentPhase::Closed && RecordUses)
                report_fatal_error(
                    "OPEN B.ASSEMBLE parent register has a second real "
                    "definition");
              if (State.Kind == StateKind::Parent &&
                  State.Phase == ParentPhase::Closed) {
                State = absentState();
              } else {
                State.Kind = StateKind::Other;
                State.Def = &MI;
                State.Distance = 1;
                State.Phase = ParentPhase::None;
              }
              continue;
            }

            if (State.Kind == StateKind::Parent) {
              if (State.Kind == StateKind::Parent &&
                  State.Phase != ParentPhase::Closed && RecordUses)
                report_fatal_error(
                    "OPEN B.ASSEMBLE parent hand contains another real tile "
                    "definition after CFG copy insertion");
              if (++State.Distance > ParentRC->getNumRegs()) {
                SawOutOfWindow = true;
                return conflictState();
              }
            }
          }
        }
        if (KillsParent && State.Kind == StateKind::Parent &&
            State.Phase == ParentPhase::Closed)
          State = absentState();
      }
      return State;
    };

    bool Changed = true;
    unsigned Iterations = 0;
    while (Changed) {
      if (++Iterations > MF.size() * (ParentRC->getNumRegs() + 2))
        report_fatal_error(
            "B.ASSEMBLE parent relative-distance dataflow did not converge");
      Changed = false;
      for (MachineBasicBlock &MBB : MF) {
        ParentState Joined = MBB.pred_empty() ? absentState() : unknownState();
        for (MachineBasicBlock *Pred : MBB.predecessors()) {
          ParentState PredState = OutState[Pred];
          if (PredState.Kind == StateKind::Unknown)
            continue;
          if (PredState.Kind == StateKind::ExplicitUndef) {
            if (Joined.Kind == StateKind::Unknown)
              Joined = PredState;
            continue;
          }
          if (Joined.Kind == StateKind::Unknown ||
              Joined.Kind == StateKind::ExplicitUndef)
            Joined = PredState;
          else if (Joined.Kind == StateKind::Parent &&
                   PredState.Kind == StateKind::Parent &&
                   Joined.Def == PredState.Def &&
                   Joined.Distance == PredState.Distance &&
                   Joined.Phase != PredState.Phase)
            Joined.Phase = ParentPhase::Maybe;
          else if (!sameState(Joined, PredState))
            Joined = conflictState();
        }
        ParentState NewOut = transfer(MBB, Joined, /*RecordUses=*/false);
        if (!sameState(InState[&MBB], Joined) ||
            !sameState(OutState[&MBB], NewOut)) {
          InState[&MBB] = Joined;
          OutState[&MBB] = NewOut;
          Changed = true;
        }
      }
    }

    for (MachineBasicBlock &MBB : MF) {
      if (InState[&MBB].Kind == StateKind::Unknown ||
          OutState[&MBB].Kind == StateKind::Unknown)
        report_fatal_error(
            "B.ASSEMBLE parent relative-distance dataflow has unresolved "
            "CFG state");
      if (SawOutOfWindow)
        report_fatal_error(
            "B.ASSEMBLE parent relative distance exceeds the 16-entry "
            "Local tile window");
    }
    for (MachineBasicBlock &MBB : MF)
      (void)transfer(MBB, InState[&MBB], /*RecordUses=*/true);
  }

  for (const MachineInstr *MI : ParentRefUses) {
    unsigned ProofCount = ParentRefProofCounts.lookup(MI);
    Register ParentRef = LinxV5::getAssembleParentRef(*MI);
    unsigned CandidateEpochCount = ParentEpochCounts.lookup(ParentRef.id());
    if (ProofCount == 1)
      continue;
    // A scalar proof may establish that a syntactic continuation is
    // unreachable for every parent epoch.  It still needs a legal encoded
    // ParentRef operand, but its architectural distance is unobservable.
    // Use the nearest offset only after every bounded analysis completed
    // without reaching the instruction.
    if (ProofCount == 0 && CandidateEpochCount != 0 &&
        ParentRefAnalysisCounts.lookup(MI) == CandidateEpochCount &&
        !IncompleteParentRefProofs.count(MI) &&
        !ReachedUnprovedParentRefs.count(MI)) {
      AssembleParentUseDistances[MI][ParentRef] = 1;
      continue;
    }
    LLVM_DEBUG(dbgs() << "unproved B.ASSEMBLE continuation: " << *MI);
    LLVM_DEBUG(dbgs() << "proofs " << ProofCount << " analyses "
                      << ParentRefAnalysisCounts.lookup(MI) << " of "
                      << CandidateEpochCount << " candidate epochs, "
                      << ParentEpochs.size() << " total, incomplete "
                      << IncompleteParentRefProofs.count(MI) << " reached "
                      << ReachedUnprovedParentRefs.count(MI) << "\n");
    report_fatal_error(
        "B.ASSEMBLE continuation requires one exact OPEN parent epoch at "
        "a uniform relative distance");
  }
}

bool LinxV5TRegToOffsetOpt::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());
  TRI = static_cast<const LinxV5RegisterInfo *>(MF.getSubtarget().getRegisterInfo());
  MRI = &MF.getRegInfo();
  SyncConflictRCs.clear();
  PinnedAssembleParentRCs.clear();

#ifndef NDEBUG
  verifyGPR(MF);
#else
  LLVM_DEBUG(verifyGPR(MF));
#endif

  RCsInfo = {
      // Register Class,
      // {Register Output,  Register Base, Register Reserve}
      // Vec Reg
      {&LinxV5::SIMT_VTRRegClass,
       {LinxV5::SIMT_VT, LinxV5::SIMT_OSVT1, LinxV5::SIMT_VT4}},
      {&LinxV5::SIMT_VURRegClass,
       {LinxV5::SIMT_VU, LinxV5::SIMT_OSVU1, LinxV5::SIMT_VU4}},
      {&LinxV5::SIMT_VMRRegClass,
       {LinxV5::SIMT_VM, LinxV5::SIMT_OSVM1, LinxV5::SIMT_VM4}},
      {&LinxV5::SIMT_VNRRegClass,
       {LinxV5::SIMT_VN, LinxV5::SIMT_OSVN1, LinxV5::SIMT_VN4}},
      // Tile Reg
      {&LinxV5::Tile_TRRegClass,
       {LinxV5::Tile_T, LinxV5::Tile_TOS1, LinxV5::Tile_T8}},
      {&LinxV5::Tile_URRegClass,
       {LinxV5::Tile_U, LinxV5::Tile_UOS1, LinxV5::Tile_U8}},
      {&LinxV5::Tile_MRRegClass,
       {LinxV5::Tile_M, LinxV5::Tile_MOS1, LinxV5::Tile_M8}},
      {&LinxV5::Tile_NRRegClass,
       {LinxV5::Tile_N, LinxV5::Tile_NOS1, LinxV5::Tile_N8}},
      {&LinxV5::TILE_ABS_ACCRegClass,
       {LinxV5::Tile_ACC, LinxV5::Tile_ACCOS1, LinxV5::Tile_ACCOS1}},
      // Scalar Reg
      {&LinxV5::LTRRegClass, {LinxV5::T, LinxV5::TOS1, LinxV5::T4}},
      {&LinxV5::LURRegClass, {LinxV5::U, LinxV5::UOS1, LinxV5::U4}},
  };

  // Parent lifetime tracking is sound only when both assembly controls are
  // positional immediate operands.  Reject unresolved controls before Phase
  // 1 can roll or copy the Local tile queue and lose the parent identity.
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isInlineAsm() ||
          !StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
               .contains("B.ASSEMBLE"))
        continue;
      if (!LinxV5::getAssembleControlBit(MI, /*InitIdx=*/0) ||
          !LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1))
        report_fatal_error(
            "B.ASSEMBLE Init/Last controls must be immediate operands");
    }
  }

  // OPEN assembly parents have already been assigned dedicated physical
  // hands by Clockhands.  Discover those hands before Phase 1 so CFG queue
  // synchronization cannot manufacture a COPY of an in-progress parent.
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isInlineAsm() ||
          !StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
               .contains("B.ASSEMBLE"))
        continue;
      Optional<bool> Last = LinxV5::getAssembleControlBit(MI, /*LastIdx=*/1);
      if (!Last || *Last)
        continue;
      for (const MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isDef() || !MO.getReg().isPhysical())
          continue;
        for (const TargetRegisterClass *RC :
             {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
              &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
          if (RC->contains(MO.getReg())) {
            PinnedAssembleParentRCs.insert(RC);
            break;
          }
        }
      }
    }
  }

  unsigned OldNumCopy = NumCopy;

  LLVM_DEBUG(dbgs() << "before insert copy:\n"; MF.dump());

  // PHI elimination may coalesce all incoming Tile values to one physical
  // register without inserting a COPY in the join block. Audit those direct
  // predecessor definitions before queue rewriting: otherwise each arm can
  // establish a different dtype/layout/shape for the same live-in handle and
  // the chosen runtime descriptor becomes path-dependent.
  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_size() < 2)
      continue;
    for (const auto &LiveIn : MBB.liveins()) {
      Register Reg = LiveIn.PhysReg;
      if (!LinxV5::Tile_ABSRegClass.contains(Reg))
        continue;
      bool EveryPredecessorDefines = llvm::all_of(
          MBB.predecessors(), [&](const MachineBasicBlock *Pred) {
            return llvm::any_of(*Pred, [&](const MachineInstr &MI) {
              return MI.definesRegister(Reg);
            });
          });
      if (!EveryPredecessorDefines)
        continue;
      Optional<LinxV5::TileDescriptor> Descriptor =
          LinxV5::tryGetTileRegDescriptor(MBB, MBB.begin(), Reg);
      if (!Descriptor)
        report_fatal_error(
            "unknown Local tile descriptor at COPY/PHI control-flow join");
    }
  }

  // Phase-1: Insert Copy to satify register offset index.
  updateGlobalSyncup(MF);
  for (MachineBasicBlock &MBB : MF)
    TBDoneMBBs.push_back(&MBB);
  while (!TBDoneMBBs.empty()) {
    MachineBasicBlock *MBB = getNextBlock();
    LLVM_DEBUG(dbgs() << "Phase-1 Order: " << MBB->getName() << "\n");
    insertCopy(*MBB);
    llvm::erase_value(TBDoneMBBs, MBB);
    DoneMBBs.push_back(MBB);
  }

  // Clockhands reserves an OPEN parent's hand before register allocation.
  // Validate the final Phase-1 machine function, including every inserted
  // copy, before relying on that isolation in the relative-window rewrite.
  solveAssembleParentDataflow(MF);

  LLVM_DEBUG(dbgs() << "copys " << NumCopy - OldNumCopy << " total "
                    << MF.getInstructionCount() << "\n");
  LLVM_DEBUG(dbgs() << "after insert copy:\n"; MF.dump());

  legalityCheck(MF);

  // So that we can use absolute register for LLVM-MCA to build DEF-USE Chain.
  if (!EnableReg2Offset)
    return true;

  // Phase-2: Rewrite to offset register for codeGen.
  DoneMBBs.clear();
  for (MachineBasicBlock &MBB : MF)
    TBDoneMBBs.push_back(&MBB);
  while (!TBDoneMBBs.empty()) {
    MachineBasicBlock *MBB = getNextBlock();
    rewriteMBB(*MBB);
    llvm::erase_value(TBDoneMBBs, MBB);
    DoneMBBs.push_back(MBB);
  }

  return true;
}

bool hasImplicitDef(DenseMap<Register, unsigned int> &TRegIdx) {
  for (auto &RI : TRegIdx) {
    if (RI.second == -1)
      return true;
  }
  return false;
}

void LinxV5TRegToOffsetOpt::rewriteMBB(MachineBasicBlock &MBB) {
  for (auto RCInfo : RCsInfo) {
    const TargetRegisterClass *RC = RCInfo.first;
    DenseMap<Register, unsigned int> TRegIdx;
    unsigned CurIdx = 0;
    if (SyncConflictRCs.count(RC)) {
      // Keep ordinary values absolute when queue synchronization is disabled,
      // but an OPEN assembly ParentRef is architecturally relative and has a
      // separately proved reaching-INIT distance. Rewrite only those exact
      // uses; solveAssembleParentDataflow already rejected unresolved or
      // conflicting join distances.
      Register OffsetBase = RCInfo.second[1];
      for (MachineInstr &MI : MBB) {
        auto UseIt = AssembleParentUseDistances.find(&MI);
        if (UseIt == AssembleParentUseDistances.end())
          continue;
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isUse() || MO.isImplicit())
            continue;
          if (!RC->contains(MO.getReg()))
            continue;
          auto DistanceIt = UseIt->second.find(MO.getReg());
          if (DistanceIt == UseIt->second.end())
            continue;
          unsigned Distance = DistanceIt->second;
          if (Distance < 1 || Distance > RC->getNumRegs())
            report_fatal_error(
                "B.ASSEMBLE parent relative distance exceeds the Local tile "
                "window");
          MO.setReg(OffsetBase + (Distance - 1));
        }
      }
      LLVM_DEBUG(dbgs() << "skip rewrite for "
                        << TRI->getRegClassName(RC)
                        << ": absolute values with solved ParentRef uses\n");
      continue;
    }
    LLVM_DEBUG(dbgs() << "rewrite " << TRI->getRegClassName(RC) << " in "
                      << printMBBReference(MBB) << "\n");
    if (true) {
      if (MBB.pred_empty()) {
        LLVM_DEBUG(dbgs() << "no pred\n");
        rewriteMBB(MBB, TRegIdx, CurIdx, RC, RCInfo.second[0],
                   RCInfo.second[1]);
      } else {
        // if we have multiple predecessors, choose the one that has no implicit_def
        bool picked = false;
        for (MachineBasicBlock *Pred : MBB.predecessors()) {
          if (llvm::find(DoneMBBs, Pred) == DoneMBBs.end())
            continue;
          TRegIdx = RewriteTRDefInfo[Pred][RC].first;
          CurIdx = RewriteTRDefInfo[Pred][RC].second;
          LLVM_DEBUG(dbgs()
                     << "pred " << printMBBReference(*Pred) << " TRegIdx: [");
          for (auto &RI : TRegIdx) {
            LLVM_DEBUG(dbgs()
                       << printReg(RI.first, TRI) << "(" << RI.second << ") ");
          }
          LLVM_DEBUG(dbgs() << "]\n");
          if (hasImplicitDef(TRegIdx)) {
            continue;
          } else {
            LLVM_DEBUG(dbgs()
                       << "pick pred " << printMBBReference(*Pred) << "\n");
            picked = true;
            break;
          }
        }
        if (!picked && MBB.pred_size() == 1 && *MBB.pred_begin() == &MBB)
          picked = true;
        if (!picked) {
          // No done pred found; pick first available pred to avoid crash
          // on complex CFGs (e.g. coalescer-generated blocks).
          for (MachineBasicBlock *Pred : MBB.predecessors()) {
            TRegIdx = RewriteTRDefInfo[Pred][RC].first;
            CurIdx = RewriteTRDefInfo[Pred][RC].second;
            picked = true;
            break;
          }
        }
        if (!picked) {
          rewriteMBB(MBB, TRegIdx, CurIdx, RC, RCInfo.second[0],
                     RCInfo.second[1]);
          continue;
        }
        (void)picked;
        rewriteMBB(MBB, TRegIdx, CurIdx, RC, RCInfo.second[0],
                   RCInfo.second[1]);
      }
      RewriteTRDefInfo[&MBB][RC] = std::make_pair(TRegIdx, CurIdx);
    } else {
      rewriteMBB(MBB, TRegIdx, CurIdx, RC, RCInfo.second[0], RCInfo.second[1]);
    }
  }
}

void LinxV5TRegToOffsetOpt::rewriteMBB(
    MachineBasicBlock &MBB, DenseMap<Register, unsigned int> &TRegIdx,
    unsigned &CurIdx, const TargetRegisterClass *RC, Register Out,
    Register OffsetBase) {
  LLVM_DEBUG(dbgs() << ">> preform rewrite \n");

  for (MachineInstr &MI : MBB) {
    if (MI.isKill()) {
      Register Reg = MI.getOperand(0).getReg();
      if (isRC(Reg, RC, MRI))
        TRegIdx.erase(Reg);
      continue;
    }

    if (canSkipMI(MI))
      continue;

    // PseudoEmptyTile lowers to a zero-mask NoPE bundle.  It does not allocate
    // a Local tile, so it must not advance the physical relative-window index.
    // Phase 1 may retain it as a CFG bookkeeping marker, but Phase 2 tracks
    // only allocations that exist in the architectural queue.
    if (MI.getOpcode() == LinxV5::PseudoEmptyTile)
      continue;

    // inherit implicit-def until over-defined
    if (MI.isImplicitDef()) {
      Register Reg = MI.getOperand(0).getReg();
      if (isRC(Reg, RC, MRI))
        TRegIdx[Reg] = -1;
      continue;
    }

    LLVM_DEBUG(dbgs() << "   rewrite " << MI);
    unsigned Occ = getTSlotOccupation(MI, RC);
    SmallVector<MachineOperand *, 2> TRDefs;
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg()) {
        continue;
      } else if (MO.isUse()) {
        if (isRC(MO.getReg(), RC, MRI)) {
          Register OriginalReg = MO.getReg();
          auto UseIt = AssembleParentUseDistances.find(&MI);
          if (UseIt != AssembleParentUseDistances.end()) {
            auto DistanceIt = UseIt->second.find(OriginalReg);
            if (DistanceIt != UseIt->second.end()) {
              unsigned Distance = DistanceIt->second;
              assert(Distance >= 1 && Distance <= RC->getNumRegs());
              MO.setReg(OffsetBase + (Distance - 1));
              continue;
            }
          }
          if (!TRegIdx.count(MO.getReg())) {
            // After coalescer, some tile vregs may be used without a prior
            // def in this block's tracked TRegIdx (e.g. live-in from a
            // block we couldn't pick a pred for). Ordinary uses retain their
            // absolute register.  OPEN assembly parents are never allowed to
            // fall back: their use must have a solved reaching-INIT distance.
            if (LinxV5::getAssembleParentRef(MI) == OriginalReg)
              report_fatal_error(
                  "OPEN B.ASSEMBLE parent use lacks solved relative distance");
            continue;
          }
          int64_t Index = CurIdx - TRegIdx[MO.getReg()];
          LLVM_DEBUG(dbgs()
                     << "   " << printReg(MO.getReg(), TRI) << " CurIdx "
                     << CurIdx << " LastIdx " << TRegIdx[MO.getReg()] << "\n");
          assert(Index <= RC->getNumRegs());
          Register NewReg = OffsetBase + (Index - 1);
          MO.setReg(NewReg);
        }
      } else if (MO.isDef() && isRC(MO.getReg(), RC, MRI)) {
        TRDefs.push_back(&MO);
      }
    }

    for (auto *Def : TRDefs) {
      TRegIdx[Def->getReg()] = CurIdx;
      LLVM_DEBUG(dbgs() << "   setIdx " << printReg(Def->getReg(), TRI) << " "
                        << CurIdx << "\n");
      Def->setReg(Out);
      CurIdx++;
    }
  }
}
