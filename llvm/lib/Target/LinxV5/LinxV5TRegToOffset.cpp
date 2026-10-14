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
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"
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
  bool operator==(const LinxRegOp &Other) const {
    if (Reg != Other.Reg || Size != Other.Size ||
        Descriptor.hasValue() != Other.Descriptor.hasValue())
      return false;
    return !Descriptor || *Descriptor == *Other.Descriptor;
  }
};

using TRegIndexState =
    SmallVector<std::pair<LinxRegOp, unsigned int>, 16>;

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
  MachineInstr *AssembleParentDef;
  bool IsLiveout;
  bool IsAssembleParent;
  bool IsLocalTileABIImport;
  size_t Idx;
  TRCopyRange()
      : LastUse(nullptr), AssembleParentDef(nullptr), IsLiveout(false),
        IsAssembleParent(false), IsLocalTileABIImport(false), Idx(0) {}
  TRCopyRange(LinxRegOp &RO, bool liveout)
      : RegOp(RO), LastUse(nullptr), AssembleParentDef(nullptr),
        IsLiveout(liveout), IsAssembleParent(false),
        IsLocalTileABIImport(false), Idx(0) {}
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

struct QueueContractEntry {
  LinxRegOp Value;
  unsigned Age;

  bool operator==(const QueueContractEntry &Other) const {
    return Value == Other.Value && Age == Other.Age;
  }
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
                    std::pair<TRegIndexState, int>>>
      RewriteTRDefInfo;
  DenseMap<const MachineOperand *, LinxRegOp> RewriteTRIdentities;
  DenseMap<const MachineOperand *, unsigned> QueueCopyUseDistances;
  DenseSet<const MachineOperand *> QueueDeadPublicationDefs;
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *,
                    SmallVector<QueueContractEntry, 8>>>
      CanonicalEntryContracts;
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *, DenseSet<Register>>>
      CanonicalEntryLiveIns;
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *,
                    std::pair<TRegIndexState, unsigned>>>
      CanonicalRewriteEntryStates;
  // Issue #60: register classes whose done-successor liveout queues cannot be
  // tail-aligned unified (e.g. an unrolled if-select where each arm defines a
  // different tile). Phase-1 copy insertion stays correct for them, but the
  // Phase-2 offset rewrite assumes every predecessor leaves the window in the
  // same state. Local Tile classes must be normalized and certified before
  // rewrite; only legacy non-Tile classes may retain absolute registers.
  DenseSet<const TargetRegisterClass *> SyncConflictRCs;
  DenseSet<const TargetRegisterClass *> PureAssembleRCs;
  // Exact post-RA INIT definitions for OPEN B.ASSEMBLE parents.  Phase 1 may
  // insert copies, so this map is populated only after copy insertion and
  // after validating that the reserved hand contains no other allocation.
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      AssembleParentUseDistances;
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      AssembleOrdinaryUseDistances;
  DenseMap<const MachineBasicBlock *, DenseSet<const MachineInstr *>>
      ClosedAssembleParentsAtTail;

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
    RewriteTRIdentities.clear();
    QueueCopyUseDistances.clear();
    QueueDeadPublicationDefs.clear();
    CanonicalEntryContracts.clear();
    CanonicalEntryLiveIns.clear();
    CanonicalRewriteEntryStates.clear();
    SyncConflictRCs.clear();
    PureAssembleRCs.clear();
    AssembleParentUseDistances.clear();
    AssembleOrdinaryUseDistances.clear();
    ClosedAssembleParentsAtTail.clear();
  }

  // Phase-1
  bool isInSyncGroup(MachineBasicBlock *MBB);
  void updateGlobalSyncup(MachineFunction &MF);
  MachineBasicBlock *getNextBlock();
  SmallVector<LinxRegOp> getLiveoutLimits(MachineBasicBlock &MBB,
                                          const TargetRegisterClass *RC);
  SmallVector<QueueContractEntry, 8>
  buildEntryContract(MachineBasicBlock &MBB, const TargetRegisterClass *RC,
                     ArrayRef<LinxRegOp> LiveOuts);
  void normalizeEntryContracts(MachineFunction &MF);
  void rebuildCanonicalEntryStates(MachineFunction &MF);
  void appendQueueFillers(MachineBasicBlock &MBB,
                          const TargetRegisterClass *RC, unsigned Count);
  void certifyEntryContracts(MachineFunction &MF);
  void simulateAndValidateQueues(MachineFunction &MF);
  SlotStatus *getInitStatus(MachineBasicBlock &MBB,
                            DenseMap<Register, MachineInstr *> LiveInsLastUse,
                            DenseSet<Register> RestLiveouts,
                            const TargetRegisterClass *RC);
  void insertCopy(MachineBasicBlock &MBB);

  void legalityCheck(MachineFunction &MF);
  void solveAssembleParentDataflow(MachineFunction &MF,
                                   bool CollectClosedTailProofs = false);

  // Phase-2
  void rewriteMBB(MachineBasicBlock &MBB);
  void rewriteMBB(MachineBasicBlock &MBB,
                  TRegIndexState &TRegIdx, unsigned &CurIdx,
                  const TargetRegisterClass *RC, Register Out,
                  Register OffsetBase);
};
} // namespace llvm

static Optional<LinxRegOp>
getTileQueueIdentity(MachineBasicBlock &MBB, MachineInstr &MI,
                     const MachineOperand &MO, bool IsDef);
static TRegIndexState::iterator findExactIdentity(TRegIndexState &State,
                                                   const LinxRegOp &Identity);
static TRegIndexState::iterator findLatestRegister(TRegIndexState &State,
                                                   Register Reg);
static void eraseRegisterIdentities(TRegIndexState &State, Register Reg);
static void resetStateAtLocalTileABISeed(
    MachineInstr &MI, const TargetRegisterClass *RC,
    TRegIndexState &State, unsigned &CurIdx,
    const MachineRegisterInfo *MRI);

static Optional<LinxV5::TileDescriptor>
getLocalTileABISeedDescriptor(const MachineInstr &MI) {
  if (MI.getOpcode() != LinxV5::PseudoLocalTileABISeed ||
      MI.getNumOperands() < 7)
    return None;
  for (unsigned I : {1u, 2u, 3u, 4u, 5u, 6u})
    if (!MI.getOperand(I).isImm())
      return None;
  return LinxV5::TileDescriptor{
      static_cast<unsigned>(MI.getOperand(5).getImm()),
      static_cast<unsigned>(MI.getOperand(6).getImm()),
      static_cast<unsigned>(MI.getOperand(3).getImm()),
      static_cast<unsigned>(MI.getOperand(2).getImm()),
      static_cast<unsigned>(MI.getOperand(4).getImm()),
      static_cast<unsigned>(MI.getOperand(1).getImm()), false};
}

static Function *getLocalTileABICallee(MachineFunction &MF,
                                       const MachineInstr &MI) {
  if (!MI.isCall())
    return nullptr;
  for (const MachineOperand &MO : MI.operands()) {
    StringRef Name;
    if (MO.isGlobal())
      Name = MO.getGlobal()->getName();
    else if (MO.isSymbol())
      Name = MO.getSymbolName();
    if (Name.empty())
      continue;
    Function *F = MF.getMMI().getModule()->getFunction(Name);
    if (F && LinxV5::getLocalTileABIInfo(*F))
      return F;
  }
  return nullptr;
}

static void addLocalTileABISeed(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator At,
                                const LinxV5InstrInfo &TII,
                                const LinxV5::TileDescriptor &Descriptor) {
  BuildMI(MBB, At, DebugLoc(), TII.get(LinxV5::PseudoLocalTileABISeed))
      .addReg(LinxV5::Tile_T1, RegState::Define)
      .addImm(Descriptor.SizeCode)
      .addImm(Descriptor.ValidCol)
      .addImm(Descriptor.ValidRow)
      .addImm(Descriptor.PhysicalCol)
      .addImm(Descriptor.DataType)
      .addImm(Descriptor.Layout);
}

static void addLocalTileABIExport(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator At,
                                  const LinxV5InstrInfo &TII,
                                  const LinxV5::TileDescriptor &Descriptor) {
  Optional<LinxV5::TileDescriptor> Actual =
      LinxV5::tryGetTileRegDescriptor(MBB, At, LinxV5::Tile_T1);
  if (!Actual || !(*Actual == Descriptor))
    report_fatal_error(
        "linx-local-tile-abi export source descriptor does not match the "
        "declared dtype/layout/capacity/valid shape");
  // This is intentionally a self-copy. Even if register allocation already
  // chose the ABI hand, the call boundary must publish one newest queue value.
  BuildMI(MBB, At, DebugLoc(), TII.get(LinxV5::PseudoTCOPYDesc))
      .addReg(LinxV5::Tile_T1, RegState::Define)
      .addImm(Descriptor.SizeCode)
      .addReg(LinxV5::Tile_T1, RegState::Kill)
      .addImm(Descriptor.ValidCol)
      .addImm(Descriptor.ValidRow)
      .addImm(Descriptor.PhysicalCol)
      .addImm(Descriptor.DataType)
      .addImm(Descriptor.Layout);
}

static void verifyLocalTileABICallLiveness(MachineFunction &MF,
                                           const TargetRegisterInfo &TRI) {
  auto RejectOtherLocal = [&](const LivePhysRegs &Live, StringRef Where) {
    for (MCPhysReg Reg : LinxV5::Tile_ABSRegClass) {
      if (Reg == LinxV5::Tile_T1 || !Live.contains(Reg))
        continue;
      report_fatal_error(Twine("linx-local-tile-abi has another live Local ") +
                         "Tile " + Where);
    }
  };

  for (MachineBasicBlock &MBB : MF) {
    LivePhysRegs Live(TRI);
    Live.addLiveOuts(MBB);
    for (MachineInstr &MI : reverse(MBB)) {
      const bool IsABICall = getLocalTileABICallee(MF, MI) != nullptr;
      if (IsABICall)
        RejectOtherLocal(Live, "after call");
      Live.stepBackward(MI);
      if (IsABICall)
        RejectOtherLocal(Live, "before call");
    }
  }
}

static void prepareLocalTileABIBoundaries(MachineFunction &MF,
                                          const LinxV5InstrInfo &TII,
                                          const TargetRegisterInfo &TRI) {
  verifyLocalTileABICallLiveness(MF, TRI);

  if (Optional<LinxV5::LocalTileABIInfo> ABI =
          LinxV5::getLocalTileABIInfo(MF.getFunction())) {
    MachineBasicBlock &Entry = MF.front();
    addLocalTileABISeed(Entry, Entry.begin(), TII, ABI->Descriptor);
  }

  for (MachineBasicBlock &MBB : MF) {
    for (auto I = MBB.begin(); I != MBB.end();) {
      MachineInstr &MI = *I++;
      if (Function *Callee = getLocalTileABICallee(MF, MI)) {
        LinxV5::LocalTileABIInfo ABI =
            *LinxV5::getLocalTileABIInfo(*Callee);
        addLocalTileABIExport(MBB, MI.getIterator(), TII, ABI.Descriptor);
        if (!MI.readsRegister(LinxV5::Tile_T1, &TRI))
          MI.addOperand(MF, MachineOperand::CreateReg(
                                LinxV5::Tile_T1, /*isDef=*/false,
                                /*isImp=*/true));
        if (!MI.definesRegister(LinxV5::Tile_T1, &TRI))
          MI.addOperand(MF, MachineOperand::CreateReg(
                                LinxV5::Tile_T1, /*isDef=*/true,
                                /*isImp=*/true));
        addLocalTileABISeed(MBB, I, TII, ABI.Descriptor);
      }
    }
  }

  if (Optional<LinxV5::LocalTileABIInfo> ABI =
          LinxV5::getLocalTileABIInfo(MF.getFunction()))
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : make_early_inc_range(MBB))
        if (MI.isReturn()) {
          auto ExportAt = MI.getIterator();
          auto TargetSetup = ExportAt;
          while (TargetSetup != MBB.begin()) {
            --TargetSetup;
            if (TargetSetup->isDebugInstr() || TargetSetup->isCFIInstruction())
              continue;
            break;
          }
          if (TargetSetup == ExportAt ||
              (TargetSetup->getOpcode() != LinxV5::C_SETC_TGT &&
               TargetSetup->getOpcode() != LinxV5::SETC_TGT))
            report_fatal_error(
                "linx-local-tile-abi return lacks adjacent target setup");
          // A Tile export starts a new architectural block.  Emit it before
          // SETC.TGT so the return target is established in the final block
          // immediately consumed by RET, rather than being cleared by TMOV.
          addLocalTileABIExport(MBB, TargetSetup, TII, ABI->Descriptor);
        }
}

static void eraseLocalTileABISeeds(MachineFunction &MF) {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : make_early_inc_range(MBB))
      if (MI.getOpcode() == LinxV5::PseudoLocalTileABISeed)
        MI.eraseFromParent();
}

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

static bool hasReachingExplicitUndef(MachineBasicBlock &MBB, Register Reg,
                                     DenseSet<MachineBasicBlock *> &Visited) {
  if (!Visited.insert(&MBB).second)
    return false;
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    const MachineInstr *Def = nullptr;
    for (auto I = Pred->rbegin(), E = Pred->rend(); I != E; ++I) {
      if (I->definesRegister(Reg)) {
        Def = &*I;
        break;
      }
    }
    if (Def && Def->isImplicitDef())
      return true;
    if (!Def && hasReachingExplicitUndef(*Pred, Reg, Visited))
      return true;
  }
  return false;
}

static bool hasAnyReachingExplicitUndef(MachineBasicBlock &MBB,
                                        Register Reg) {
  DenseSet<MachineBasicBlock *> Visited;
  return hasReachingExplicitUndef(MBB, Reg, Visited);
}

