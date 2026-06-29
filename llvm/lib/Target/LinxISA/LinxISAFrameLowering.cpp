//===-- LinxISAFrameLowering.cpp - Frame lowering for LinxISA -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxISA implementation of TargetFrameLowering.
// 
// LinxISA uses FENTRY/FEXIT/FRET.STK frame-template instructions for
// function prologue/epilogue and tail-transfer exits. These are hardware macro
// instructions that expand to save/restore register sequences.
//
//===----------------------------------------------------------------------===//

#include "LinxISAFrameLowering.h"
#include "LinxISAInstrInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include <algorithm>

using namespace llvm;

static bool shouldEmitFrameMacros(const MachineFunction &MF) {
  if (MF.getFunction().hasFnAttribute(Attribute::Naked))
    return false;

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.getStackSize() != 0 || !MFI.getCalleeSavedInfo().empty();
}

bool LinxISAFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}

static std::pair<unsigned, unsigned>
getFentryRangeEnc(const MachineFunction &MF) {
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // FENTRY/FRET save/restore a contiguous range starting at `ra`.
  const unsigned RegBeginEnc = TRI.getEncodingValue(LinxISA::R10);
  unsigned RegEndEnc = RegBeginEnc;

  for (const CalleeSavedInfo &CS : MFI.getCalleeSavedInfo())
    RegEndEnc = std::max<unsigned>(RegEndEnc, TRI.getEncodingValue(CS.getReg()));

  return {RegBeginEnc, RegEndEnc};
}

static constexpr uint64_t kFentryStackAlign = 8;
static constexpr uint64_t kFentryStackImmBits = 15;
static constexpr uint64_t kFentryStackMax =
    ((1ULL << kFentryStackImmBits) - 1) & ~(kFentryStackAlign - 1);
static constexpr uint64_t kStackAdjustImmMax = 4095;
static constexpr uint64_t kStackAdjustChunk =
    kStackAdjustImmMax & ~(kFentryStackAlign - 1);

static std::pair<uint64_t, uint64_t> splitFentryStack(uint64_t StackSize) {
  const uint64_t MacroStack = std::min<uint64_t>(StackSize, kFentryStackMax);
  return {MacroStack, StackSize - MacroStack};
}

static void emitStackAdjustChunks(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator InsertPt,
                                  const LinxISAInstrInfo &TII,
                                  uint64_t StackBytes, bool IsAllocate) {
  if (StackBytes == 0)
    return;

  const unsigned Opc = IsAllocate ? LinxISA::SUBIri : LinxISA::ADDIri;
  while (StackBytes > 0) {
    uint64_t Chunk = std::min<uint64_t>(StackBytes, kStackAdjustChunk);
    if (Chunk == 0 || Chunk > kStackAdjustImmMax)
      report_fatal_error("Linx: invalid stack adjustment chunk");

    BuildMI(MBB, InsertPt, DebugLoc(), TII.get(Opc), LinxISA::R1)
        .addReg(LinxISA::R1)
        .addImm(Chunk);
    StackBytes -= Chunk;
  }
}

void LinxISAFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                               BitVector &SavedRegs,
                                               RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  // If we're using the FENTRY/FRET macros, we must reserve stack space for the
  // full contiguous register range they will save/restore. Expand the
  // SavedRegs set so that if any callee-saved register is saved, then all
  // callee-saved registers from `ra` through the highest saved one are saved.
  //
  // This guarantees PEI allocates spill slots for the whole range, preventing
  // the macro save/restore microcode from clobbering local stack objects.
  if (MF.getFunction().hasFnAttribute(Attribute::Naked))
    return;

  // If the function has a stack frame but PEI didn't decide to save anything,
  // force saving `ra` so that FRET.STK has a valid restore slot.
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const bool HasFrame = MFI.hasStackObjects();
  const bool HasFP = hasFPImpl(MF);

  if (HasFP)
    SavedRegs.set(LinxISA::R18);

  const MCPhysReg *CSRs =
      MF.getSubtarget().getRegisterInfo()->getCalleeSavedRegs(&MF);
  unsigned LastIdx = 0;
  bool AnySaved = false;
  for (unsigned I = 0; CSRs[I] != 0; ++I) {
    if (!SavedRegs.test(CSRs[I]))
      continue;
    AnySaved = true;
    LastIdx = I;
  }

  if (!AnySaved) {
    if (!HasFrame)
      return;
    // Ensure `ra` is saved at least.
    SavedRegs.set(LinxISA::R10);
    AnySaved = true;
    LastIdx = 0;
  }

  for (unsigned I = 0; I <= LastIdx; ++I)
    SavedRegs.set(CSRs[I]);
}

void LinxISAFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  if (!RS)
    return;

  // LinxISA frequently needs a post-RA scratch register when eliminating frame
  // indices for reg-offset loads/stores (e.g. stack arrays indexed by a
  // runtime value). Provide an emergency spill slot for the register scavenger.
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  const TargetRegisterClass *RC = &LinxISA::GPRRegClass;

  int FI = MFI.CreateSpillStackObject(TRI.getSpillSize(*RC),
                                      TRI.getSpillAlign(*RC));
  RS->addScavengingFrameIndex(FI);
}

void LinxISAFrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  if (!shouldEmitFrameMacros(MF))
    return;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const uint64_t StackSize = MFI.getStackSize();
  if ((StackSize & (kFentryStackAlign - 1)) != 0)
    report_fatal_error(
        "Linx: invalid stack size for FENTRY/FRET (must be 8-byte aligned)");
  auto [MacroStack, ExtraStack] = splitFentryStack(StackSize);

  const LinxISAInstrInfo &TII =
      *static_cast<const LinxISAInstrInfo *>(MF.getSubtarget().getInstrInfo());

  auto [RegBeginEnc, RegEndEnc] = getFentryRangeEnc(MF);

  // The FENTRY macro is a standalone block instruction. Emit it in a dedicated
  // prologue block that falls through to the original entry block.
  MachineBasicBlock *PrologueBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(MBB.getIterator(), PrologueBB);
  PrologueBB->addSuccessor(&MBB);

  for (const MachineBasicBlock::RegisterMaskPair &LI : MBB.liveins())
    PrologueBB->addLiveIn(LI);
  PrologueBB->sortUniqueLiveIns();

  BuildMI(*PrologueBB, PrologueBB->end(), DebugLoc(), TII.get(LinxISA::FENTRY))
      .addImm(RegBeginEnc)
      .addImm(RegEndEnc)
      .addImm(MacroStack);
  emitStackAdjustChunks(*PrologueBB, PrologueBB->end(), TII, ExtraStack, true);
  if (hasFPImpl(MF)) {
    BuildMI(*PrologueBB, PrologueBB->end(), DebugLoc(), TII.get(LinxISA::ADDIri),
            LinxISA::R18)
        .addReg(LinxISA::R1)
        .addImm(0);
  }
}

void LinxISAFrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  if (!shouldEmitFrameMacros(MF))
    return;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  const uint64_t StackSize = MFI.getStackSize();
  if ((StackSize & (kFentryStackAlign - 1)) != 0)
    report_fatal_error(
        "Linx: invalid stack size for FENTRY/FRET (must be 8-byte aligned)");
  auto [MacroStack, ExtraStack] = splitFentryStack(StackSize);

  const LinxISAInstrInfo &TII =
      *static_cast<const LinxISAInstrInfo *>(MF.getSubtarget().getInstrInfo());

  auto [RegBeginEnc, RegEndEnc] = getFentryRangeEnc(MF);

  MachineInstr *TailCallMI = nullptr;
  for (MachineInstr &MI : llvm::reverse(MBB)) {
    if (MI.isDebugInstr() || MI.isCFIInstruction())
      continue;
    if (MI.getOpcode() == LinxISA::PSEUDO_TAILCALL ||
        MI.getOpcode() == LinxISA::PSEUDO_TAILICALL) {
      TailCallMI = &MI;
      break;
    }
    break;
  }

  if (TailCallMI) {
    auto It = TailCallMI->getIterator();
    ++It;
    auto E = MBB.end();
    for (; It != E; ++It) {
      if (It->isDebugInstr() || It->isCFIInstruction())
        continue;
      if (It->getOpcode() == LinxISA::PSEUDO_RET)
        continue;
      report_fatal_error(
          "Linx: musttail block contains non-return instructions after tail call");
    }

    MachineBasicBlock *FExitBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
    MachineBasicBlock *TailBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
    MF.insert(std::next(MBB.getIterator()), FExitBB);
    MF.insert(std::next(FExitBB->getIterator()), TailBB);

    TailBB->splice(TailBB->end(), &MBB, TailCallMI->getIterator(), MBB.end());
    for (auto It = TailBB->begin(); It != TailBB->end();) {
      if (It->getOpcode() == LinxISA::PSEUDO_RET) {
        It = TailBB->erase(It);
      } else {
        ++It;
      }
    }

    TailBB->transferSuccessorsAndUpdatePHIs(&MBB);
    MBB.addSuccessor(FExitBB);
    FExitBB->addSuccessor(TailBB);

    if (hasFPImpl(MF)) {
      BuildMI(*FExitBB, FExitBB->end(), DebugLoc(), TII.get(LinxISA::ADDIri),
              LinxISA::R1)
          .addReg(LinxISA::R18)
          .addImm(0);
    }
    emitStackAdjustChunks(*FExitBB, FExitBB->end(), TII, ExtraStack, false);
    BuildMI(*FExitBB, FExitBB->end(), DebugLoc(), TII.get(LinxISA::FEXIT))
        .addImm(RegBeginEnc)
        .addImm(RegEndEnc)
        .addImm(MacroStack);
    FExitBB->addLiveIn(LinxISA::R1);
    FExitBB->addLiveIn(LinxISA::R10);
    return;
  }

  MachineInstr *RetMI = nullptr;
  for (MachineInstr &MI : llvm::reverse(MBB)) {
    if (MI.isDebugInstr())
      continue;
    if (MI.isReturn()) {
      RetMI = &MI;
      break;
    }
    // No return terminator found.
    break;
  }

  if (!RetMI)
    return;

  SmallVector<Register, 8> RetValRegs;
  auto AddRetValReg = [&](Register Reg) {
    if (!Reg)
      return;
    // Skip special registers that are unrelated to the C ABI return value.
    if (Reg == LinxISA::R1 /*sp*/ || Reg == LinxISA::R10 /*ra*/)
      return;
    for (Register Existing : RetValRegs) {
      if (Existing == Reg)
        return;
    }
    RetValRegs.push_back(Reg);
  };
  for (const MachineOperand &MO : RetMI->operands()) {
    if (!MO.isReg() || MO.isDef())
      continue;
    AddRetValReg(MO.getReg());
  }
  RetMI->eraseFromParent();

  // The FRET macro is a standalone block instruction. Emit it in a dedicated
  // epilogue block and fall through to it from the original return block.
  MachineBasicBlock *EpilogueBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(std::next(MBB.getIterator()), EpilogueBB);
  MBB.addSuccessor(EpilogueBB);

  if (hasFPImpl(MF)) {
    BuildMI(*EpilogueBB, EpilogueBB->end(), DebugLoc(), TII.get(LinxISA::ADDIri),
            LinxISA::R1)
        .addReg(LinxISA::R18)
        .addImm(0);
  }
  emitStackAdjustChunks(*EpilogueBB, EpilogueBB->end(), TII, ExtraStack, false);
  MachineInstrBuilder MIB =
      BuildMI(*EpilogueBB, EpilogueBB->end(), DebugLoc(),
              TII.get(LinxISA::FRET_STK))
          .addImm(RegBeginEnc)
          .addImm(RegEndEnc)
          .addImm(MacroStack);
  for (Register Reg : RetValRegs)
    MIB.addReg(Reg, RegState::Implicit);

  // Post-RA MachineCopyPropagation deletes copies at the end of a block unless
  // the copy destination is listed as live-in to a successor block. Our return
  // value registers are modeled as implicit uses on FRET_STK (so they are not
  // necessarily present in live-in lists). Add them explicitly to keep return
  // value materialization in predecessor blocks intact.
  EpilogueBB->addLiveIn(LinxISA::R1);
  for (Register Reg : RetValRegs)
    EpilogueBB->addLiveIn(Reg);
}

bool LinxISAFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  // Callee-saved register saves are performed by the FENTRY macro.
  (void)MBB;
  (void)MI;
  (void)TRI;
  return !CSI.empty();
}

bool LinxISAFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  // Callee-saved register restores are performed by the FRET macro.
  (void)MBB;
  (void)MI;
  (void)TRI;
  if (CSI.empty())
    return false;
  for (CalleeSavedInfo &CS : CSI)
    CS.setRestored(true);
  return true;
}

MachineBasicBlock::iterator LinxISAFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  // LinxISA uses explicit call-sequence stack adjustments for outgoing stack
  // arguments. This keeps the fixed FENTRY/FRET "home" area (modeled in QEMU as
  // LINX_CALLFRAME_SIZE) independent from the variable per-call stack argument
  // area.
  if (hasReservedCallFrame(MF))
    return MBB.erase(MI);

  int64_t Amount = MI->getOperand(0).getImm();
  if (Amount == 0)
    return MBB.erase(MI);

  const LinxISAInstrInfo &TII =
      *static_cast<const LinxISAInstrInfo *>(MF.getSubtarget().getInstrInfo());
  DebugLoc DL = MI->getDebugLoc();

  const bool IsDown = MI->getOpcode() == LinxISA::ADJCALLSTACKDOWN;
  unsigned Op = IsDown ? LinxISA::SUBIri : LinxISA::ADDIri;

  // The immediate range for ADDI/SUBI is 12-bit unsigned; chunk large call
  // frames to keep bring-up code simple.
  while (Amount > 0) {
    int64_t Chunk = std::min<int64_t>(Amount, 4095);
    BuildMI(MBB, MI, DL, TII.get(Op), LinxISA::R1)
        .addReg(LinxISA::R1)
        .addImm(Chunk);
    Amount -= Chunk;
  }

  return MBB.erase(MI);
}
