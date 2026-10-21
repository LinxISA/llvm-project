//===----------------------- LinxV5TileFixup.cpp -------------------------===//
//
// Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// Move TileReg COPYs and spills to the block boundary. To avoid break scalar-tr
// live-ranges.
//   ......
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include <functional>
using namespace llvm;

#define DEBUG_TYPE "linxv5-tile-fixup"
#define PASS_NAME "LinxV5 Tile Fixup"

namespace {

class LinxV5TileFixup : public MachineFunctionPass {
public:
  static char ID;

  LinxV5TileFixup() : MachineFunctionPass(ID) {
    initializeLinxV5TileFixupPass(*PassRegistry::getPassRegistry());
  }
  virtual ~LinxV5TileFixup() = default;

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool Hoist(MachineBasicBlock &MBB);

  bool AnnotateTileSizes(MachineFunction &MF);
  bool AnnotateTileSpillDescriptors(MachineFunction &MF);

  bool isTileReg(Register Reg) {
    return LinxV5::Tile_ABSRegClass.contains(Reg);
  }

  bool isLocalTileCopy(const MachineInstr &MI) {
    if (!MI.isCopy() || MI.getNumExplicitOperands() < 2 ||
        !MI.getOperand(0).isReg() || !MI.getOperand(1).isReg())
      return false;
    const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    auto isLocal = [&MRI](Register Reg) {
      if (Reg.isPhysical())
        return LinxV5::Tile_ABSRegClass.contains(Reg);
      return LinxV5::Tile_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg));
    };
    return isLocal(Dst) && isLocal(Src);
  }

  bool DependTileRegs(const MachineInstr &MI) {
    if (LinxV5::isTileOp(MI))
      return true;
    if (MI.isImplicitDef() && isTileReg(MI.getOperand(0).getReg()))
      return true;
    if (MI.isKill() && isTileReg(MI.getOperand(0).getReg()))
      return true;
    return false;
  }

private:
  MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
};
char LinxV5TileFixup::ID = 0;

bool LinxV5TileFixup::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  MRI = &MF.getRegInfo();
  TII = MF.getSubtarget().getInstrInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= Hoist(MBB);
  }
  Changed |= AnnotateTileSpillDescriptors(MF);
  Changed |= AnnotateTileSizes(MF);

  return Changed;
}