static bool isOpenAssembleParentRegister(MachineFunction &MF, Register Reg) {
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      Optional<bool> Init = LinxV5::getAssembleControlBit(MI, 0);
      Optional<bool> Last = LinxV5::getAssembleControlBit(MI, 1);
      if (!Init || !*Init || !Last || *Last)
        continue;
      for (const MachineOperand &MO : MI.operands())
        if (MO.isReg() && MO.isDef() && MO.getReg() == Reg)
          return true;
    }
  return false;
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
  Slot->IsLocalTileABIImport = false;
  Slot->AssembleParentDef = nullptr;
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

static bool isSelfTileCopyOf(const MachineInstr &MI, Register Reg) {
  return (MI.getOpcode() == LinxV5::PseudoTCOPY ||
          (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
           MI.getOpcode() == LinxV5::PseudoPCOPYDesc)) &&
         MI.getNumOperands() > 2 && MI.getOperand(0).isReg() &&
         MI.getOperand(2).isReg() && MI.getOperand(0).getReg() == Reg &&
         MI.getOperand(2).getReg() == Reg;
}

static MachineOperand *getAssembleParentRefOperand(MachineInstr &MI) {
  Register ParentRef = LinxV5::getAssembleParentRef(MI);
  if (ParentRef == LinxV5::NoRegister)
    return nullptr;

  StringRef Asm =
      MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  size_t AssemblePos = Asm.find("B.ASSEMBLE");
  StringRef BeforeAssemble = Asm.take_front(AssemblePos);
  size_t IOTPos = BeforeAssemble.rfind("B.IOT");
  size_t IOSPos = BeforeAssemble.rfind("B.IOS");
  size_t BinderPos = IOTPos;
  if (BinderPos == StringRef::npos ||
      (IOSPos != StringRef::npos && IOSPos > BinderPos))
    BinderPos = IOSPos;
  if (BinderPos == StringRef::npos)
    return nullptr;
  size_t BinderEnd = Asm.find('\n', BinderPos);
  if (BinderEnd == StringRef::npos || BinderEnd > AssemblePos)
    BinderEnd = AssemblePos;

  MachineOperand *Exact = nullptr;
  for (size_t Pos = Asm.find('$', BinderPos);
       Pos != StringRef::npos && Pos < BinderEnd;
       Pos = Asm.find('$', Pos + 1)) {
    size_t DigitPos = Pos + 1;
    if (DigitPos < BinderEnd && Asm[DigitPos] == '{')
      ++DigitPos;
    size_t DigitEnd = DigitPos;
    while (DigitEnd < BinderEnd && isDigit(Asm[DigitEnd]))
      ++DigitEnd;
    if (DigitEnd == DigitPos)
      continue;
    unsigned ConstraintIdx = 0;
    if (Asm.slice(DigitPos, DigitEnd).getAsInteger(10, ConstraintIdx))
      continue;
    unsigned Group = 0;
    for (unsigned I = InlineAsm::MIOp_FirstOperand, E = MI.getNumOperands();
         I < E; ++Group) {
      MachineOperand &Flag = MI.getOperand(I);
      if (!Flag.isImm())
        break;
      unsigned N = InlineAsm::getNumOperandRegisters(Flag.getImm());
      if (Group == ConstraintIdx) {
        if (N == 1 && I + 1 < E) {
          MachineOperand &MO = MI.getOperand(I + 1);
          if (MO.isReg() && MO.isUse() && !MO.isImplicit() &&
              MO.getReg() == ParentRef)
            Exact = &MO;
        }
        break;
      }
      I += 1 + N;
    }
  }
  return Exact;
}

static bool isAssembleParentDefinition(const MachineInstr &MI,
                                       const MachineOperand &MO) {
  return MO.isDef() && MI.isInlineAsm() &&
         StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
             .contains("B.ASSEMBLE");
}

