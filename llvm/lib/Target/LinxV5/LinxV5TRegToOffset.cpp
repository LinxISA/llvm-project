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
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"

#define DEBUG_TYPE "linxv5-treg-to-offset"
#define LINX_TREG_TO_OFFSET_NAME "LinxV5 Treg to Offset"

using namespace llvm;

STATISTIC(NumCopy, "number copys inserted");
STATISTIC(TileLongCopy, "Number jcore-tile long live range copys");
STATISTIC(TileCFGCopy, "Number jcore-tile control flow copys");

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
  TRLiveRange(MachineInstr *I)
      : MI(I), LastUse(nullptr), DefMO(nullptr), DefIdx(-1u), IsLiveout(false) {
  }
};

struct TRCopyRange {
  LinxRegOp RegOp;
  MachineInstr *LastUse;
  bool IsLiveout;
  size_t Idx;
  TRCopyRange() : LastUse(nullptr), IsLiveout(false), Idx(0) {}
  TRCopyRange(LinxRegOp &RO, bool liveout)
      : RegOp(RO), LastUse(nullptr), IsLiveout(liveout), Idx(0) {}
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
  Slot->LastUse = nullptr;
}

static unsigned getTSlotOccupation(MachineInstr &MI,
                                   const TargetRegisterClass *RC) {
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
  void BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp);
  void SlotCopy(MachineBasicBlock::iterator Before, TRCopyRange &Slot);
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

void SlotCalc::BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp) {
  Register TR = RegOp.Reg;

  if (Before != MBB->begin()) {
    while (std::prev(Before)->getOpcode() == LinxV5::PseudoLABEL ||
           std::prev(Before)->isLabel())
      --Before;
  }

  if (TR == LinxV5::NoRegister) {
    if (IsTileRC) {
      BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::PseudoEmptyTile))
          .addReg(ReserveReg, RegState::Define | RegState::Dead)
          .addImm(16);
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
        unsigned RegSize = 1 << (RegSizeCode + 4);
        BuildMI(*MBB, Before, DebugLoc(), TII->get(LinxV5::PseudoTCOPY))
            .addReg(TR, RegState::Define)
            .addImm(RegSizeCode)
            .addReg(TR, RegState::Kill);
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
  return;
}

void SlotCalc::PreAlloc(TRCopyRange Slot) {
  pre() = Slot;
  pre().Idx = PreIdx;
  expire();
  prealloc();
}

void SlotCalc::SlotCopy(MachineBasicBlock::iterator Before, TRCopyRange &Slot) {
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
    BuildCopy(Before, head().RegOp);
    advance();
  }

  BuildCopy(Before, Slot.RegOp);
  head() = Slot;
  head().Idx = HeadIdx;
  if (&Slot != &head())
    initSlot(&Slot);
  if (&tail() != &head()) {
    initSlot(&tail());
  }
  shift();
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
    SlotCopy(MI, *Slot);
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
      Register Reg = Slot.IsLiveout ? Slot.RegOp.Reg : LinxV5::NoRegister;
      CurLiveouts.push_back(LinxRegOp(Reg, Slot.RegOp.Size));
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
  SlotCopy(EndI, tmp);
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
        if (LRs[i]->LastUse != nullptr || LRs[i]->IsLiveout) {
          NewSlots[i]->RegOp.Reg = LRs[i]->DefMO->getReg();
        }
      }
      LLVM_DEBUG(dbgs() << "emit " << *OI.MI);
    } else {
      ++OII;
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
      SlotCopy(EndI, RR);
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
          LLVM_DEBUG(dbgs() << MBB << "\n");
          LLVM_DEBUG(dbgs() << "bgpr multi set inst: " << MI << "\n");
          assert(0 && "BGPR multi set!");
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

  // TODO: Add legality check for all SlotStatus.
  // All Done MBB liveouts SlotStatus should be same.

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
      if (ResRegOp.Reg == LinxV5::NoRegister)
        Res[MaxSize - 1 - i] = CurRegOp;
      else
        assert(ResRegOp == CurRegOp && "Liveouts sync-up error!");
      HasLiveoutLimit = true;
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
    SC.insertCopys(OriginalInstrs, RCsInfo[RC][2],
                   getInitStatus(MBB, LiveInsLastUse, RestLiveouts, RC),
                   isInSyncGroup(&MBB), getLiveoutLimits(MBB, RC), ResultSS);
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

bool LinxV5TRegToOffsetOpt::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());
  TRI = static_cast<const LinxV5RegisterInfo *>(MF.getSubtarget().getRegisterInfo());
  MRI = &MF.getRegInfo();

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

  unsigned OldNumCopy = NumCopy;

  LLVM_DEBUG(dbgs() << "before insert copy:\n"; MF.dump());

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
          // if function entry block do self loop. it do not have done
          // preds, but its pred's status is known empty, so skip the
          // check.
          //
          // func:
          // bb.1:
          //    ...
          //    brcond bb.1
          picked = true;
        (void)picked;
        assert(picked && "the block does not have a legal pred?");
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
          assert(TRegIdx.count(MO.getReg()) &&
                 "a t-register used but not defined");
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
