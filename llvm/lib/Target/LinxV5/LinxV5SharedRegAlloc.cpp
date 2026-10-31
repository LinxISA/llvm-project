//===-- LinxV5SharedRegAlloc.cpp - Allocate Shared registers -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-shared-regalloc"

namespace {

static bool isSharedRegister(const MachineRegisterInfo &MRI, Register Reg) {
  if (!Reg)
    return false;
  if (Reg.isPhysical())
    return LinxV5::Shared_ABSRegClass.contains(Reg);
  return MRI.getRegClass(Reg) == &LinxV5::Shared_ABSRegClass;
}

static bool isLocalTileRegister(const MachineRegisterInfo &MRI, Register Reg) {
  if (!Reg)
    return false;
  if (Reg.isPhysical())
    return LinxV5::Tile_ABSRegClass.contains(Reg);
  return LinxV5::Tile_ABSRegClass.hasSubClassEq(MRI.getRegClass(Reg));
}

static SmallVector<Register, 2>
getAssembleLocalSources(const MachineInstr &MI,
                        const MachineRegisterInfo &MRI) {
  SmallVector<Register, 2> Sources;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !isLocalTileRegister(MRI, MO.getReg()))
      continue;
    if (!llvm::is_contained(Sources, MO.getReg()))
      Sources.push_back(MO.getReg());
  }
  return Sources;
}

static StringRef getAssembleDescriptor(const MachineInstr &MI) {
  if (!MI.isInlineAsm() || MI.getNumOperands() <= InlineAsm::MIOp_AsmString)
    return StringRef();
  StringRef Asm = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  size_t Begin = Asm.find("B.DATR");
  if (Begin == StringRef::npos)
    return StringRef();
  size_t End = Asm.find('\n', Begin);
  return Asm.slice(Begin, End == StringRef::npos ? Asm.size() : End);
}

static bool isAssembleInit(const MachineInstr &MI) {
  return MI.isInlineAsm() &&
         StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
             .contains("B.ASSEMBLE") &&
         llvm::any_of(MI.operands(), [](const MachineOperand &MO) {
           return MO.isReg() && MO.isDef() && !MO.isImplicit();
         });
}

static bool isAssembleContinuation(const MachineInstr &MI) {
  return MI.isInlineAsm() &&
         StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
             .contains("B.ASSEMBLE") &&
         !llvm::any_of(MI.operands(), [](const MachineOperand &MO) {
           return MO.isReg() && MO.isDef() && !MO.isImplicit();
         });
}

static Register getSharedDef(const MachineInstr &MI,
                             const MachineRegisterInfo &MRI) {
  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isDef() && !MO.isImplicit() &&
        isSharedRegister(MRI, MO.getReg()))
      return MO.getReg();
  return LinxV5::NoRegister;
}

static Register getSharedUse(const MachineInstr &MI,
                             const MachineRegisterInfo &MRI) {
  Register Result = LinxV5::NoRegister;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || MO.isDef() || MO.isImplicit() ||
        !isSharedRegister(MRI, MO.getReg()))
      continue;
    if (Result != LinxV5::NoRegister)
      return LinxV5::NoRegister;
    Result = MO.getReg();
  }
  return Result;
}

static void collectAssembleSharedIdentity(
    MachineFunction &MF, DenseMap<Register, Register> &EmptyToInit) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  SmallVector<MachineInstr *, 8> Inits;
  SmallVector<MachineInstr *, 8> Continuations;
  for (MachineBasicBlock &MBB : MF)
    for (MachineInstr &MI : MBB) {
      if (isAssembleInit(MI) && getSharedDef(MI, MRI))
        Inits.push_back(&MI);
      else if (isAssembleContinuation(MI) && getSharedUse(MI, MRI))
        Continuations.push_back(&MI);
    }

  for (MachineInstr *Continuation : Continuations) {
    Register Use = getSharedUse(*Continuation, MRI);
    if (!Use)
      continue;
    StringRef Descriptor = getAssembleDescriptor(*Continuation);
    SmallVector<Register, 2> Sources =
        getAssembleLocalSources(*Continuation, MRI);
    SmallVector<Register, 2> Matches;
    for (MachineInstr *Init : Inits) {
      if (getAssembleDescriptor(*Init) != Descriptor ||
          getAssembleLocalSources(*Init, MRI) != Sources)
        continue;
      Register Def = getSharedDef(*Init, MRI);
      if (Def && !llvm::is_contained(Matches, Def))
        Matches.push_back(Def);
    }

    if (Matches.size() != 1)
      report_fatal_error(
          "B.ASSEMBLE Shared continuation has no unique cooperative INIT "
          "identity");
    EmptyToInit[Use] = Matches.front();
  }
}

class LinxV5SharedHandleLegalize : public MachineFunctionPass {
public:
  static char ID;

  LinxV5SharedHandleLegalize() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "LinxV5 Shared Handle Legalization";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
};

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
char LinxV5SharedHandleLegalize::ID = 0;