static unsigned getTSlotOccupation(MachineInstr &MI,
                                   const TargetRegisterClass *RC) {
  // PseudoEmptyTile expands to a zero-mask NoPE bundle.  Its dead destination
  // is only a compiler bookkeeping operand; the instruction neither
  // allocates nor publishes a Local tile queue entry.
  if (MI.getOpcode() == LinxV5::PseudoEmptyTile)
    return 0;
  if (MI.getOpcode() == LinxV5::PseudoLocalTileABISeed)
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
  const DenseSet<const MachineInstr *> *ClosedParentEpochs;
  DenseMap<const MachineOperand *, unsigned> *QueueCopyUseDistances;
  DenseSet<const MachineOperand *> *QueueDeadPublicationDefs;
  bool HadUnsynchronizedOpenParent = false;
  SlotCalc(MachineBasicBlock *mbb, const TargetInstrInfo *tii,
           const TargetRegisterClass *rc, bool iScalarRC, bool isTileRC,
           const DenseSet<const MachineInstr *> *closedParentEpochs,
           DenseMap<const MachineOperand *, unsigned> *queueCopyUseDistances,
           DenseSet<const MachineOperand *> *queueDeadPublicationDefs)
      : MBB(mbb), TII(tii), RC(rc), IsScalarRC(iScalarRC), IsTileRC(isTileRC),
        HeadIdx(0), TailIdx(0), PreIdx(0), NrCopy(0),
        TRNumber(RC->getNumRegs()), ClosedParentEpochs(closedParentEpochs),
        QueueCopyUseDistances(queueCopyUseDistances),
        QueueDeadPublicationDefs(queueDeadPublicationDefs) {
    TRI = MBB->getParent()->getSubtarget().getRegisterInfo();
  }
  void init(SlotStatus *SS);
  void resetAtLocalTileABIBoundary(const TRLiveRange &Seed);
  SmallVector<TRCopyRange *>
  allocNewSlot(SmallVectorImpl<const TRLiveRange *> &LRs);
  SmallVector<LinxRegOp> getLiveouts(bool EnableVerify);
  void BuildLiveoutAndEraseFrom(TRCopyRange &Slot, LinxRegOp RegOp,
                                SmallVector<LinxRegOp> &CurLiveouts);
  void rollingLiveouts(SmallVector<LinxRegOp> needSyncTo);
  void insertCopys(const SmallVector<TRLiveRange> &OriginalInstrs,
                   Register ReservedReg, SlotStatus *SS, bool IsInSyncGroup,
                   SmallVector<LinxRegOp> needSyncTo, SlotStatus &ResultSS);
  bool BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp,
                 Optional<size_t> SourceIdx = None);
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
  bool isInCycle() const {
    SmallVector<MachineBasicBlock *, 8> Worklist(MBB->successors());
    DenseSet<MachineBasicBlock *> Visited;
    while (!Worklist.empty()) {
      MachineBasicBlock *Cur = Worklist.pop_back_val();
      if (Cur == MBB)
        return true;
      if (!Visited.insert(Cur).second)
        continue;
      Worklist.append(Cur->succ_begin(), Cur->succ_end());
    }
    return false;
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

void SlotCalc::resetAtLocalTileABIBoundary(const TRLiveRange &Seed) {
  LiveSlots.clear();
  LiveSlots.resize(TRNumber);
  TailIdx = 0;
  HeadIdx = TRNumber;
  PreIdx = HeadIdx;

  if (!Seed.DefMO || !isRC(Seed.DefMO->getReg(), RC,
                           &MBB->getParent()->getRegInfo()))
    return;
  Optional<LinxV5::TileDescriptor> Descriptor =
      getLocalTileABISeedDescriptor(*Seed.MI);
  if (!Descriptor)
    report_fatal_error("malformed Local Tile ABI seed");
  TRCopyRange &Slot = LiveSlots[TRNumber - 1];
  Slot.RegOp = LinxRegOp(Seed.DefMO->getReg(), Descriptor->SizeCode);
  Slot.RegOp.Descriptor = *Descriptor;
  Slot.LastUse = Seed.LastUse;
  Slot.IsLiveout = Seed.IsLiveout;
  Slot.IsLocalTileABIImport = true;
  Slot.Idx = HeadIdx - 1;
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

bool SlotCalc::BuildCopy(MachineBasicBlock::iterator Before, LinxRegOp RegOp,
                         Optional<size_t> SourceIdx) {
  Register TR = RegOp.Reg;
  const bool IsDeadPublication = TR == LinxV5::NoRegister;

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
        bool IsProvenClosedParent =
            Candidate.AssembleParentDef && ClosedParentEpochs &&
            ClosedParentEpochs->count(Candidate.AssembleParentDef);
        if (Candidate.RegOp.Reg != LinxV5::NoRegister &&
            Candidate.RegOp.Descriptor &&
            (Candidate.IsLiveout || Candidate.LastUse) &&
            (!Candidate.AssembleParentDef || IsProvenClosedParent)) {
          // Candidate is one exact queue slot.  A physical register may have
          // been redefined with another descriptor since that slot was
          // published, so resolving the register at the insertion point can
          // describe a different payload.  Copy the slot's own descriptor;
          // Phase-2 simulation verifies the chosen relative source still
          // carries this exact value and descriptor.
          if (Candidate.RegOp.Descriptor->SizeCode != Candidate.RegOp.Size)
            continue;
          Donor = &Candidate;
          DonorDescriptor = *Candidate.RegOp.Descriptor;
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
      SourceIdx = Donor->Idx;
      BuildMI(*MBB, Before, DebugLoc(),
              TII->get(Descriptor.PackedPredicate ? LinxV5::PseudoPCOPYDesc
                                                  : LinxV5::PseudoTCOPYDesc))
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
                TII->get(Descriptor.PackedPredicate ? LinxV5::PseudoPCOPYDesc
                                                    : LinxV5::PseudoTCOPYDesc))
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
  MachineInstr &BuiltCopy = *std::prev(Before);
  if (IsTileRC) {
    if (!SourceIdx || HeadIdx < *SourceIdx ||
        HeadIdx - *SourceIdx < 1 || HeadIdx - *SourceIdx > TRNumber)
      report_fatal_error(
          "compiler-inserted Local tile copy lacks one exact source slot");
    if ((BuiltCopy.getOpcode() != LinxV5::PseudoTCOPYDesc &&
         BuiltCopy.getOpcode() != LinxV5::PseudoPCOPYDesc) ||
        BuiltCopy.getNumOperands() <= 2 || !BuiltCopy.getOperand(2).isReg())
      report_fatal_error(
          "compiler-inserted Local tile copy has no trackable source operand");
    (*QueueCopyUseDistances)[&BuiltCopy.getOperand(2)] =
        HeadIdx - *SourceIdx;
    if (IsDeadPublication)
      QueueDeadPublicationDefs->insert(&BuiltCopy.getOperand(0));
  }
  LLVM_DEBUG(dbgs() << "build copy " << BuiltCopy);
  return true;
}

void SlotCalc::PreAlloc(TRCopyRange Slot) {
  pre() = Slot;
  pre().Idx = PreIdx;
  expire();
  prealloc();
}

bool SlotCalc::SlotCopy(MachineBasicBlock::iterator Before, TRCopyRange &Slot) {
  if (Slot.AssembleParentDef &&
      (!ClosedParentEpochs ||
       !ClosedParentEpochs->count(Slot.AssembleParentDef)))
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
    if (!BuildCopy(Before, head().RegOp, head().Idx))
      return false;
    advance();
  }

  if (!BuildCopy(Before, Slot.RegOp,
                 Slot.RegOp.Reg == LinxV5::NoRegister
                     ? Optional<size_t>()
                     : Optional<size_t>(Slot.Idx)))
    return false;
  head() = Slot;
  head().Idx = HeadIdx;
  // A proved-CLOSED parent is ordinary payload after a real copy.  The new
  // queue entry must not inherit stale protection for the old INIT epoch.
  head().IsAssembleParent = false;
  head().IsLocalTileABIImport = false;
  head().AssembleParentDef = nullptr;
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
  const TRCopyRange *Source = nullptr;
  if (RegOp.Reg != LinxV5::NoRegister) {
    if (Slot.IsLiveout && Slot.RegOp == RegOp)
      Source = &Slot;
    else
      for (unsigned I = TailIdx; I < HeadIdx; ++I) {
        const TRCopyRange &Candidate = LiveSlots[I % TRNumber];
        if (Candidate.IsLiveout && Candidate.RegOp == RegOp) {
          Source = &Candidate;
          break;
        }
      }
    if (!Source)
      report_fatal_error(
          "cannot locate exact source value for live-out queue copy");
    // Copy the value that is actually present in the selected queue slot.
    // The synchronization target may use the same physical register with a
    // different descriptor; it must never relabel this payload.
    tmp.RegOp = Source->RegOp;
    tmp.Idx = Source->Idx;
  }
  // Rolling a live-out changes only its queue position.  Preserve the OPEN
  // B.ASSEMBLE-parent identity from the source slot so SlotCopy cannot bypass
  // the live-parent copy guard, and so later rolls still know that the
  // generation is open.
  if (RegOp.Reg != LinxV5::NoRegister) {
    if (Source->AssembleParentDef) {
      tmp.IsAssembleParent = true;
      tmp.AssembleParentDef = Source->AssembleParentDef;
    }
  }
  if (!SlotCopy(EndI, tmp))
    return;
  if (RegOp.Reg != LinxV5::NoRegister) {
    // update: release liveout flag.
    for (size_t j = 0; j < TRNumber; ++j) {
      if (LiveSlots[j].IsLiveout && LiveSlots[j].RegOp == RegOp)
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

  // An ABI seed describes the existing incoming t#1; it is not an allocation.
  // Preserve that fact through scalar-only blocks.  If the complete
  // tail-aligned live-out sequence already matches the successor contract,
  // publishing the imported value again would add a forbidden callee-entry
  // TMOV without changing any relative age.  Ordinary queue values still use
  // the established rolling path, retaining descriptor-copy coverage at PHIs.
  SmallVector<LinxRegOp> ExistingLiveouts = getLiveouts();
  if (ExistingLiveouts == needSyncTo) {
    bool FoundFirstLiveout = false;
    bool HasImportedValue = false;
    bool AllValuesAreImports = true;
    unsigned Index = tailsi();
    for (unsigned I = 0; I != TRNumber; ++I) {
      const TRCopyRange &Slot = LiveSlots[Index];
      FoundFirstLiveout |= Slot.IsLiveout;
      if (FoundFirstLiveout && Slot.IsLiveout) {
        HasImportedValue |= Slot.IsLocalTileABIImport;
        AllValuesAreImports &= Slot.IsLocalTileABIImport;
      }
      Index = Index == TRNumber - 1 ? 0 : Index + 1;
    }
    if (HasImportedValue && AllValuesAreImports)
      return;
  }

  // An OPEN assembly parent itself cannot be copied, but ordinary live-outs
  // may be rotated past it.  This is required when an INIT is conditional:
  // the first loop entry has no parent while a backedge has one, so the
  // ordinary loop-carried value otherwise reaches the same instruction at
  // two different relative offsets.  Only accept an exact cyclic rotation
  // whose moved prefix contains no OPEN parent.  The inserted ordinary copies
  // advance the queue without changing parent identity; the later assembly
  // dataflow proof recomputes every ParentRef distance after copy insertion.
  const bool HasOpenParent = llvm::any_of(LiveSlots, [&](const TRCopyRange &Slot) {
    return Slot.AssembleParentDef &&
           (!ClosedParentEpochs ||
            !ClosedParentEpochs->count(Slot.AssembleParentDef));
  });
  if (HasOpenParent) {
    SmallVector<LinxRegOp> CurLiveouts(getLiveouts());
    while (!needSyncTo.empty() &&
           needSyncTo.front().Reg == LinxV5::NoRegister)
      needSyncTo.erase(needSyncTo.begin());
    // Only an older prefix before the first constrained live-out is outside
    // the successor contract.  Trailing/interior holes represent real newer
    // publications and therefore contribute to every live value's relative
    // age.  Dropping them makes a loop backedge appear synchronized while its
    // hardware queue is one entry younger than the preheader queue.
    LLVM_DEBUG(dbgs() << "OPEN-parent sync current/required:";
               for (const LinxRegOp &RO : CurLiveouts)
                 dbgs() << " " << printReg(RO.Reg, TRI);
               dbgs() << " /";
               for (const LinxRegOp &RO : needSyncTo)
                 dbgs() << " " << printReg(RO.Reg, TRI);
               dbgs() << "\n");
    if (CurLiveouts.size() != needSyncTo.size())
      report_fatal_error(
          "cannot synchronize Local tile queue without copying an OPEN "
          "B.ASSEMBLE parent");

    Optional<unsigned> SafeRotation;
    for (unsigned Shift = 0; Shift < CurLiveouts.size(); ++Shift) {
      bool Matches = true;
      for (unsigned I = 0; I < CurLiveouts.size(); ++I)
        Matches &= CurLiveouts[(I + Shift) % CurLiveouts.size()] ==
                   needSyncTo[I];
      if (!Matches)
        continue;

      bool MovesParent = false;
      for (unsigned I = 0; I < Shift; ++I)
        MovesParent |= llvm::any_of(LiveSlots, [&](const TRCopyRange &Slot) {
          return Slot.AssembleParentDef &&
                 (!ClosedParentEpochs ||
                  !ClosedParentEpochs->count(Slot.AssembleParentDef)) &&
                 Slot.RegOp == CurLiveouts[I];
        });
      if (!MovesParent) {
        SafeRotation = Shift;
        break;
      }
    }
    if (!SafeRotation)
      report_fatal_error(
          "cannot synchronize Local tile queue without copying an OPEN "
          "B.ASSEMBLE parent");

    // A copy in a cyclic block executes once per dynamic iteration and can
    // push an otherwise stationary OPEN parent beyond the 16-entry window.
    // Keep this class absolute for ordinary values instead; the post-Phase1
    // proof still requires every ParentRef to have one exact distance.
    if (*SafeRotation != 0 && isInCycle()) {
      HadUnsynchronizedOpenParent = true;
      return;
    }

    for (unsigned I = 0; I < *SafeRotation; ++I) {
      TRCopyRange &TmpSlot = tail();
      BuildLiveoutAndEraseFrom(TmpSlot, CurLiveouts.front(), CurLiveouts);
    }
    return;
  }

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
        if (Slot.RegOp == needSyncTo[rollingI]) {
          // success
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
    if (MI->getOpcode() == LinxV5::PseudoLocalTileABISeed) {
      resetAtLocalTileABIBoundary(OI);
      ++OII;
      LLVM_DEBUG(printLiveSlots(); dbgs() << "\n");
      continue;
    }
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
        NewSlots[i]->AssembleParentDef =
            NewSlots[i]->IsAssembleParent ? LRs[i]->MI : nullptr;
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
            LiveSlots[j].AssembleParentDef = nullptr;
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
    SS->LiveOuts = DoneMBBsSlotStatus[0]->LiveOuts;

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
  // Holes before the oldest constrained live-out carry no successor state.
  // Preserve every trailing/interior hole: each is a real publication newer
  // than a live-in and must be reproduced by other incoming edges so relative
  // source ages stay identical at the join.
  while (!Res.empty() && Res.front().Reg == LinxV5::NoRegister)
    Res.erase(Res.begin());
  return Res;
}

SmallVector<QueueContractEntry, 8>
LinxV5TRegToOffsetOpt::buildEntryContract(
    MachineBasicBlock &MBB, const TargetRegisterClass *RC,
    ArrayRef<LinxRegOp> LiveOuts) {
  SmallVector<QueueContractEntry, 8> Contract;
  auto RequiredMBB = CanonicalEntryLiveIns.find(&MBB);
  if (RequiredMBB == CanonicalEntryLiveIns.end() ||
      !RequiredMBB->second.count(RC))
    return Contract;
  for (Register Reg : RequiredMBB->second.find(RC)->second) {
    bool HasLocalUse = llvm::any_of(MBB, [&](const MachineInstr &MI) {
      return llvm::any_of(MI.operands(), [&](const MachineOperand &MO) {
        return MO.isReg() && MO.isUse() && !MO.isImplicit() &&
               MO.getReg() == Reg;
      });
    });
    if (isOpenAssembleParentRegister(*MBB.getParent(), Reg))
      continue;
    if (!HasLocalUse)
      continue;

    Optional<unsigned> Match;
    for (unsigned I = LiveOuts.size(); I != 0; --I) {
      const LinxRegOp &Candidate = LiveOuts[I - 1];
      if (Candidate.Reg == Reg) {
        Match = I - 1;
        break;
      }
    }
    if (!Match) {
      LLVM_DEBUG(dbgs() << "missing entry contract for "
                        << printReg(Reg, TRI) << " on "
                        << printMBBReference(MBB) << " in "
                        << TRI->getRegClassName(RC) << "\n");
      report_fatal_error(
          "reachable CFG edge is missing a Local tile live-in value");
    }
    const LinxRegOp &Value = LiveOuts[*Match];
    Contract.push_back({Value,
                        static_cast<unsigned>(LiveOuts.size() - *Match)});
  }
  llvm::sort(Contract, [](const QueueContractEntry &LHS,
                          const QueueContractEntry &RHS) {
    return LHS.Value.Reg < RHS.Value.Reg;
  });
  return Contract;
}

void LinxV5TRegToOffsetOpt::appendQueueFillers(
    MachineBasicBlock &MBB, const TargetRegisterClass *RC, unsigned Count) {
  SlotStatus &Status = GlobalSlotsInfo[&MBB][RC];
  const unsigned Capacity = RC->getNumRegs();
  Register ReserveReg = RCsInfo[RC][2];
  auto InsertAt = MBB.end();
  for (auto I = MBB.end(); I != MBB.begin(); --I)
    if (LinxV5::isTileOp(*std::prev(I))) {
      InsertAt = I;
      break;
    }
  if (InsertAt == MBB.end())
    InsertAt = MBB.getFirstTerminator();

  while (Count--) {
    if (Status.LiveOuts.size() >= Capacity)
      report_fatal_error(
          "Local tile edge normalization would evict a live queue value");
    const TRCopyRange *Donor = nullptr;
    for (unsigned I = Status.TailIdx; I < Status.HeadIdx; ++I) {
      const TRCopyRange &Candidate = Status.TRCopyRanges[I % Capacity];
      if (Candidate.RegOp.Reg == LinxV5::NoRegister ||
          !Candidate.RegOp.Descriptor || Candidate.AssembleParentDef)
        continue;
      bool IsLive = llvm::any_of(Status.LiveOuts, [&](const LinxRegOp &Value) {
        return Value.Reg != LinxV5::NoRegister &&
               Value == Candidate.RegOp;
      });
      if (IsLive) {
        Donor = &Candidate;
        break;
      }
    }
    if (!Donor)
      report_fatal_error(
          "Local tile edge normalization lacks a closed live filler donor");
    unsigned Distance = Status.HeadIdx - Donor->Idx;
    if (Distance < 1 || Distance > Capacity)
      report_fatal_error(
          "Local tile edge normalization donor exceeds the queue window");
    const LinxV5::TileDescriptor &Descriptor = *Donor->RegOp.Descriptor;
    MachineInstrBuilder Copy =
        BuildMI(MBB, InsertAt, DebugLoc(),
                TII->get(Descriptor.PackedPredicate ? LinxV5::PseudoPCOPYDesc
                                                    : LinxV5::PseudoTCOPYDesc))
            .addReg(ReserveReg, RegState::Define | RegState::Dead)
            .addImm(Descriptor.SizeCode)
            .addReg(Donor->RegOp.Reg)
            .addImm(Descriptor.ValidCol)
            .addImm(Descriptor.ValidRow)
            .addImm(Descriptor.PhysicalCol)
            .addImm(Descriptor.DataType)
            .addImm(Descriptor.Layout);
    MachineInstr *CopyMI = Copy.getInstr();
    QueueCopyUseDistances[&CopyMI->getOperand(2)] = Distance;
    QueueDeadPublicationDefs.insert(&CopyMI->getOperand(0));

    TRCopyRange &Published =
        Status.TRCopyRanges[Status.HeadIdx % Capacity];
    initSlot(&Published);
    Published.Idx = Status.HeadIdx;
    ++Status.HeadIdx;
    ++Status.TailIdx;
    Status.LiveOuts.push_back(LinxRegOp());
    ++TileCFGCopy;
  }
}

void LinxV5TRegToOffsetOpt::rebuildCanonicalEntryStates(MachineFunction &MF) {
  CanonicalEntryContracts.clear();
  CanonicalRewriteEntryStates.clear();
  for (MachineBasicBlock &MBB : MF) {
    if (MBB.pred_empty())
      continue;
    MachineBasicBlock *Pred = *MBB.pred_begin();
    for (const TargetRegisterClass *RC :
         {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
          &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
      SlotStatus &Status = GlobalSlotsInfo[Pred][RC];
      CanonicalEntryContracts[&MBB][RC] =
          buildEntryContract(MBB, RC, Status.LiveOuts);
      TRegIndexState RewriteState;
      for (unsigned I = Status.TailIdx; I < Status.HeadIdx; ++I) {
        const LinxRegOp &Value =
            Status.TRCopyRanges[I % RC->getNumRegs()].RegOp;
        if (Value.Reg != LinxV5::NoRegister)
          RewriteState.emplace_back(Value, I - Status.TailIdx);
      }
      CanonicalRewriteEntryStates[&MBB][RC] =
          std::make_pair(std::move(RewriteState),
                         Status.HeadIdx - Status.TailIdx);
    }
  }
}

void LinxV5TRegToOffsetOpt::normalizeEntryContracts(MachineFunction &MF) {
  const unsigned IterationLimit =
      std::max<unsigned>(1, MF.size()) * LinxV5::Tile_NRRegClass.getNumRegs();
  for (unsigned Iteration = 0; Iteration < IterationLimit; ++Iteration) {
    bool Changed = false;
    for (MachineBasicBlock &Succ : MF) {
      if (Succ.pred_size() < 2)
        continue;
      for (const TargetRegisterClass *RC :
           {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
            &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
        if (PureAssembleRCs.count(RC))
          continue;
        SmallVector<MachineBasicBlock *, 4> Preds(Succ.predecessors());
        SmallVector<SmallVector<QueueContractEntry, 8>, 4> Contracts;
        for (MachineBasicBlock *Pred : Preds)
          Contracts.push_back(buildEntryContract(
              Succ, RC, GlobalSlotsInfo[Pred][RC].LiveOuts));
        if (Contracts.empty() || Contracts.front().empty())
          continue;
        SmallVector<unsigned, 8> TargetAges(Contracts.front().size(), 0);
        for (const auto &Contract : Contracts) {
          if (Contract.size() != Contracts.front().size())
            report_fatal_error(
                "Local tile CFG predecessors expose different live values");
          for (unsigned I = 0; I < Contract.size(); ++I) {
            if (!Contract[I].Value.Descriptor ||
                !Contracts.front()[I].Value.Descriptor)
              report_fatal_error(
                  "multi-predecessor Local tile contract lacks a complete "
                  "descriptor");
            if (!(Contract[I].Value == Contracts.front()[I].Value))
              report_fatal_error(
                  "Local tile CFG predecessors expose conflicting values or "
                  "descriptors");
            TargetAges[I] = std::max(TargetAges[I], Contract[I].Age);
          }
        }
        for (unsigned P = 0; P < Preds.size(); ++P) {
          Optional<unsigned> Delta;
          for (unsigned I = 0; I < Contracts[P].size(); ++I) {
            unsigned ThisDelta = TargetAges[I] - Contracts[P][I].Age;
            if (Delta && *Delta != ThisDelta)
              report_fatal_error(
                  "Local tile CFG edge needs non-uniform value rotation");
            Delta = ThisDelta;
          }
          if (Delta && *Delta) {
            appendQueueFillers(*Preds[P], RC, *Delta);
            Changed = true;
          }
        }
      }
    }
    if (!Changed) {
      rebuildCanonicalEntryStates(MF);
      return;
    }
  }
  report_fatal_error("Local tile CFG queue normalization did not converge");
}

void LinxV5TRegToOffsetOpt::certifyEntryContracts(MachineFunction &MF) {
  DenseSet<MachineBasicBlock *> Reachable;
  SmallVector<MachineBasicBlock *, 16> Worklist;
  if (!MF.empty())
    Worklist.push_back(&MF.front());
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.pop_back_val();
    if (!Reachable.insert(MBB).second)
      continue;
    Worklist.append(MBB->succ_begin(), MBB->succ_end());
  }

  for (MachineBasicBlock &Succ : MF) {
    if (!Reachable.count(&Succ) || Succ.pred_empty())
      continue;
    for (const TargetRegisterClass *RC :
         {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
          &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
      if (PureAssembleRCs.count(RC))
        continue;
      bool HasLiveIn = CanonicalEntryLiveIns.count(&Succ) &&
                       CanonicalEntryLiveIns[&Succ].count(RC) &&
                       !CanonicalEntryLiveIns[&Succ][RC].empty();
      if (!HasLiveIn)
        continue;
      auto SuccContracts = CanonicalEntryContracts.find(&Succ);
      if (SuccContracts == CanonicalEntryContracts.end() ||
          !SuccContracts->second.count(RC))
        report_fatal_error(
            "reachable Local tile block has no canonical entry contract");
      ArrayRef<QueueContractEntry> Canonical =
          SuccContracts->second.find(RC)->second;
      for (MachineBasicBlock *Pred : Succ.predecessors()) {
        if (!Reachable.count(Pred))
          continue;
        ArrayRef<LinxRegOp> LiveOuts = GlobalSlotsInfo[Pred][RC].LiveOuts;
        SmallVector<QueueContractEntry, 8> Actual =
            buildEntryContract(Succ, RC, LiveOuts);
        bool Matches = Actual.size() == Canonical.size();
        if (Matches)
          for (unsigned I = 0; I < Actual.size(); ++I)
            Matches &= Actual[I] == Canonical[I];
        if (Matches)
          continue;
        LLVM_DEBUG(dbgs() << "Local tile edge contract mismatch "
                          << printMBBReference(*Pred) << " -> "
                          << printMBBReference(Succ) << " in "
                          << TRI->getRegClassName(RC) << "\n";
                   dbgs() << "canonical:";
                   for (const QueueContractEntry &Entry : Canonical)
                     dbgs() << " " << printReg(Entry.Value.Reg, TRI) << "/"
                            << Entry.Value.Size << "@" << Entry.Age;
                   dbgs() << " actual:";
                   for (const QueueContractEntry &Entry : Actual)
                     dbgs() << " " << printReg(Entry.Value.Reg, TRI) << "/"
                            << Entry.Value.Size << "@" << Entry.Age;
                   dbgs() << "\n");
        report_fatal_error(
            "Local tile CFG edge violates the canonical entry contract");
      }
    }
  }
}

void LinxV5TRegToOffsetOpt::simulateAndValidateQueues(MachineFunction &MF) {
  DenseMap<MachineBasicBlock *,
           DenseMap<const TargetRegisterClass *,
                    std::pair<TRegIndexState, unsigned>>>
      ExitStates;

  for (MachineBasicBlock &MBB : MF) {
    for (const TargetRegisterClass *RC :
         {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
          &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
      if (PureAssembleRCs.count(RC))
        continue;
      TRegIndexState State;
      unsigned CurIdx = 0;
      if (!MBB.pred_empty() && CanonicalRewriteEntryStates.count(&MBB) &&
          CanonicalRewriteEntryStates[&MBB].count(RC)) {
        State = CanonicalRewriteEntryStates[&MBB][RC].first;
        CurIdx = CanonicalRewriteEntryStates[&MBB][RC].second;
      }

      for (MachineInstr &MI : MBB) {
        if (MI.getOpcode() == LinxV5::PseudoLocalTileABISeed) {
          resetStateAtLocalTileABISeed(MI, RC, State, CurIdx, MRI);
          continue;
        }
        if (MI.isKill()) {
          Register Reg = MI.getOperand(0).getReg();
          if (isRC(Reg, RC, MRI))
            eraseRegisterIdentities(State, Reg);
          continue;
        }
        if (canSkipMI(MI) || MI.getOpcode() == LinxV5::PseudoEmptyTile)
          continue;
        if (MI.isImplicitDef()) {
          Register Reg = MI.getOperand(0).getReg();
          if (isRC(Reg, RC, MRI)) {
            eraseRegisterIdentities(State, Reg);
            State.emplace_back(LinxRegOp(Reg, LinxV5::SIMT_REG_SIZE_D), -1u);
          }
          continue;
        }

        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isUse() || MO.isImplicit() ||
              !isRC(MO.getReg(), RC, MRI))
            continue;
          Register OriginalReg = MO.getReg();
          MachineOperand *ParentRef = getAssembleParentRefOperand(MI);
          auto ParentUse = AssembleParentUseDistances.find(&MI);
          if (&MO == ParentRef && ParentUse != AssembleParentUseDistances.end() &&
              ParentUse->second.count(OriginalReg)) {
            unsigned Distance = ParentUse->second.lookup(OriginalReg);
            if (Distance < 1 || Distance > RC->getNumRegs())
              report_fatal_error(
                  "simulated B.ASSEMBLE parent source exceeds Tile window");
            continue;
          }
          auto OrdinaryUse = AssembleOrdinaryUseDistances.find(&MI);
          if (OrdinaryUse != AssembleOrdinaryUseDistances.end() &&
              OrdinaryUse->second.count(OriginalReg)) {
            unsigned Distance = OrdinaryUse->second.lookup(OriginalReg);
            if (Distance < 1 || Distance > RC->getNumRegs())
              report_fatal_error(
                  "simulated reserved-hand source exceeds Tile window");
            continue;
          }

          auto CopyUse = QueueCopyUseDistances.find(&MO);
          if (CopyUse != QueueCopyUseDistances.end()) {
            unsigned Distance = CopyUse->second;
            Optional<LinxRegOp> Expected =
                getTileQueueIdentity(MBB, MI, MO, /*IsDef=*/false);
            if (!Expected || Distance < 1 || Distance > RC->getNumRegs() ||
                CurIdx < Distance)
              report_fatal_error(
                  "compiler-inserted Tile copy lacks a simulatable source");
            unsigned SourceIdx = CurIdx - Distance;
            auto Source = llvm::find_if(State, [&](const auto &Entry) {
              return Entry.second == SourceIdx;
            });
            if (Source == State.end() || !(Source->first == *Expected)) {
              LLVM_DEBUG(dbgs() << "simulated Tile copy source mismatch in "
                                << printMBBReference(MBB) << " distance "
                                << Distance << " index " << SourceIdx
                                << " expected "
                                << printReg(Expected->Reg, TRI) << "/"
                                << Expected->Size << " actual ";
                         if (Source == State.end())
                           dbgs() << "<hole>";
                         else
                           dbgs() << printReg(Source->first.Reg, TRI) << "/"
                                  << Source->first.Size;
                         dbgs() << " state";
                         for (const auto &Entry : State)
                           dbgs() << " " << printReg(Entry.first.Reg, TRI)
                                  << "/" << Entry.first.Size << "@"
                                  << Entry.second;
                         dbgs() << ": " << MI);
              report_fatal_error(
                  "compiler-inserted Tile copy source does not match the "
                  "simulated queue value");
            }
            continue;
          }

          auto Identity = RewriteTRIdentities.find(&MO);
          if (Identity == RewriteTRIdentities.end())
            report_fatal_error(
                "Local tile source lacks a pre-rewrite identity snapshot");
          auto Source = findExactIdentity(State, Identity->second);
          if (Source == State.end() || Source->second == -1u ||
              CurIdx <= Source->second ||
              CurIdx - Source->second > RC->getNumRegs()) {
            LLVM_DEBUG(dbgs() << "simulated ordinary Tile source mismatch in "
                              << printMBBReference(MBB) << ": " << MI);
            report_fatal_error(
                "ordinary Local tile source has no exact simulated queue "
                "value");
          }
        }

        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isDef() || MO.isImplicit() ||
              !isRC(MO.getReg(), RC, MRI))
            continue;
          if (isAssembleParentDefinition(MI, MO)) {
            ++CurIdx;
            continue;
          }
          if (QueueDeadPublicationDefs.count(&MO)) {
            ++CurIdx;
            continue;
          }
          auto Identity = RewriteTRIdentities.find(&MO);
          if (Identity == RewriteTRIdentities.end())
            report_fatal_error(
                "Local tile definition lacks a pre-rewrite identity snapshot");
          State.emplace_back(Identity->second, CurIdx++);
        }
      }
      ExitStates[&MBB][RC] = std::make_pair(std::move(State), CurIdx);
    }
  }

  for (MachineBasicBlock &Succ : MF) {
    if (Succ.pred_empty())
      continue;
    for (const TargetRegisterClass *RC :
         {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
          &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
      if (PureAssembleRCs.count(RC))
        continue;
      if (!CanonicalEntryContracts.count(&Succ) ||
          !CanonicalEntryContracts[&Succ].count(RC))
        continue;
      ArrayRef<QueueContractEntry> Canonical =
          CanonicalEntryContracts[&Succ][RC];
      for (MachineBasicBlock *Pred : Succ.predecessors()) {
        if (!ExitStates.count(Pred) || !ExitStates[Pred].count(RC))
          report_fatal_error(
              "reachable predecessor lacks a simulated Local tile exit");
        TRegIndexState &State = ExitStates[Pred][RC].first;
        unsigned CurIdx = ExitStates[Pred][RC].second;
        for (const QueueContractEntry &Expected : Canonical) {
          auto Source = findExactIdentity(State, Expected.Value);
          if (Source == State.end() || Source->second == -1u ||
              CurIdx <= Source->second ||
              CurIdx - Source->second != Expected.Age) {
            LLVM_DEBUG(dbgs() << "simulated edge mismatch "
                              << printMBBReference(*Pred) << " -> "
                              << printMBBReference(Succ) << " for "
                              << printReg(Expected.Value.Reg, TRI) << "@"
                              << Expected.Age << "\n");
            report_fatal_error(
                "simulated Local tile edge violates canonical entry age");
          }
        }
      }
    }
  }
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
    auto ClosedIt = ClosedAssembleParentsAtTail.find(&MBB);
    const DenseSet<const MachineInstr *> *ClosedParentEpochs =
        ClosedIt == ClosedAssembleParentsAtTail.end() ? nullptr
                                                      : &ClosedIt->second;
    SlotCalc SC(&MBB, TII, RC, IsScalarRC, IsTileRC, ClosedParentEpochs,
                &QueueCopyUseDistances, &QueueDeadPublicationDefs);
    SlotStatus ResultSS;
    if (IsTileRC) {
      DenseSet<Register> &Required = CanonicalEntryLiveIns[&MBB][RC];
      for (const auto &Use : LiveInsLastUse)
        if (isRC(Use.first, RC, MRI) &&
            !hasAnyReachingExplicitUndef(MBB, Use.first))
          Required.insert(Use.first);
    }
    SlotStatus *InitStatus =
        getInitStatus(MBB, LiveInsLastUse, RestLiveouts, RC);
    if (IsTileRC && InitStatus) {
      CanonicalEntryContracts[&MBB][RC] =
          buildEntryContract(MBB, RC, InitStatus->LiveOuts);
      TRegIndexState RewriteState;
      for (unsigned I = InitStatus->TailIdx; I < InitStatus->HeadIdx; ++I) {
        const LinxRegOp &Value =
            InitStatus->TRCopyRanges[I % RC->getNumRegs()].RegOp;
        if (Value.Reg != LinxV5::NoRegister)
          RewriteState.emplace_back(Value, I - InitStatus->TailIdx);
      }
      CanonicalRewriteEntryStates[&MBB][RC] =
          std::make_pair(std::move(RewriteState),
                         InitStatus->HeadIdx - InitStatus->TailIdx);
    }
    SmallVector<LinxRegOp> NeedSyncTo;
    bool CanSynchronizeQueue = !SyncConflictRCs.count(RC);
    if (CanSynchronizeQueue)
      NeedSyncTo = getLiveoutLimits(MBB, RC);
    // getLiveoutLimits can discover that completed successors require
    // different logical values in one tail-aligned queue position.  Do not
    // partially honor the surviving positions. Local Tile classes are later
    // normalized to a converged edge contract or rejected; Phase 1 must not
    // fabricate a copy from an already-conflicting partial target window.
    CanSynchronizeQueue = !SyncConflictRCs.count(RC);
    if (!CanSynchronizeQueue)
      NeedSyncTo.clear();
    SC.insertCopys(OriginalInstrs, RCsInfo[RC][2], InitStatus,
                   CanSynchronizeQueue && isInSyncGroup(&MBB),
                   NeedSyncTo,
                   ResultSS);
    if (SC.HadUnsynchronizedOpenParent)
      SyncConflictRCs.insert(RC);
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
  certifyEntryContracts(MF);
  for (const TargetRegisterClass *RC :
       {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
        &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass})
    SyncConflictRCs.erase(RC);
}

void LinxV5TRegToOffsetOpt::solveAssembleParentDataflow(
    MachineFunction &MF, bool CollectClosedTailProofs) {
  AssembleParentUseDistances.clear();
  AssembleOrdinaryUseDistances.clear();
  if (CollectClosedTailProofs)
    ClosedAssembleParentsAtTail.clear();
  SmallVector<std::pair<Register, MachineInstr *>, 4> ParentEpochs;
  DenseMap<unsigned, unsigned> ParentEpochCounts;
  DenseSet<const MachineInstr *> ParentRefUses;
  DenseMap<const MachineInstr *, unsigned> ParentRefProofCounts;
  DenseMap<const MachineInstr *, unsigned> ParentRefAnalysisCounts;
  DenseSet<const MachineInstr *> ReachedUnprovedParentRefs;
  DenseSet<const MachineInstr *> IncompleteParentRefProofs;
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      OrdinaryAnalysisCounts;
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      OrdinaryProofDistances;
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      OrdinaryReachedCounts;
  DenseMap<const MachineInstr *, DenseMap<Register, unsigned>>
      OrdinaryOtherReachedCounts;
  DenseMap<const MachineInstr *, DenseSet<Register>>
      ReachedUnprovedOrdinaryUses;
  DenseMap<const MachineInstr *, DenseSet<Register>>
      IncompleteOrdinaryUses;
  DenseMap<const MachineInstr *, DenseSet<Register>>
      ConflictOrdinaryUses;
  DenseMap<const MachineInstr *, DenseSet<Register>> OpenOrdinaryUses;

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
    enum class ProofRequirement {
      OpenParent,
      PublishedSource,
      ExactClosed,
      ExactClosedAnyDistance
    };
    DenseMap<const MachineInstr *, UseProof> UseProofs;
    DenseMap<const MachineInstr *, UseProof> ClosedPointProofs;
    DenseMap<const MachineInstr *, UseProof> ClosedAnyDistanceProofs;
    auto proveUse = [&](MachineInstr *UseMI, ProofRequirement Requirement) {
      DenseMap<const MachineInstr *, UseProof> *ProofCachePtr = &UseProofs;
      if (Requirement == ProofRequirement::ExactClosed)
        ProofCachePtr = &ClosedPointProofs;
      else if (Requirement == ProofRequirement::ExactClosedAnyDistance)
        ProofCachePtr = &ClosedAnyDistanceProofs;
      DenseMap<const MachineInstr *, UseProof> &ProofCache = *ProofCachePtr;
      auto Cached = ProofCache.find(UseMI);
      if (Cached != ProofCache.end())
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
        std::map<unsigned, std::set<uint64_t>> Constants;
        std::map<SpillSlotKey, std::set<uint64_t>> SpillConstants;
        // Exact equalities survive set joins; equal sets alone do not imply
        // that two registers hold the same value on each concrete path.
        std::set<std::pair<unsigned, unsigned>> EqualRegisters;
      };
      SmallVector<PathState, 32> Worklist;
      PathState Entry;
      Entry.MBB = &MF.front();
      Worklist.push_back(std::move(Entry));
      std::map<std::string, PathState> JoinedStates;
      bool SawUse = false;
      Optional<StateKind> ReachingUseKind;
      MachineInstr *ReachingOtherDef = nullptr;
      Optional<unsigned> ReachingParentDistance;
      constexpr unsigned MaxPathStates = 65536;
      constexpr unsigned MaxTrackedConstants = 16;
      unsigned ProcessedStates = 0;

      auto getConstantSet = [](const PathState &State, Register Reg)
          -> Optional<std::set<uint64_t>> {
        if (Reg == LinxV5::R0)
          return std::set<uint64_t>{0};
        auto It = State.Constants.find(Reg.id());
        if (It == State.Constants.end())
          return None;
        return It->second;
      };
      auto evaluateImmediatePredicate = [](unsigned Opcode, uint64_t Value,
                                           uint64_t Immediate)
          -> Optional<bool> {
        switch (Opcode) {
        case LinxV5::SETC_EQI:
        case LinxV5::SETC_EQI_BR:
          return Value == Immediate;
        case LinxV5::SETC_NEI:
        case LinxV5::SETC_NEI_BR:
          return Value != Immediate;
        case LinxV5::SETC_GEI:
          return static_cast<int64_t>(Value) >=
                 static_cast<int64_t>(Immediate);
        case LinxV5::SETC_LTI:
        case LinxV5::SETC_LTI_BR:
          return static_cast<int64_t>(Value) <
                 static_cast<int64_t>(Immediate);
        case LinxV5::SETC_LTUI:
        case LinxV5::SETC_LTUI_BR:
          return Value < Immediate;
        default:
          return None;
        }
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
        for (auto It = State.EqualRegisters.begin();
             It != State.EqualRegisters.end();) {
          if (TRI->regsOverlap(Register(It->first), Def) ||
              TRI->regsOverlap(Register(It->second), Def))
            It = State.EqualRegisters.erase(It);
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
                      MI.getOpcode() == LinxV5::SETC_GEI ||
                      MI.getOpcode() == LinxV5::SETC_LTI ||
                      MI.getOpcode() == LinxV5::SETC_LTUI ||
                      MI.getOpcode() == LinxV5::SETC_EQI_BR ||
                      MI.getOpcode() == LinxV5::SETC_NEI_BR ||
                      MI.getOpcode() == LinxV5::SETC_LTI_BR) &&
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
                (MI.getOpcode() == TargetOpcode::COPY ||
                 MI.getOpcode() == LinxV5::ORI ||
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
        PathState State = Worklist.pop_back_val();
        std::string StateKey = makeKey(State);
        auto ExistingState = JoinedStates.find(StateKey);
        if (ExistingState == JoinedStates.end()) {
          JoinedStates.emplace(StateKey, State);
        } else {
          PathState Joined = ExistingState->second;
          bool Changed = false;
          for (auto It = Joined.Constants.begin();
               It != Joined.Constants.end();) {
            auto Incoming = State.Constants.find(It->first);
            if (Incoming == State.Constants.end()) {
              It = Joined.Constants.erase(It);
              Changed = true;
              continue;
            }
            std::set<uint64_t> Values = It->second;
            Values.insert(Incoming->second.begin(), Incoming->second.end());
            if (Values.size() > MaxTrackedConstants) {
              It = Joined.Constants.erase(It);
              Changed = true;
              continue;
            }
            if (Values != It->second) {
              It->second = std::move(Values);
              Changed = true;
            }
            ++It;
          }
          for (auto It = Joined.SpillConstants.begin();
               It != Joined.SpillConstants.end();) {
            auto Incoming = State.SpillConstants.find(It->first);
            if (Incoming == State.SpillConstants.end()) {
              It = Joined.SpillConstants.erase(It);
              Changed = true;
              continue;
            }
            std::set<uint64_t> Values = It->second;
            Values.insert(Incoming->second.begin(), Incoming->second.end());
            if (Values.size() > MaxTrackedConstants) {
              It = Joined.SpillConstants.erase(It);
              Changed = true;
              continue;
            }
            if (Values != It->second) {
              It->second = std::move(Values);
              Changed = true;
            }
            ++It;
          }
          for (auto It = Joined.EqualRegisters.begin();
               It != Joined.EqualRegisters.end();) {
            if (!State.EqualRegisters.count(*It)) {
              It = Joined.EqualRegisters.erase(It);
              Changed = true;
            } else {
              ++It;
            }
          }
          if (!Changed)
            continue;
          ExistingState->second = Joined;
          State = std::move(Joined);
        }
        if (++ProcessedStates > MaxPathStates) {
          LLVM_DEBUG(dbgs() << "B.ASSEMBLE scalar proof state cap for "
                            << printReg(Parent, TRI) << " init " << *InitMI
                            << "use " << *UseMI);
          ProofCache[UseMI] = UseProof();
          return UseProof();
        }

        Optional<bool> CArg;
        Optional<bool> ConditionalTaken;
        MachineBasicBlock *ConditionalTarget = nullptr;
        Register DirectConditionalReg;
        Optional<unsigned> DirectConditionalOpcode;
        uint64_t DirectConditionalValue = 0;
        Register RegisterConditionalLHS;
        Register RegisterConditionalRHS;
        Optional<unsigned> RegisterConditionalOpcode;
        bool EndedAtUse = false;
        for (MachineInstr &MI : *State.MBB) {
          if (&MI == UseMI) {
            SawUse = true;
            StateKind UseKind = State.Kind;
            if (Requirement == ProofRequirement::OpenParent) {
              if (UseKind != StateKind::Parent ||
                  State.Phase != ParentPhase::Open || State.Def != InitMI) {
                LLVM_DEBUG(dbgs()
                           << "reached continuation without exact OPEN epoch "
                           << printReg(Parent, TRI) << " init " << *InitMI
                           << "state kind " << static_cast<unsigned>(UseKind)
                           << " phase "
                           << static_cast<unsigned>(State.Phase)
                           << " distance " << State.Distance << " use "
                           << MI << "constants";
                           for (const auto &Entry : State.Constants) {
                             dbgs() << " " << printReg(Register(Entry.first), TRI)
                                    << "={";
                             for (uint64_t Value : Entry.second)
                               dbgs() << Value << ",";
                             dbgs() << "}";
                           }
                           dbgs() << "\n";);
                UseProof Failed;
                Failed.Reached = true;
                Failed.Complete = true;
                ProofCache[UseMI] = Failed;
                return Failed;
              }
              if (!ReachingParentDistance)
                ReachingParentDistance = State.Distance;
              else if (*ReachingParentDistance != State.Distance) {
                UseProof Failed;
                Failed.Reached = true;
                Failed.Complete = true;
                ProofCache[UseMI] = Failed;
                return Failed;
              }
            } else if (Requirement == ProofRequirement::ExactClosed ||
                       Requirement ==
                           ProofRequirement::ExactClosedAnyDistance) {
              if (UseKind != StateKind::Parent ||
                  State.Phase != ParentPhase::Closed ||
                  State.Def != InitMI) {
                LLVM_DEBUG(if (Requirement ==
                                   ProofRequirement::ExactClosedAnyDistance)
                               dbgs()
                                   << "ordinary source is not exact CLOSED "
                                      "epoch on one path: kind "
                                   << static_cast<unsigned>(UseKind)
                                   << " phase "
                                   << static_cast<unsigned>(State.Phase)
                                   << " def " << State.Def << " expected "
                                   << InitMI << " at " << MI;);
                UseProof Failed;
                Failed.Reached = true;
                Failed.Complete = true;
                ProofCache[UseMI] = Failed;
                return Failed;
              }
              if (Requirement == ProofRequirement::ExactClosed) {
                if (!ReachingParentDistance)
                  ReachingParentDistance = State.Distance;
                else if (*ReachingParentDistance != State.Distance) {
                  ProofCache[UseMI] = UseProof();
                  return UseProof();
                }
              }
            } else {
              if (!((UseKind == StateKind::Parent &&
                     State.Phase == ParentPhase::Closed) ||
                    UseKind == StateKind::Other)) {
                ProofCache[UseMI] = UseProof();
                return UseProof();
              }
              if (!ReachingUseKind) {
                ReachingUseKind = UseKind;
                ReachingOtherDef = State.Def;
              } else if (*ReachingUseKind != UseKind ||
                         (UseKind == StateKind::Other &&
                          ReachingOtherDef != State.Def)) {
                ProofCache[UseMI] = UseProof();
                return UseProof();
              }
              if (UseKind == StateKind::Parent) {
                if (!ReachingParentDistance)
                  ReachingParentDistance = State.Distance;
                else if (*ReachingParentDistance != State.Distance) {
                  ProofCache[UseMI] = UseProof();
                  return UseProof();
                }
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
            if (Last && *Last) {
              State.Phase = ParentPhase::Closed;
            }
          }

          Optional<std::pair<Register, std::set<uint64_t>>> KnownDef;
          Register EqualSource;
          if (MI.getOpcode() == TargetOpcode::COPY &&
              MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isReg()) {
            Optional<std::set<uint64_t>> Src =
                getConstantSet(State, MI.getOperand(1).getReg());
            if (Src) {
              KnownDef = std::make_pair(MI.getOperand(0).getReg(), *Src);
              EqualSource = MI.getOperand(1).getReg();
            }
          } else if (MI.getOpcode() == LinxV5::ORI && MI.getNumOperands() >= 3 &&
              MI.getOperand(0).isReg() && MI.getOperand(1).isReg() &&
              MI.getOperand(2).isImm()) {
            Optional<std::set<uint64_t>> Src =
                getConstantSet(State, MI.getOperand(1).getReg());
            if (Src) {
              std::set<uint64_t> Values;
              uint64_t Imm =
                  static_cast<uint64_t>(MI.getOperand(2).getImm());
              for (uint64_t Value : *Src)
                Values.insert(Value | Imm);
              if (Values.size() <= MaxTrackedConstants)
                KnownDef = std::make_pair(MI.getOperand(0).getReg(),
                                          std::move(Values));
            }
          } else if (MI.getOpcode() == LinxV5::ADDI &&
                     MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
                     MI.getOperand(1).isReg() && MI.getOperand(2).isImm()) {
            Optional<std::set<uint64_t>> Src =
                getConstantSet(State, MI.getOperand(1).getReg());
            if (Src) {
              std::set<uint64_t> Values;
              uint64_t Imm =
                  static_cast<uint64_t>(MI.getOperand(2).getImm());
              for (uint64_t Value : *Src)
                Values.insert(Value + Imm);
              if (Values.size() <= MaxTrackedConstants)
                KnownDef = std::make_pair(MI.getOperand(0).getReg(),
                                          std::move(Values));
            }
          } else if (MI.getOpcode() == LinxV5::OR_SW &&
                     MI.getNumOperands() >= 4 && MI.getOperand(0).isReg() &&
                     MI.getOperand(1).isReg() &&
                     MI.getOperand(1).getReg() == LinxV5::R0 &&
                     MI.getOperand(2).isReg() && MI.getOperand(3).isImm() &&
                     MI.getOperand(3).getImm() == 0) {
            // Only the unshifted zero-left form is a signed-word copy.
            // General OR_SW preserves the full-width left input and shifts
            // its transformed right input; forget those forms conservatively.
            Register Source = MI.getOperand(2).getReg();
            auto SourceValues = getConstantSet(State, Source);
            if (SourceValues) {
              std::set<uint64_t> Values;
              bool IsExactCopy = true;
              for (uint64_t V : *SourceValues) {
                uint64_t Word = static_cast<uint64_t>(
                    static_cast<int64_t>(static_cast<int32_t>(V)));
                Values.insert(Word);
                IsExactCopy &= Word == V;
              }
              KnownDef = std::make_pair(MI.getOperand(0).getReg(),
                                        std::move(Values));
              if (IsExactCopy)
                EqualSource = Source;
            }
          }

          Optional<SpillSlotKey> ExactSpillLoad =
              getExactSpillAccess(MI, /*IsLoad=*/true);
          if (ExactSpillLoad && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isReg()) {
            auto It = State.SpillConstants.find(*ExactSpillLoad);
            if (It != State.SpillConstants.end())
              KnownDef =
                  std::make_pair(MI.getOperand(0).getReg(), It->second);
          }
          Optional<SpillSlotKey> ExactSpillStore =
              getExactSpillAccess(MI, /*IsLoad=*/false);
          Optional<std::set<uint64_t>> SpillStoreValue;
          if (ExactSpillStore && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isReg())
            SpillStoreValue =
                getConstantSet(State, MI.getOperand(0).getReg());

          bool DefinesCArg = false;
          bool RedefinesDirectConditionalSource = false;
          Optional<bool> NewCArg;
          const bool ModelsCArg =
              MI.getOpcode() == LinxV5::SETC_EQI ||
              MI.getOpcode() == LinxV5::SETC_NEI ||
              MI.getOpcode() == LinxV5::SETC_GEI ||
              MI.getOpcode() == LinxV5::SETC_LTI ||
              MI.getOpcode() == LinxV5::SETC_LTUI;
          if (ModelsCArg &&
              MI.getNumOperands() >= 2 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isImm()) {
            Optional<std::set<uint64_t>> Values =
                getConstantSet(State, MI.getOperand(0).getReg());
            if (Values) {
              int64_t Imm = MI.getOperand(1).getImm();
              Optional<bool> Uniform;
              for (uint64_t Value : *Values) {
                Optional<bool> Result = evaluateImmediatePredicate(
                    MI.getOpcode(), Value, static_cast<uint64_t>(Imm));
                assert(Result && "modeled immediate predicate must evaluate");
                if (!Uniform)
                  Uniform = *Result;
                else if (*Uniform != *Result) {
                  Uniform = None;
                  break;
                }
              }
              NewCArg = Uniform;
            }
            if (MI.getOpcode() == LinxV5::SETC_EQI ||
                MI.getOpcode() == LinxV5::SETC_NEI ||
                MI.getOpcode() == LinxV5::SETC_GEI ||
                MI.getOpcode() == LinxV5::SETC_LTI ||
                MI.getOpcode() == LinxV5::SETC_LTUI) {
              DirectConditionalReg = MI.getOperand(0).getReg();
              DirectConditionalOpcode = MI.getOpcode();
              DirectConditionalValue =
                  static_cast<uint64_t>(MI.getOperand(1).getImm());
            }
          }

          const bool ModelsDirectBranch =
              MI.getOpcode() == LinxV5::SETC_EQI_BR ||
              MI.getOpcode() == LinxV5::SETC_NEI_BR ||
              MI.getOpcode() == LinxV5::SETC_LTI_BR;
          if (ModelsDirectBranch &&
              MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isImm() && MI.getOperand(2).isMBB()) {
            ConditionalTarget = MI.getOperand(2).getMBB();
            DirectConditionalReg = MI.getOperand(0).getReg();
            DirectConditionalOpcode = MI.getOpcode();
            DirectConditionalValue =
                static_cast<uint64_t>(MI.getOperand(1).getImm());
            Optional<std::set<uint64_t>> Values =
                getConstantSet(State, DirectConditionalReg);
            if (Values) {
              uint64_t Imm =
                  static_cast<uint64_t>(MI.getOperand(1).getImm());
              Optional<bool> Uniform;
              for (uint64_t Value : *Values) {
                Optional<bool> Taken = evaluateImmediatePredicate(
                    MI.getOpcode(), Value, Imm);
                assert(Taken && "modeled direct predicate must evaluate");
                if (!Uniform)
                  Uniform = *Taken;
                else if (*Uniform != *Taken) {
                  Uniform = None;
                  break;
                }
              }
              ConditionalTaken = Uniform;
            }
          } else if (MI.getOpcode() == LinxV5::SETC_LTUI_BR &&
                     MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
                     MI.getOperand(2).isMBB()) {
            // This branch is outside the small signed phase-control subset.
            // Explore both edges and forget its induction source so an
            // unrelated high-trip loop cannot manufacture one exact path
            // state per iteration merely because register allocation reused
            // the same physical scalar as a modeled phase counter.
            ConditionalTarget = MI.getOperand(2).getMBB();
            DirectConditionalReg = Register();
            DirectConditionalOpcode = None;
            ConditionalTaken = None;
            invalidateAliases(State, MI.getOperand(0).getReg());
          } else if ((MI.getOpcode() == LinxV5::BEQ ||
                      MI.getOpcode() == LinxV5::BNE) &&
              MI.getNumOperands() >= 3 && MI.getOperand(0).isReg() &&
              MI.getOperand(1).isReg() && MI.getOperand(2).isMBB()) {
            ConditionalTarget = MI.getOperand(2).getMBB();
            // This edge is controlled by the register comparison, regardless
            // of any preceding SETC that left a live predicate in CARG.
            DirectConditionalReg = Register();
            DirectConditionalOpcode = None;
            ConditionalTaken = None;
            RegisterConditionalLHS = MI.getOperand(0).getReg();
            RegisterConditionalRHS = MI.getOperand(1).getReg();
            RegisterConditionalOpcode = MI.getOpcode();
            Optional<std::set<uint64_t>> LHS =
                getConstantSet(State, RegisterConditionalLHS);
            Optional<std::set<uint64_t>> RHS =
                getConstantSet(State, RegisterConditionalRHS);
            if (LHS && RHS) {
              Optional<bool> Uniform;
              bool Mixed = false;
              for (uint64_t L : *LHS) {
                for (uint64_t R : *RHS) {
                  bool Taken =
                      MI.getOpcode() == LinxV5::BEQ ? L == R : L != R;
                  if (!Uniform)
                    Uniform = Taken;
                  else if (*Uniform != Taken) {
                    Uniform = None;
                    Mixed = true;
                    break;
                  }
                }
                if (Mixed)
                  break;
              }
              ConditionalTaken = Uniform;
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
              if (DirectConditionalOpcode &&
                  TRI->regsOverlap(MO.getReg(), DirectConditionalReg))
                RedefinesDirectConditionalSource = true;
              invalidateAliases(State, MO.getReg());
              if (MO.getReg().isPhysical() &&
                  TRI->regsOverlap(MO.getReg(), LinxV5::CARG))
                DefinesCArg = true;
            } else if (MO.isRegMask()) {
              if (DirectConditionalOpcode && DirectConditionalReg.isPhysical() &&
                  MO.clobbersPhysReg(DirectConditionalReg))
                RedefinesDirectConditionalSource = true;
              for (auto It = State.Constants.begin();
                   It != State.Constants.end();) {
                if (MO.clobbersPhysReg(Register(It->first)))
                  It = State.Constants.erase(It);
                else
                  ++It;
              }
              for (auto It = State.EqualRegisters.begin();
                   It != State.EqualRegisters.end();) {
                if (MO.clobbersPhysReg(Register(It->first)) ||
                    MO.clobbersPhysReg(Register(It->second)))
                  It = State.EqualRegisters.erase(It);
                else
                  ++It;
              }
              DefinesCArg |= MO.clobbersPhysReg(LinxV5::CARG);
            }
          }
          if (KnownDef)
            if (RelevantRegs.count(KnownDef->first.id())) {
              State.Constants[KnownDef->first.id()] = KnownDef->second;
              if (EqualSource && EqualSource != KnownDef->first &&
                  !TRI->regsOverlap(EqualSource, KnownDef->first) &&
                  State.Constants.count(EqualSource.id()))
                State.EqualRegisters.insert(std::minmax(
                    KnownDef->first.id(), EqualSource.id()));
            }
          // SETC is the only modeled producer.  Every other explicit or
          // implicit CARG definition, including a regmask clobber, destroys
          // the previous predicate so a later branch explores both edges.
          if (DefinesCArg)
            CArg = None;
          if (ModelsCArg)
            CArg = NewCArg;
          if ((DefinesCArg && !ModelsCArg && !ModelsDirectBranch) ||
              RedefinesDirectConditionalSource) {
            DirectConditionalReg = Register();
            DirectConditionalOpcode = None;
            DirectConditionalValue = 0;
          }

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
                // Even a bit-preserving self-copy publishes a new ordinary
                // identity. It cannot retain the original assembly epoch.
                State.Kind = StateKind::Other;
                State.Phase = ParentPhase::None;
                State.Def = &MI;
                State.Distance = 1;
              } else if (State.Kind == StateKind::Parent) {
                if (++State.Distance > ParentRC->getNumRegs()) {
                  LLVM_DEBUG(if (Requirement ==
                                     ProofRequirement::ExactClosedAnyDistance)
                                 dbgs() << "exact CLOSED epoch left Local "
                                            "tile window before ordinary use "
                                         << *UseMI << "from "
                                         << printMBBReference(*State.MBB)
                                         << " at " << MI;);
                  ProofCache[UseMI] = UseProof();
                  return UseProof();
                }
              }
            }
          }
          if (KillsTrackedParent && State.Kind == StateKind::Parent &&
              State.Phase == ParentPhase::Closed &&
              !isSelfTileCopyOf(MI, Parent)) {
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
          if (ConditionalTarget && DirectConditionalOpcode &&
              Next.Constants.count(DirectConditionalReg.id())) {
            bool TargetEdge = Succ == ConditionalTarget;
            std::set<uint64_t> Filtered;
            for (uint64_t Value :
                 Next.Constants[DirectConditionalReg.id()]) {
              Optional<bool> Taken = evaluateImmediatePredicate(
                  *DirectConditionalOpcode, Value, DirectConditionalValue);
              assert(Taken && "tracked immediate predicate must evaluate");
              if (*Taken == TargetEdge)
                Filtered.insert(Value);
            }
            if (Filtered.empty())
              continue;
            Next.Constants[DirectConditionalReg.id()] = std::move(Filtered);
          } else if (ConditionalTarget && RegisterConditionalOpcode) {
            Optional<std::set<uint64_t>> LHS =
                getConstantSet(Next, RegisterConditionalLHS);
            Optional<std::set<uint64_t>> RHS =
                getConstantSet(Next, RegisterConditionalRHS);
            if (LHS && RHS) {
              bool TargetEdge = Succ == ConditionalTarget;
              bool EqualityEdge =
                  (*RegisterConditionalOpcode == LinxV5::BEQ) == TargetEdge;
              if (RegisterConditionalLHS == RegisterConditionalRHS) {
                if (!EqualityEdge)
                  continue;
              } else if (EqualityEdge) {
                std::set<uint64_t> Intersection;
                std::set_intersection(
                    LHS->begin(), LHS->end(), RHS->begin(), RHS->end(),
                    std::inserter(Intersection, Intersection.begin()));
                if (Intersection.empty())
                  continue;
                Next.Constants[RegisterConditionalLHS.id()] = Intersection;
                Next.Constants[RegisterConditionalRHS.id()] =
                    std::move(Intersection);
              } else if (LHS->size() == 1) {
                std::set<uint64_t> Filtered = *RHS;
                Filtered.erase(*LHS->begin());
                if (Filtered.empty())
                  continue;
                Next.Constants[RegisterConditionalRHS.id()] =
                    std::move(Filtered);
              } else if (RHS->size() == 1) {
                std::set<uint64_t> Filtered = *LHS;
                Filtered.erase(*RHS->begin());
                if (Filtered.empty())
                  continue;
                Next.Constants[RegisterConditionalLHS.id()] =
                    std::move(Filtered);
              }
            }
          }
          // Propagate edge restrictions through proven copy equalities.
          // This retains phase-counter correlations without inferring them
          // merely from independently joined sets with matching members.
          bool EqualitiesChanged = true;
          bool Feasible = true;
          while (EqualitiesChanged && Feasible) {
            EqualitiesChanged = false;
            for (const auto &Equality : Next.EqualRegisters) {
              auto L = Next.Constants.find(Equality.first);
              auto R = Next.Constants.find(Equality.second);
              if (L == Next.Constants.end() || R == Next.Constants.end())
                continue;
              std::set<uint64_t> Intersection;
              std::set_intersection(
                  L->second.begin(), L->second.end(), R->second.begin(),
                  R->second.end(),
                  std::inserter(Intersection, Intersection.begin()));
              if (Intersection.empty()) {
                Feasible = false;
                break;
              }
              if (L->second != Intersection || R->second != Intersection) {
                L->second = Intersection;
                R->second = std::move(Intersection);
                EqualitiesChanged = true;
              }
            }
          }
          if (!Feasible)
            continue;
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
      ProofCache[UseMI] = Proof;
      return Proof;
    };

    if (CollectClosedTailProofs) {
      for (MachineBasicBlock &MBB : MF) {
        auto Before = MBB.end();
        for (auto I = MBB.end(); I != MBB.begin(); --I) {
          if (LinxV5::isTileOp(*std::prev(I))) {
            Before = I;
            break;
          }
          Before = MBB.begin();
        }
        // An end-of-block point has no stable instruction anchor.  Reject it
        // conservatively instead of proving a different insertion position.
        if (Before == MBB.end())
          continue;
        UseProof Proof =
            proveUse(&*Before, ProofRequirement::ExactClosed);
        if (Proof.Proven && Proof.Complete) {
          ClosedAssembleParentsAtTail[&MBB].insert(InitMI);
          LLVM_DEBUG(dbgs() << "proved CLOSED assembly epoch at tail of "
                            << printMBBReference(MBB) << ": " << *InitMI);
        }
      }
      continue;
    }

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
          UseProof Proof =
              proveUse(&MI, ProofRequirement::OpenParent);
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
             (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
              MI.getOpcode() == LinxV5::PseudoPCOPYDesc)) &&
            State.Kind == StateKind::Parent && State.Phase == ParentPhase::Open)
          report_fatal_error(
              "cannot copy an OPEN B.ASSEMBLE parent source before LAST");

        // A physical hand may be reused by several independent assembly
        // generations.  Only the B.ASSEMBLE ParentRef operand continues the
        // generation tracked by this dataflow instance.  Every other use is
        // an ordinary tile source and is legal only after every reaching path
        // has LAST-closed the tracked generation.  This keeps a closed child
        // usable as the writer for its next hierarchy level without mistaking
        // that source for the child's old ParentRef epoch.
        if (RecordUses && UsesParentRegister && !UsesParentRef) {
          UseProof Proof =
              proveUse(&MI, ProofRequirement::PublishedSource);
          ++OrdinaryAnalysisCounts[&MI][Parent];
          if (Proof.Reached)
            ++OrdinaryReachedCounts[&MI][Parent];
          if (!Proof.Complete)
            IncompleteOrdinaryUses[&MI].insert(Parent);
          if (!Proof.Complete && State.Kind == StateKind::Conflict)
            ConflictOrdinaryUses[&MI].insert(Parent);
          if (!Proof.Complete && State.Kind == StateKind::Parent &&
              State.Phase != ParentPhase::Closed)
            OpenOrdinaryUses[&MI].insert(Parent);
          if (Proof.Reached && !Proof.Proven)
            ReachedUnprovedOrdinaryUses[&MI].insert(Parent);
          if (Proof.Proven && Proof.Distance != 0) {
            unsigned &Agreed = OrdinaryProofDistances[&MI][Parent];
            if (Agreed && Agreed != Proof.Distance)
              report_fatal_error(
                  "B.ASSEMBLE ordinary source has conflicting native "
                  "distances across parent epochs");
            Agreed = Proof.Distance;
          }
          if (Proof.Proven && Proof.Reached && Proof.Distance == 0)
            ++OrdinaryOtherReachedCounts[&MI][Parent];
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
                  State.Phase == ParentPhase::Closed &&
                  isSelfTileCopyOf(MI, Parent)) {
                State.Kind = StateKind::Other;
                State.Def = &MI;
                State.Phase = ParentPhase::None;
                State.Distance = 1;
              } else if (State.Kind == StateKind::Parent &&
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
              const bool IsOrdinaryQueueRotationCopy =
                  (MI.getOpcode() == LinxV5::PseudoTCOPYDesc ||
                   MI.getOpcode() == LinxV5::PseudoPCOPYDesc) &&
                  MI.getNumOperands() > 2 && MI.getOperand(0).isReg() &&
                  MI.getOperand(2).isReg() &&
                  MI.getOperand(0).getReg() == MI.getOperand(2).getReg() &&
                  Def == MI.getOperand(0).getReg();
              if (State.Kind == StateKind::Parent &&
                  State.Phase != ParentPhase::Closed && RecordUses &&
                  !IsOrdinaryQueueRotationCopy)
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
            State.Phase == ParentPhase::Closed &&
            !isSelfTileCopyOf(MI, Parent))
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

  if (CollectClosedTailProofs)
    return;

  for (const auto &MIEntry : OrdinaryAnalysisCounts) {
    const MachineInstr *MI = MIEntry.first;
    for (const auto &ParentEntry : MIEntry.second) {
      Register Parent = ParentEntry.first;
      unsigned Analyses = ParentEntry.second;
      unsigned CandidateEpochs = ParentEpochCounts.lookup(Parent.id());
      bool Incomplete = IncompleteOrdinaryUses[MI].count(Parent);
      bool ReachedUnproved =
          ReachedUnprovedOrdinaryUses[MI].count(Parent);
      if (ConflictOrdinaryUses[MI].count(Parent))
        report_fatal_error(
            "B.ASSEMBLE parent has conflicting relative distances at a CFG "
            "join");
      if (OpenOrdinaryUses[MI].count(Parent))
        report_fatal_error(
            "cannot use an OPEN B.ASSEMBLE parent as an ordinary tile source "
            "before LAST");
      if (Analyses != CandidateEpochs || Incomplete)
        report_fatal_error(
            "B.ASSEMBLE ordinary source phase proof did not complete for "
            "every candidate epoch");
      if (ReachedUnproved)
        report_fatal_error(
            "cannot use an OPEN B.ASSEMBLE parent as an ordinary tile source "
            "before LAST");
      unsigned Reached = OrdinaryReachedCounts[MI].lookup(Parent);
      unsigned ReachedOther =
          OrdinaryOtherReachedCounts[MI].lookup(Parent);
      unsigned Distance = OrdinaryProofDistances[MI].lookup(Parent);
      if (Distance && ReachedOther)
        report_fatal_error(
            "B.ASSEMBLE ordinary source mixes a CLOSED parent epoch with a "
            "different ordinary definition");
      if (Distance) {
        AssembleOrdinaryUseDistances[MI][Parent] = Distance;
        continue;
      }
      if (ReachedOther) {
        if (ReachedOther != Reached)
          report_fatal_error(
              "B.ASSEMBLE ordinary source has inconsistent reaching "
              "definitions across parent epochs");
        // A real ordinary definition reached the use.  Leave this operand to
        // the normal descriptor-aware queue identity rewrite.
        continue;
      }
      if (Reached)
        report_fatal_error(
            "B.ASSEMBLE ordinary source reached without a native distance");
      // Every bounded epoch analysis completed without reaching this
      // syntactic ordinary use.  Its encoded source is unobservable, but it
      // must still be a legal native offset rather than an absolute Tile
      // register.
      AssembleOrdinaryUseDistances[MI][Parent] = 1;
    }
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
  PureAssembleRCs.clear();
  ClosedAssembleParentsAtTail.clear();
  RewriteTRIdentities.clear();
  QueueCopyUseDistances.clear();
  QueueDeadPublicationDefs.clear();
  CanonicalEntryContracts.clear();
  CanonicalEntryLiveIns.clear();
  CanonicalRewriteEntryStates.clear();

  prepareLocalTileABIBoundaries(MF, *TII, *TRI);

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

  // A hand used exclusively by OPEN-generating INITs is identity-reserved.
  // Ordinary alignment copies would churn another live OPEN epoch on a loop
  // backedge. Keep ordinary references absolute in these structurally proven
  // hands during scheduling. Phase 2 still lowers every definition and uses
  // separately proved ParentRef/ordinary-source distances.
  // Mixed hands (including hand-written MIR) retain the existing strict path.
  for (const TargetRegisterClass *RC :
       {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
        &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass}) {
    bool HasOpenInit = false;
    bool OnlyOpenInits = true;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        for (const MachineOperand &MO : MI.operands())
          if (MO.isRegMask())
            for (MCPhysReg Reg : RC->getRegisters())
              if (MO.clobbersPhysReg(Reg))
                OnlyOpenInits = false;
        if (MI.isImplicitDef() || MI.getOpcode() == LinxV5::PseudoEmptyTile ||
            getTSlotOccupation(MI, RC) == 0)
          continue;
        Optional<bool> Init = LinxV5::getAssembleControlBit(MI, 0);
        Optional<bool> Last = LinxV5::getAssembleControlBit(MI, 1);
        bool IsOpenInit = Init && *Init && Last && !*Last &&
                          getTSlotOccupation(MI, RC) == 1;
        HasOpenInit |= IsOpenInit;
        OnlyOpenInits &= IsOpenInit;
      }
    }
    if (HasOpenInit && OnlyOpenInits) {
      SyncConflictRCs.insert(RC);
      PureAssembleRCs.insert(RC);
    }
  }

  unsigned OldNumCopy = NumCopy;

  LLVM_DEBUG(dbgs() << "before insert copy:\n"; MF.dump());

  // Prove exact CLOSED epochs at the real tail-copy insertion points before
  // Phase 1 mutates the CFG queue.  These proofs allow only ordinary payload
  // relocation; post-copy ParentRef distances are solved again below.
  solveAssembleParentDataflow(MF, /*CollectClosedTailProofs=*/true);
  // Detect ordinary CLOSED joins before queue synchronization can introduce
  // asymmetric copies or churn a different OPEN epoch out of its hand.
  // ParentRef distance proofs remain exact and are recomputed after Phase 1.
  solveAssembleParentDataflow(MF);

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

  normalizeEntryContracts(MF);

  // Clockhands reserves an OPEN parent's hand before register allocation.
  // Validate the final Phase-1 machine function, including every inserted
  // copy, before relying on that isolation in the relative-window rewrite.
  solveAssembleParentDataflow(MF);

  LLVM_DEBUG(dbgs() << "copys " << NumCopy - OldNumCopy << " total "
                    << MF.getInstructionCount() << "\n");
  LLVM_DEBUG(dbgs() << "after insert copy:\n"; MF.dump());

  legalityCheck(MF);

  // So that we can use absolute register for LLVM-MCA to build DEF-USE Chain.
  if (!EnableReg2Offset) {
    eraseLocalTileABISeeds(MF);
    return true;
  }

  // Snapshot every Local-tile operand's logical queue identity before Phase
  // 2 mutates predecessor register operands.  Descriptor resolution walks
  // reaching definitions across the CFG; resolving on demand after an
  // earlier block has already been rewritten loses those allocated register
  // names.  The immutable snapshot also lets two descriptor-distinct values
  // allocated to one physical hand remain separately addressable.
  RewriteTRIdentities.clear();
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (canSkipMI(MI) || MI.isKill() || MI.isImplicitDef() ||
          MI.getOpcode() == LinxV5::PseudoEmptyTile)
        continue;
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || MO.isImplicit() ||
            !LinxV5::Tile_ABSRegClass.contains(MO.getReg()))
          continue;
        const TargetRegisterClass *OperandRC = nullptr;
        for (const TargetRegisterClass *CandidateRC :
             {&LinxV5::Tile_TRRegClass, &LinxV5::Tile_URRegClass,
              &LinxV5::Tile_MRRegClass, &LinxV5::Tile_NRRegClass})
          if (CandidateRC->contains(MO.getReg())) {
            OperandRC = CandidateRC;
            break;
          }
        if (OperandRC && PureAssembleRCs.count(OperandRC))
          continue;
        if (isAssembleParentDefinition(MI, MO))
          continue;
        if (MO.isUse() && MI.isInlineAsm() &&
            StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
                .contains("B.ASSEMBLE") &&
            MO.getReg() == LinxV5::getAssembleParentRef(MI))
          continue;
        if (MO.isDef() && MO.isDead() &&
            MI.getOpcode() == LinxV5::PseudoTCOPY) {
          QueueDeadPublicationDefs.insert(&MO);
          continue;
        }
        if (MO.isDef() && QueueDeadPublicationDefs.count(&MO))
          continue;
        if (MO.isUse() && QueueCopyUseDistances.count(&MO))
          continue;
        if (MO.isUse()) {
          MachineOperand *ParentRef = getAssembleParentRefOperand(MI);
          auto ParentUse = AssembleParentUseDistances.find(&MI);
          if (&MO == ParentRef && ParentUse != AssembleParentUseDistances.end() &&
              ParentUse->second.count(MO.getReg()))
            continue;
          auto OrdinaryUse = AssembleOrdinaryUseDistances.find(&MI);
          if (OrdinaryUse != AssembleOrdinaryUseDistances.end() &&
              OrdinaryUse->second.count(MO.getReg()))
            continue;
        }
        Optional<LinxRegOp> Identity =
            getTileQueueIdentity(MBB, MI, MO, MO.isDef());
        if (!Identity && MO.isUse() && OperandRC &&
            CanonicalRewriteEntryStates.count(&MBB) &&
            CanonicalRewriteEntryStates[&MBB].count(OperandRC)) {
          TRegIndexState &Entry =
              CanonicalRewriteEntryStates[&MBB][OperandRC].first;
          auto Reaching = findLatestRegister(Entry, MO.getReg());
          if (Reaching != Entry.end() && Reaching->second != -1u)
            Identity = Reaching->first;
        }
        if (!Identity && MO.isDef())
          Identity = LinxRegOp(
              MO.getReg(), LinxV5::getTileOpRegSize(MI, MO.getReg()));
        if (!Identity) {
          LLVM_DEBUG(dbgs() << "unresolved Phase-2 Tile identity operand "
                            << MO << " in " << MI);
          report_fatal_error(
              "cannot snapshot exact Local tile queue identity before "
              "offset rewrite");
        }
        RewriteTRIdentities[&MO] = *Identity;
      }
    }
  }
  simulateAndValidateQueues(MF);

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

  eraseLocalTileABISeeds(MF);

  return true;
}