bool LinxV5TileFixup::AnnotateTileSpillDescriptors(MachineFunction &MF) {
  DenseSet<int> SpillFIs;
  bool Changed = false;

  auto CapacityBytes = [](const LinxV5::TileDescriptor &D) -> uint64_t {
    if (D.PackedPredicate)
      report_fatal_error(
          "packed Predicate Tile spilling has no storage-kind-preserving reload");
    if (D.SizeCode < 1 || D.SizeCode > 10)
      report_fatal_error("raw Tile spill SizeCode must be 1..10");
    return uint64_t{1} << (D.SizeCode + 6);
  };
  auto FixedFI = [](const MachineMemOperand *MMO) -> Optional<int> {
    const auto *FS = dyn_cast_or_null<FixedStackPseudoSourceValue>(
        MMO ? MMO->getPseudoValue() : nullptr);
    return FS ? Optional<int>(FS->getFrameIndex()) : None;
  };
  auto RewriteMemRef = [&](MachineInstr &MI, int FI, uint64_t Bytes,
                           bool IsStore) {
    MachineMemOperand *MMO = MF.getMachineMemOperand(
        MachinePointerInfo::getFixedStack(MF, FI),
        IsStore ? MachineMemOperand::MOStore : MachineMemOperand::MOLoad, Bytes,
        MF.getFrameInfo().getObjectAlign(FI));
    SmallVector<MachineMemOperand *, 1> MMOs{MMO};
    MI.setMemRefs(MF, MMOs);
  };
  auto ValidateInputMemRef = [&](MachineInstr &MI, int FI, bool IsStore) {
    if (std::distance(MI.memoperands_begin(), MI.memoperands_end()) != 1)
      report_fatal_error("raw Tile spill requires exactly one fixed-stack MMO");
    MachineMemOperand *MMO = *MI.memoperands_begin();
    if (FixedFI(MMO) != Optional<int>(FI) || MMO->getOffset() != 0 ||
        MMO->isStore() != IsStore || MMO->isLoad() == IsStore)
      report_fatal_error(
          "raw Tile spill MMO identity or direction disagrees with its FI");
  };
  auto DescriptorFromPseudo = [](const MachineInstr &MI) {
    return LinxV5::TileDescriptor{
        static_cast<unsigned>(MI.getOperand(5).getImm()),
        static_cast<unsigned>(MI.getOperand(6).getImm()),
        static_cast<unsigned>(MI.getOperand(3).getImm()),
        static_cast<unsigned>(MI.getOperand(2).getImm()),
        static_cast<unsigned>(MI.getOperand(4).getImm()),
        static_cast<unsigned>(MI.getOperand(7).getImm())};
  };

  enum class AssemblyKind {
    Uninitialized,
    Absent,
    Ordinary,
    Open,
    Closed,
    DefinitelySafe,
    Conflict
  };
  struct AssemblyState {
    AssemblyKind Kind = AssemblyKind::Uninitialized;
    const MachineInstr *Epoch = nullptr;
  };
  auto SameAssemblyState = [](const AssemblyState &A, const AssemblyState &B) {
    return A.Kind == B.Kind && A.Epoch == B.Epoch;
  };
  auto IsAssemblyClosedAt = [&](MachineBasicBlock &TargetMBB,
                                MachineBasicBlock::iterator Before,
                                Register Reg) {
    DenseMap<MachineBasicBlock *, AssemblyState> In, Out;
    auto Transfer = [&](MachineBasicBlock &MBB, AssemblyState S,
                        MachineBasicBlock::iterator Stop) {
      for (auto It = MBB.begin(); It != Stop; ++It) {
        MachineInstr &MI = *It;
        bool IsAssemble =
            MI.isInlineAsm() &&
            StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
                .contains("B.ASSEMBLE");
        bool DefinesReg = llvm::any_of(MI.operands(), [&](MachineOperand &MO) {
          return MO.isReg() && MO.isDef() && !MO.isImplicit() &&
                 MO.getReg() == Reg;
        });
        if (IsAssemble && DefinesReg) {
          Optional<bool> Last =
              LinxV5::getAssembleControlBit(MI, /*OperandIndex=*/1);
          S.Kind = Last && *Last ? AssemblyKind::Closed : AssemblyKind::Open;
          S.Epoch = &MI;
          continue;
        }
        if (LinxV5::getAssembleParentRef(MI) == Reg) {
          Optional<bool> Last =
              LinxV5::getAssembleControlBit(MI, /*OperandIndex=*/1);
          if (!Last || !S.Epoch || S.Kind == AssemblyKind::Conflict ||
              S.Kind == AssemblyKind::Uninitialized ||
              S.Kind == AssemblyKind::Absent ||
              S.Kind == AssemblyKind::Ordinary) {
            S.Kind = AssemblyKind::Conflict;
            S.Epoch = nullptr;
          } else if (*Last) {
            S.Kind = AssemblyKind::Closed;
          }
          continue;
        }
        if (DefinesReg) {
          S.Kind = AssemblyKind::Ordinary;
          S.Epoch = &MI;
        }
      }
      return S;
    };

    bool Progress = true;
    while (Progress) {
      Progress = false;
      for (MachineBasicBlock &MBB : MF) {
        AssemblyState Joined;
        bool SawInitializedPredecessor = false;
        for (MachineBasicBlock *Pred : MBB.predecessors()) {
          AssemblyState P = Out.lookup(Pred);
          if (P.Kind == AssemblyKind::Uninitialized)
            continue;
          if (!SawInitializedPredecessor) {
            Joined = P;
            SawInitializedPredecessor = true;
          } else if (!SameAssemblyState(Joined, P)) {
            bool JoinedSafe = Joined.Kind == AssemblyKind::Closed ||
                              Joined.Kind == AssemblyKind::Ordinary ||
                              Joined.Kind == AssemblyKind::DefinitelySafe;
            bool IncomingSafe = P.Kind == AssemblyKind::Closed ||
                                P.Kind == AssemblyKind::Ordinary ||
                                P.Kind == AssemblyKind::DefinitelySafe;
            if (JoinedSafe && IncomingSafe)
              Joined.Kind = AssemblyKind::DefinitelySafe;
            else
              Joined.Kind = AssemblyKind::Conflict;
            Joined.Epoch = nullptr;
          }
        }
        if (MBB.pred_empty())
          Joined.Kind = AssemblyKind::Absent;
        AssemblyState NewOut = Transfer(MBB, Joined, MBB.end());
        if (!SameAssemblyState(In.lookup(&MBB), Joined) ||
            !SameAssemblyState(Out.lookup(&MBB), NewOut)) {
          In[&MBB] = Joined;
          Out[&MBB] = NewOut;
          Progress = true;
        }
      }
    }
    AssemblyState AtUse = Transfer(TargetMBB, In.lookup(&TargetMBB), Before);
    return AtUse.Kind == AssemblyKind::Closed ||
           AtUse.Kind == AssemblyKind::DefinitelySafe;
  };

  enum class SourceStatus { Safe, Pending, Invalid };
  std::function<SourceStatus(
      MachineBasicBlock &, MachineBasicBlock::iterator, Register,
      MachineBasicBlock &, MachineBasicBlock::iterator,
      SmallVectorImpl<std::pair<MachineBasicBlock *, Register>> &)>
      ClassifySpillSource;
  ClassifySpillSource =
      [&](MachineBasicBlock &MBB, MachineBasicBlock::iterator Before,
          Register Reg, MachineBasicBlock &UseMBB,
          MachineBasicBlock::iterator UseBefore,
          SmallVectorImpl<std::pair<MachineBasicBlock *, Register>> &Active) {
        if (LinxV5::Shared_ABSRegClass.contains(Reg))
          return SourceStatus::Invalid;
        std::pair<MachineBasicBlock *, Register> Key{&MBB, Reg};
        if (is_contained(Active, Key))
          return SourceStatus::Safe;
        Active.push_back(Key);
        for (MachineInstr &Def : reverse(make_range(MBB.begin(), Before))) {
          if (!Def.definesRegister(Reg))
            continue;
          SourceStatus Status = SourceStatus::Safe;
          if (Def.getOpcode() == TargetOpcode::COPY)
            Status = ClassifySpillSource(MBB, Def.getIterator(),
                                         Def.getOperand(1).getReg(), MBB,
                                         Def.getIterator(), Active);
          else if (Def.getOpcode() == LinxV5::PseudoTReload)
            Status = SourceStatus::Pending;
          else if (Def.isInlineAsm()) {
            StringRef Text =
                Def.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
            if (Text.contains("B.SUBVIEW"))
              Status = SourceStatus::Invalid;
            else if (Text.contains("B.ASSEMBLE") &&
                     !IsAssemblyClosedAt(UseMBB, UseBefore, Reg))
              Status = SourceStatus::Invalid;
          }
          Active.pop_back();
          return Status;
        }
        bool SawPredecessor = false;
        SourceStatus Joined = SourceStatus::Safe;
        for (MachineBasicBlock *Pred : MBB.predecessors()) {
          SawPredecessor = true;
          SourceStatus Incoming =
              ClassifySpillSource(*Pred, Pred->end(), Reg, UseMBB, UseBefore,
                                  Active);
          if (Incoming == SourceStatus::Invalid) {
            Active.pop_back();
            return SourceStatus::Invalid;
          }
          if (Incoming == SourceStatus::Pending)
            Joined = SourceStatus::Pending;
        }
        Active.pop_back();
        return SawPredecessor ? Joined : SourceStatus::Invalid;
      };

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Opcode = MI.getOpcode();
      if (Opcode != LinxV5::PseudoTSpill && Opcode != LinxV5::PseudoTReload &&
          Opcode != LinxV5::PseudoTSpillDesc &&
          Opcode != LinxV5::PseudoTReloadDesc)
        continue;
      int FI = MI.getOperand(1).getIndex();
      bool IsStore =
          Opcode == LinxV5::PseudoTSpill || Opcode == LinxV5::PseudoTSpillDesc;
      ValidateInputMemRef(MI, FI, IsStore);
      SpillFIs.insert(FI);
      if (Opcode != LinxV5::PseudoTSpillDesc &&
          Opcode != LinxV5::PseudoTReloadDesc)
        continue;
      uint64_t Bytes = CapacityBytes(DescriptorFromPseudo(MI));
      MachineMemOperand *MMO = *MI.memoperands_begin();
      if (MMO->getSize() != Bytes)
        report_fatal_error(
            "raw Tile spill descriptor disagrees with its MMO extent");
    }
  }

  enum class StateKind { Uninitialized, Absent, Exact, Conflict };
  struct State {
    StateKind Kind = StateKind::Uninitialized;
    Optional<LinxV5::TileDescriptor> Descriptor;
  };
  auto Same = [](const State &A, const State &B) {
    return A.Kind == B.Kind &&
           ((!A.Descriptor && !B.Descriptor) ||
            (A.Descriptor && B.Descriptor && *A.Descriptor == *B.Descriptor));
  };
  auto SetAbsent = [](State &S) {
    S.Kind = StateKind::Absent;
    S.Descriptor = None;
  };
  auto SetExact = [&](State &S, const MachineInstr &MI) {
    S.Kind = StateKind::Exact;
    S.Descriptor = DescriptorFromPseudo(MI);
  };
  auto TransferFI = [&](MachineBasicBlock &MBB, int FI, State S) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == LinxV5::PseudoTSpillDesc &&
          MI.getOperand(1).getIndex() == FI) {
        SetExact(S, MI);
        continue;
      }
      if (MI.getOpcode() == LinxV5::PseudoTSpill &&
          MI.getOperand(1).getIndex() == FI) {
        SetAbsent(S);
        continue;
      }
      if ((MI.getOpcode() == LinxV5::PseudoTReload ||
           MI.getOpcode() == LinxV5::PseudoTReloadDesc) &&
          MI.getOperand(1).getIndex() == FI)
        continue;
      for (MachineMemOperand *MMO : MI.memoperands())
        if (MMO->isStore() && FixedFI(MMO) == Optional<int>(FI)) {
          SetAbsent(S);
          break;
        }
    }
    return S;
  };
  auto ComputeFIStates = [&](int FI, DenseMap<MachineBasicBlock *, State> &In,
                             DenseMap<MachineBasicBlock *, State> &Out) {
    In.clear();
    Out.clear();
    bool Progress = true;
    while (Progress) {
      Progress = false;
      for (MachineBasicBlock &MBB : MF) {
        State Joined;
        bool SawInitializedPredecessor = false;
        for (MachineBasicBlock *Pred : MBB.predecessors()) {
          State P = Out.lookup(Pred);
          if (P.Kind == StateKind::Uninitialized)
            continue;
          if (!SawInitializedPredecessor) {
            Joined = P;
            SawInitializedPredecessor = true;
          } else if (!Same(Joined, P)) {
            Joined.Kind = StateKind::Conflict;
            Joined.Descriptor = None;
          }
        }
        if (MBB.pred_empty())
          Joined.Kind = StateKind::Absent;
        State NewOut = TransferFI(MBB, FI, Joined);
        if (!Same(In.lookup(&MBB), Joined) || !Same(Out.lookup(&MBB), NewOut)) {
          In[&MBB] = Joined;
          Out[&MBB] = NewOut;
          Progress = true;
        }
      }
    }
  };
  DenseSet<Register> DescriptorRegs;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == LinxV5::PseudoTSpill ||
          MI.getOpcode() == LinxV5::PseudoTReload ||
          MI.getOpcode() == LinxV5::PseudoTSpillDesc ||
          MI.getOpcode() == LinxV5::PseudoTReloadDesc)
        DescriptorRegs.insert(MI.getOperand(0).getReg());
  bool AddedRegister = true;
  while (AddedRegister) {
    AddedRegister = false;
    for (MachineBasicBlock &MBB : MF)
      for (MachineInstr &MI : MBB)
        if (MI.getOpcode() == TargetOpcode::COPY &&
            DescriptorRegs.contains(MI.getOperand(0).getReg()))
          AddedRegister |=
              DescriptorRegs.insert(MI.getOperand(1).getReg()).second;
  }
  struct DescriptorState {
    DenseMap<Register, State> Registers;
    DenseMap<int, State> Slots;
  };
  auto Merge = [&](State &Into, const State &Incoming) {
    if (Incoming.Kind == StateKind::Uninitialized)
      return;
    if (Into.Kind == StateKind::Uninitialized) {
      Into = Incoming;
      return;
    }
    if (!Same(Into, Incoming)) {
      Into.Kind = StateKind::Conflict;
      Into.Descriptor = None;
    }
  };
  DenseMap<MachineInstr *, State> SpillDescriptors;
  auto TransferDescriptors = [&](MachineBasicBlock &MBB, DescriptorState S) {
    for (MachineInstr &MI : MBB) {
      State CopySource;
      if (MI.getOpcode() == TargetOpcode::COPY)
        CopySource = S.Registers.lookup(MI.getOperand(1).getReg());
      for (Register Reg : DescriptorRegs) {
        if (!MI.definesRegister(Reg))
          continue;
        State &Current = S.Registers[Reg];
        if (MI.getOpcode() == LinxV5::PseudoTReload) {
          Current = S.Slots.lookup(MI.getOperand(1).getIndex());
        } else if (MI.getOpcode() == TargetOpcode::COPY) {
          Current = CopySource;
        } else {
          auto Descriptor = LinxV5::tryGetTileRegDescriptor(
              MBB, std::next(MI.getIterator()), Reg);
          if (Descriptor) {
            Current.Kind = StateKind::Exact;
            Current.Descriptor = *Descriptor;
          } else {
            SetAbsent(Current);
          }
        }
      }
      if (MI.getOpcode() == LinxV5::PseudoTSpill) {
        State Source = S.Registers.lookup(MI.getOperand(0).getReg());
        SpillDescriptors[&MI] = Source;
        S.Slots[MI.getOperand(1).getIndex()] = Source;
        continue;
      }
      if (MI.getOpcode() == LinxV5::PseudoTSpillDesc) {
        SetExact(S.Slots[MI.getOperand(1).getIndex()], MI);
        continue;
      }
      for (MachineMemOperand *MMO : MI.memoperands())
        if (MMO->isStore())
          if (auto FI = FixedFI(MMO))
            if (SpillFIs.contains(*FI))
              SetAbsent(S.Slots[*FI]);
    }
    return S;
  };
  DenseMap<MachineBasicBlock *, DescriptorState> DescriptorOut;
  bool DescriptorProgress = true;
  while (DescriptorProgress) {
    DescriptorProgress = false;
    for (MachineBasicBlock &MBB : MF) {
      DescriptorState Joined;
      for (MachineBasicBlock *Pred : MBB.predecessors()) {
        const DescriptorState &Incoming = DescriptorOut[Pred];
        for (Register Reg : DescriptorRegs)
          Merge(Joined.Registers[Reg], Incoming.Registers.lookup(Reg));
        for (int FI : SpillFIs)
          Merge(Joined.Slots[FI], Incoming.Slots.lookup(FI));
      }
      if (MBB.pred_empty()) {
        for (Register Reg : DescriptorRegs)
          SetAbsent(Joined.Registers[Reg]);
        for (int FI : SpillFIs)
          SetAbsent(Joined.Slots[FI]);
      }
      DescriptorState NewOut = TransferDescriptors(MBB, std::move(Joined));
      DescriptorState &OldOut = DescriptorOut[&MBB];
      for (Register Reg : DescriptorRegs)
        DescriptorProgress |=
            !Same(OldOut.Registers.lookup(Reg), NewOut.Registers.lookup(Reg));
      for (int FI : SpillFIs)
        DescriptorProgress |=
            !Same(OldOut.Slots.lookup(FI), NewOut.Slots.lookup(FI));
      OldOut = std::move(NewOut);
    }
  }

  bool Progress = true;
  while (Progress) {
    Progress = false;

    for (MachineBasicBlock &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It++;
        if (MI.getOpcode() != LinxV5::PseudoTSpill)
          continue;
        SmallVector<std::pair<MachineBasicBlock *, Register>, 8> Active;
        SourceStatus Status = ClassifySpillSource(
            MBB, MI.getIterator(), MI.getOperand(0).getReg(), MBB,
            MI.getIterator(), Active);
        if (Status == SourceStatus::Invalid)
          report_fatal_error(
              "raw Tile spill source is OPEN, alias-bearing, or unresolved");
        State Proven = SpillDescriptors.lookup(&MI);
        Optional<LinxV5::TileDescriptor> MaybeD =
            Proven.Kind == StateKind::Exact ? Proven.Descriptor : None;
        if (!MaybeD)
          continue;
        const LinxV5::TileDescriptor &D = *MaybeD;
        int FI = MI.getOperand(1).getIndex();
        uint64_t Bytes = CapacityBytes(D);
        MachineFrameInfo &MFI = MF.getFrameInfo();
        MFI.setObjectSize(FI, std::max<uint64_t>(MFI.getObjectSize(FI), Bytes));
        MFI.setObjectAlignment(FI, Align(256));
        MachineInstrBuilder New = BuildMI(MBB, MI, MI.getDebugLoc(),
                                          TII->get(LinxV5::PseudoTSpillDesc));
        New.add(MI.getOperand(0))
            .addFrameIndex(FI)
            .addImm(D.ValidCol)
            .addImm(D.ValidRow)
            .addImm(D.PhysicalCol)
            .addImm(D.DataType)
            .addImm(D.Layout)
            .addImm(D.SizeCode);
        RewriteMemRef(*New, FI, Bytes, /*IsStore=*/true);
        MI.eraseFromParent();
        Progress = Changed = true;
      }
    }

    for (int FI : SpillFIs) {
      DenseMap<MachineBasicBlock *, State> In, Out;
      ComputeFIStates(FI, In, Out);
      for (MachineBasicBlock &MBB : MF) {
        State S = In.lookup(&MBB);
        for (auto It = MBB.begin(); It != MBB.end();) {
          MachineInstr &MI = *It++;
          if (MI.getOpcode() == LinxV5::PseudoTSpillDesc &&
              MI.getOperand(1).getIndex() == FI) {
            SetExact(S, MI);
            continue;
          }
          if (MI.getOpcode() == LinxV5::PseudoTSpill &&
              MI.getOperand(1).getIndex() == FI) {
            SetAbsent(S);
            continue;
          }
          if (MI.getOpcode() == LinxV5::PseudoTReload &&
              MI.getOperand(1).getIndex() == FI) {
            if (S.Kind != StateKind::Exact || !S.Descriptor)
              continue;
            const LinxV5::TileDescriptor &D = *S.Descriptor;
            uint64_t Bytes = CapacityBytes(D);
            MachineInstrBuilder New = BuildMI(
                MBB, MI, MI.getDebugLoc(), TII->get(LinxV5::PseudoTReloadDesc));
            New.add(MI.getOperand(0))
                .addFrameIndex(FI)
                .addImm(D.ValidCol)
                .addImm(D.ValidRow)
                .addImm(D.PhysicalCol)
                .addImm(D.DataType)
                .addImm(D.Layout)
                .addImm(D.SizeCode);
            RewriteMemRef(*New, FI, Bytes, /*IsStore=*/false);
            MI.eraseFromParent();
            Progress = Changed = true;
            continue;
          }
          for (MachineMemOperand *MMO : MI.memoperands())
            if (MMO->isStore() && FixedFI(MMO) == Optional<int>(FI)) {
              SetAbsent(S);
              break;
            }
        }
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == LinxV5::PseudoTSpill)
        report_fatal_error(
            "cannot prove a complete Local tile descriptor for raw spill");
      if (MI.getOpcode() == LinxV5::PseudoTReload)
        report_fatal_error(
            "raw Tile reload has no exact reaching spill descriptor");
    }
  }
  return Changed;
}