INITIALIZE_PASS(LinxV5SharedHandleLegalize, "linxv5-shared-handle-legalize",
                "LinxV5 Shared Handle Legalization", false, false)

INITIALIZE_PASS(LinxV5SharedRegAlloc, DEBUG_TYPE,
                "LinxV5 Shared Register Allocation", false, false)

bool LinxV5SharedHandleLegalize::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  DenseSet<Register> Handles;
  SmallVector<Register, 16> Worklist;
  for (unsigned Index = 0; Index < MRI.getNumVirtRegs(); ++Index) {
    Register Reg = Register::index2VirtReg(Index);
    if (!MRI.reg_nodbg_empty(Reg) &&
        MRI.getRegClass(Reg) == &LinxV5::Shared_ABSRegClass) {
      Handles.insert(Reg);
      Worklist.push_back(Reg);
    }
  }

  while (!Worklist.empty()) {
    Register Handle = Worklist.pop_back_val();
    for (MachineOperand &Operand : MRI.reg_nodbg_operands(Handle)) {
      MachineInstr &MI = *Operand.getParent();
      if (MI.isCopy() || MI.isPHI()) {
        for (MachineOperand &Connected : MI.operands()) {
          if (!Connected.isReg() || !Connected.getReg())
            continue;
          Register Reg = Connected.getReg();
          if (!Reg.isVirtual())
            report_fatal_error(
                "LinxV5 Shared handle cannot cross a physical register ABI");
          const TargetRegisterClass *RegClass = MRI.getRegClass(Reg);
          if (RegClass != &LinxV5::Shared_ABSRegClass &&
              RegClass != &LinxV5::MixedGPRRegClass &&
              RegClass != &LinxV5::MixedGPRNoRARegClass &&
              RegClass != &LinxV5::GRRegClass &&
              RegClass != &LinxV5::GRNoRARegClass &&
              RegClass != &LinxV5::GRNoR0RegClass)
            report_fatal_error(
                "LinxV5 Shared handle cannot alias a Local tile register");
          if (Handles.insert(Reg).second)
            Worklist.push_back(Reg);
        }
        continue;
      }
      if (MI.isImplicitDef())
        continue;
      unsigned OperandIndex = &Operand - &MI.getOperand(0);
      if (MI.getRegClassConstraint(OperandIndex, TII, TRI) !=
          &LinxV5::Shared_ABSRegClass)
        report_fatal_error(
            "LinxV5 Shared handle requires Shared-only COPY/PHI aliases and "
            "B.IOS operands; integer operations, memory bridges and calls "
            "cannot preserve Shared identity");
    }
  }

  bool Changed = false;
  for (Register Handle : Handles) {
    if (MRI.getRegClass(Handle) != &LinxV5::Shared_ABSRegClass) {
      MRI.setRegClass(Handle, &LinxV5::Shared_ABSRegClass);
      Changed = true;
    }
  }
  return Changed;
}

FunctionPass *llvm::createLinxV5SharedHandleLegalizePass() {
  return new LinxV5SharedHandleLegalize();
}

bool LinxV5SharedRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  LiveIntervals &LIS = getAnalysis<LiveIntervals>();
  VirtRegMap &VRM = getAnalysis<VirtRegMap>();

  bool Changed = false;
  SmallVector<LiveInterval *, 16> SharedIntervals;
  DenseMap<Register, Register> EmptyToInit;
  collectAssembleSharedIdentity(MF, EmptyToInit);
  DenseMap<Register, Register> EmptySharedRegs;
  for (unsigned Index = 0; Index < MRI.getNumVirtRegs(); ++Index) {
    Register Reg = Register::index2VirtReg(Index);
    if (MRI.reg_nodbg_empty(Reg) ||
        MRI.getRegClass(Reg) != &LinxV5::Shared_ABSRegClass)
      continue;
    if (!LIS.hasInterval(Reg))
      report_fatal_error("LinxV5 Shared register has no live interval");
    LiveInterval &Interval = LIS.getInterval(Reg);
    Changed = true;
    if (Interval.empty()) {
      EmptySharedRegs[Reg] = Reg;
      continue;
    }
    SharedIntervals.push_back(&Interval);
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

  for (const auto &Entry : EmptySharedRegs) {
    Register Empty = Entry.first;
    Register Init = EmptyToInit.lookup(Empty);
    if (!Init)
      VRM.assignVirt2Phys(Empty, LinxV5::Shared_ABSRegClass.getRegister(0));
    else if (VRM.getPhys(Init))
      VRM.assignVirt2Phys(Empty, VRM.getPhys(Init));
    else
      report_fatal_error(
          "B.ASSEMBLE Shared INIT has no allocated physical identity");
  }

  return Changed;
}

FunctionPass *llvm::createLinxV5SharedRegAllocPass() {
  return new LinxV5SharedRegAlloc();
}