bool hasImplicitDef(const TRegIndexState &TRegIdx) {
  for (auto &RI : TRegIdx) {
    if (RI.second == -1)
      return true;
  }
  return false;
}

static Optional<LinxV5::TileDescriptor>
getTCOPYDescriptor(const MachineInstr &MI) {
  if ((MI.getOpcode() != LinxV5::PseudoTCOPYDesc &&
       MI.getOpcode() != LinxV5::PseudoPCOPYDesc) ||
      MI.getNumOperands() < 8)
    return None;
  for (unsigned I : {1u, 3u, 4u, 5u, 6u, 7u})
    if (!MI.getOperand(I).isImm())
      return None;
  LinxV5::TileDescriptor Descriptor;
  Descriptor.SizeCode = MI.getOperand(1).getImm();
  Descriptor.ValidCol = MI.getOperand(3).getImm();
  Descriptor.ValidRow = MI.getOperand(4).getImm();
  Descriptor.PhysicalCol = MI.getOperand(5).getImm();
  Descriptor.DataType = MI.getOperand(6).getImm();
  Descriptor.Layout = MI.getOperand(7).getImm();
  Descriptor.PackedPredicate = MI.getOpcode() == LinxV5::PseudoPCOPYDesc;
  return Descriptor;
}

static Optional<LinxRegOp>
getTileQueueIdentity(MachineBasicBlock &MBB, MachineInstr &MI,
                     const MachineOperand &MO, bool IsDef) {
  Optional<LinxV5::TileDescriptor> Descriptor = getTCOPYDescriptor(MI);
  if (!Descriptor) {
    MachineBasicBlock::iterator Position = MI.getIterator();
    if (IsDef)
      ++Position;
    Descriptor = LinxV5::tryGetTileRegDescriptor(MBB, Position, MO.getReg());
  }
  if (!Descriptor)
    return None;
  LinxRegOp Identity(MO.getReg(), Descriptor->SizeCode);
  Identity.Descriptor = *Descriptor;
  return Identity;
}