bool LinxV5TileFixup::Hoist(MachineBasicBlock &MBB) {
  bool Changed = false;
  auto Boundary = MBB.begin();
  for (auto MBBI = MBB.begin(), MBBE = MBB.end(); MBBI != MBBE;) {
    MachineInstr *MI = &*MBBI++;
    if ((isLocalTileCopy(*MI)) || MI->getOpcode() == LinxV5::PseudoTSpill ||
        MI->getOpcode() == LinxV5::PseudoTReload) {
      if (MI->getIterator() != Boundary) {
        MBB.splice(Boundary, &MBB, MI);
        Changed = true;
      }
      Boundary = std::next(MI->getIterator());
    } else if (DependTileRegs(*MI)) {
      Boundary = std::next(MI->getIterator());
    }
  }
  return Changed;
}

// Adjust the stack slot size of the Tile register
bool LinxV5TileFixup::AnnotateTileSizes(MachineFunction &MF) {
  bool Changed = false;
  MachineFrameInfo &MFI = MF.getFrameInfo();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == LinxV5::PseudoTSpill) {
        Register TileReg = MI.getOperand(0).getReg();

        unsigned RegSizeCode = LinxV5::getTileRegSize(MBB, MI, TileReg, true);
        if (RegSizeCode != -1u) {
          unsigned RegSize = 1 << (RegSizeCode + 6);
          int FI = MI.getOperand(1).getIndex();
          MFI.setObjectSize(FI,
                            std::max<uint64_t>(MFI.getObjectSize(FI), RegSize));
          MFI.setObjectAlignment(FI, Align(256));
          Changed = true;
        }
      }
    }
  }
  return Changed;
}

} // namespace

INITIALIZE_PASS(LinxV5TileFixup, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {

FunctionPass *createLinxV5TileFixupPass() { return new LinxV5TileFixup(); }

} // namespace llvm