static TRegIndexState::iterator findExactIdentity(TRegIndexState &State,
                                                   const LinxRegOp &Identity) {
  for (auto I = State.end(); I != State.begin();) {
    --I;
    if (I->first == Identity)
      return I;
  }
  return State.end();
}

static TRegIndexState::iterator findLatestRegister(TRegIndexState &State,
                                                   Register Reg) {
  for (auto I = State.end(); I != State.begin();) {
    --I;
    if (I->first.Reg == Reg)
      return I;
  }
  return State.end();
}

static void eraseRegisterIdentities(TRegIndexState &State, Register Reg) {
  llvm::erase_if(State,
                 [&](const auto &Entry) { return Entry.first.Reg == Reg; });
}

static void resetStateAtLocalTileABISeed(
    MachineInstr &MI, const TargetRegisterClass *RC,
    TRegIndexState &State, unsigned &CurIdx,
    const MachineRegisterInfo *MRI) {
  State.clear();
  CurIdx = RC->getNumRegs();
  if (MI.getNumOperands() == 0 || !MI.getOperand(0).isReg() ||
      !isRC(MI.getOperand(0).getReg(), RC, MRI))
    return;
  Optional<LinxV5::TileDescriptor> Descriptor =
      getLocalTileABISeedDescriptor(MI);
  if (!Descriptor)
    report_fatal_error("malformed Local Tile ABI seed");
  LinxRegOp Imported(MI.getOperand(0).getReg(), Descriptor->SizeCode);
  Imported.Descriptor = *Descriptor;
  State.emplace_back(Imported, CurIdx - 1);
}

void LinxV5TRegToOffsetOpt::rewriteMBB(MachineBasicBlock &MBB) {
  for (auto RCInfo : RCsInfo) {
    const TargetRegisterClass *RC = RCInfo.first;
    TRegIndexState TRegIdx;
    unsigned CurIdx = 0;
    if (PureAssembleRCs.count(RC)) {
      // Disabling alignment does not disable native register lowering. Every
      // use in a pure parent hand has an independently proved epoch distance;
      // real definitions must still become the architectural destination hand.
      Register OffsetBase = RCInfo.second[1];
      for (MachineInstr &MI : MBB) {
        if (canSkipMI(MI) || MI.isKill() || MI.isImplicitDef() ||
            MI.getOpcode() == LinxV5::PseudoEmptyTile)
          continue;
        MachineOperand *ParentRef = getAssembleParentRefOperand(MI);
        SmallVector<MachineOperand *, 2> Defs;
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || MO.isImplicit() || !RC->contains(MO.getReg()))
            continue;
          if (MO.isDef()) {
            Defs.push_back(&MO);
            continue;
          }
          if (!MO.isUse())
            continue;
          auto &Distances = &MO == ParentRef ? AssembleParentUseDistances
                                            : AssembleOrdinaryUseDistances;
          auto Use = Distances.find(&MI);
          if (Use == Distances.end() || !Use->second.count(MO.getReg()))
            report_fatal_error(
                "reserved B.ASSEMBLE hand use lacks one proved native distance");
          unsigned Distance = Use->second.lookup(MO.getReg());
          if (Distance < 1 || Distance > RC->getNumRegs())
            report_fatal_error("reserved B.ASSEMBLE hand use exceeds its window");
          MO.setReg(OffsetBase + Distance - 1);
        }
        for (MachineOperand *Def : Defs)
          Def->setReg(RCInfo.second[0]);
      }
      continue;
    }
    if (SyncConflictRCs.count(RC)) {
      if (isTileReg(RC))
        report_fatal_error(
            "Local tile queue synchronization conflict has no certified "
            "native offset rewrite");
      // Keep ordinary values absolute when queue synchronization is disabled,
      // but an OPEN assembly ParentRef is architecturally relative and has a
      // separately proved reaching-INIT distance. Rewrite only those exact
      // uses; solveAssembleParentDataflow already rejected unresolved or
      // conflicting join distances.
      Register OffsetBase = RCInfo.second[1];
      for (MachineInstr &MI : MBB) {
        MachineOperand *ExactParentRef = getAssembleParentRefOperand(MI);
        if (!ExactParentRef)
          continue;
        auto UseIt = AssembleParentUseDistances.find(&MI);
        if (UseIt == AssembleParentUseDistances.end())
          continue;
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isUse() || MO.isImplicit())
            continue;
          if (&MO != ExactParentRef)
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
                        << ": legacy non-Tile absolute values\n");
      continue;
    }
    LLVM_DEBUG(dbgs() << "rewrite " << TRI->getRegClassName(RC) << " in "
                      << printMBBReference(MBB) << "\n");
    if (isTileReg(RC)) {
      if (!MBB.pred_empty()) {
        auto MBBState = CanonicalRewriteEntryStates.find(&MBB);
        if (MBBState != CanonicalRewriteEntryStates.end() &&
            MBBState->second.count(RC)) {
          TRegIdx = MBBState->second.find(RC)->second.first;
          CurIdx = MBBState->second.find(RC)->second.second;
        } else {
          bool HasTileOperand = llvm::any_of(MBB, [&](MachineInstr &MI) {
            if (MI.isImplicitDef() || MI.getOpcode() == LinxV5::PseudoEmptyTile)
              return false;
            return llvm::any_of(MI.operands(), [&](const MachineOperand &MO) {
              return MO.isReg() && !MO.isImplicit() &&
                     isRC(MO.getReg(), RC, MRI);
            });
          });
          if (HasTileOperand)
            report_fatal_error(
                "Local tile block lacks a certified canonical rewrite entry");
        }
      }
      rewriteMBB(MBB, TRegIdx, CurIdx, RC, RCInfo.second[0],
                 RCInfo.second[1]);
      RewriteTRDefInfo[&MBB][RC] = std::make_pair(TRegIdx, CurIdx);
    } else {
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
                       << printReg(RI.first.Reg, TRI) << "/" << RI.first.Size
                       << "(" << RI.second << ") ");
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
    }
  }
}

void LinxV5TRegToOffsetOpt::rewriteMBB(
    MachineBasicBlock &MBB, TRegIndexState &TRegIdx,
    unsigned &CurIdx, const TargetRegisterClass *RC, Register Out,
    Register OffsetBase) {
  LLVM_DEBUG(dbgs() << ">> preform rewrite \n");

  for (MachineInstr &MI : MBB) {
    if (MI.getOpcode() == LinxV5::PseudoLocalTileABISeed) {
      resetStateAtLocalTileABISeed(MI, RC, TRegIdx, CurIdx, MRI);
      continue;
    }
    if (MI.isKill()) {
      Register Reg = MI.getOperand(0).getReg();
      if (isRC(Reg, RC, MRI))
        eraseRegisterIdentities(TRegIdx, Reg);
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
      if (isRC(Reg, RC, MRI)) {
        eraseRegisterIdentities(TRegIdx, Reg);
        TRegIdx.emplace_back(LinxRegOp(Reg, LinxV5::SIMT_REG_SIZE_D), -1u);
      }
      continue;
    }

    LLVM_DEBUG(dbgs() << "   rewrite " << MI);
    unsigned Occ = getTSlotOccupation(MI, RC);
    SmallVector<MachineOperand *, 2> TRDefs;
    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg()) {
        continue;
      } else if (MO.isImplicit() && (MI.isCall() || MI.isReturn())) {
        // ABI dependencies constrain scheduling/liveness but have no encoded
        // Local register operand to rewrite.
        continue;
      } else if (MO.isUse()) {
        if (isRC(MO.getReg(), RC, MRI)) {
          Register OriginalReg = MO.getReg();
          auto UseIt = AssembleParentUseDistances.find(&MI);
          if (&MO != getAssembleParentRefOperand(MI))
            UseIt = AssembleParentUseDistances.end();
          if (UseIt != AssembleParentUseDistances.end()) {
            auto DistanceIt = UseIt->second.find(OriginalReg);
            if (DistanceIt != UseIt->second.end()) {
              unsigned Distance = DistanceIt->second;
              assert(Distance >= 1 && Distance <= RC->getNumRegs());
              MO.setReg(OffsetBase + (Distance - 1));
              continue;
            }
          }
          auto OrdinaryUseIt = AssembleOrdinaryUseDistances.find(&MI);
          if (OrdinaryUseIt != AssembleOrdinaryUseDistances.end()) {
            auto DistanceIt = OrdinaryUseIt->second.find(OriginalReg);
            if (DistanceIt != OrdinaryUseIt->second.end()) {
              unsigned Distance = DistanceIt->second;
              assert(Distance >= 1 && Distance <= RC->getNumRegs());
              MO.setReg(OffsetBase + (Distance - 1));
              continue;
            }
          }
          auto CopyDistance = QueueCopyUseDistances.find(&MO);
          if (CopyDistance != QueueCopyUseDistances.end()) {
            unsigned Distance = CopyDistance->second;
            if (Distance < 1 || Distance > RC->getNumRegs())
              report_fatal_error(
                  "compiler-inserted Local tile copy source exceeds window");
            MO.setReg(OffsetBase + (Distance - 1));
            continue;
          }
          TRegIndexState::iterator Reaching = TRegIdx.end();
          if (isTileReg(RC)) {
            auto Identity = RewriteTRIdentities.find(&MO);
            if (Identity == RewriteTRIdentities.end())
              report_fatal_error(
                  "cannot prove exact Local tile queue identity for use");
            Reaching = findExactIdentity(TRegIdx, Identity->second);
            if (Reaching == TRegIdx.end() &&
                findLatestRegister(TRegIdx, MO.getReg()) != TRegIdx.end())
              report_fatal_error(
                  "Local tile queue descriptor does not match reaching value");
          } else {
            Reaching = findLatestRegister(TRegIdx, MO.getReg());
          }
          if (Reaching == TRegIdx.end()) {
            if (isTileReg(RC))
              report_fatal_error(
                  "Local tile use lacks one certified canonical source");
            // After coalescer, some tile vregs may be used without a prior
            // def in this block's tracked TRegIdx (e.g. live-in from a
            // block we couldn't pick a pred for). Ordinary uses retain their
            // absolute register.  OPEN assembly parents are never allowed to
            // fall back: their use must have a solved reaching-INIT distance.
            if (&MO == getAssembleParentRefOperand(MI))
              report_fatal_error(
                  "OPEN B.ASSEMBLE parent use lacks solved relative distance");
            continue;
          }
          int64_t Index = CurIdx - Reaching->second;
          LLVM_DEBUG(dbgs()
                     << "   " << printReg(MO.getReg(), TRI) << " CurIdx "
                     << CurIdx << " LastIdx " << Reaching->second << "\n");
          assert(Index <= RC->getNumRegs());
          Register NewReg = OffsetBase + (Index - 1);
          MO.setReg(NewReg);
        }
      } else if (MO.isDef() && isRC(MO.getReg(), RC, MRI)) {
        TRDefs.push_back(&MO);
      }
    }

    for (auto *Def : TRDefs) {
      if (isTileReg(RC) && isAssembleParentDefinition(MI, *Def)) {
        Def->setReg(Out);
        ++CurIdx;
        continue;
      }
      if (isTileReg(RC) && QueueDeadPublicationDefs.count(Def)) {
        LLVM_DEBUG(dbgs() << "   publish dead queue hole at " << CurIdx
                          << "\n");
        Def->setReg(Out);
        ++CurIdx;
        continue;
      }
      LinxRegOp Identity(Def->getReg(), LinxV5::SIMT_REG_SIZE_D);
      if (isTileReg(RC)) {
        auto TileIdentity = RewriteTRIdentities.find(Def);
        if (TileIdentity == RewriteTRIdentities.end())
          report_fatal_error(
              "cannot prove exact Local tile queue identity for definition");
        Identity = TileIdentity->second;
        // An IMPLICIT_DEF is only a predecessor-selection marker.  A real
        // definition replaces it, while older descriptor-distinct queue
        // entries remain addressable for compiler-inserted rotations.
        llvm::erase_if(TRegIdx, [&](const auto &Entry) {
          return Entry.first.Reg == Identity.Reg && Entry.second == -1u;
        });
      } else {
        eraseRegisterIdentities(TRegIdx, Identity.Reg);
      }
      auto Existing = findExactIdentity(TRegIdx, Identity);
      if (Existing == TRegIdx.end())
        TRegIdx.emplace_back(Identity, CurIdx);
      else
        Existing->second = CurIdx;
      LLVM_DEBUG(dbgs() << "   setIdx " << printReg(Def->getReg(), TRI) << " "
                        << CurIdx << "\n");
      Def->setReg(Out);
      CurIdx++;
    }
  }
}
