//===---------------------- LinxV5FrameLowering.cpp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of TargetFrameLowering class.
//
// LinxV5 Stack Layout:
// |                      | --> FP
// |----------------------|
// |      varargs         |
// |(fixed statck object) |
// |----------------------|
// |                      |
// |----------------------|
// |callee saved register |
// |----------------------|
// |                      |
// |----------------------|
// |  stack realignment   |
// |----------------------|
// |                      |
// |----------------------|
// |static local variables|
// |----------------------|
// |                      |
// |----------------------| --> sp'
// |   varsize object     |
// |  (dynamic statck)    |
// |----------------------| --> sp
// |                      |
// |                      |
//
//===----------------------------------------------------------------------===//

#include "LinxV5FrameLowering.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5Subtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "linx-frame-lowering"

static cl::opt<bool> DisableProEpiMemBlock(
    "linxv5-disable-proepi-memblock",
    cl::desc("Disable generate memory block for prologue/epilogue"),
    cl::init(false), cl::Hidden);

static constexpr uint64_t MAX_FENTRY_STACK_SIZE = 0xfff * 8;
static constexpr uint64_t MAX_FEXIT_STACK_SIZE = 0xfff * 8;

bool LinxV5FrameLowering::hasFP(const MachineFunction &MF) const {
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         RegInfo->hasStackRealignment(MF) || MFI.hasVarSizedObjects() ||
         MFI.isFrameAddressTaken();
}

bool LinxV5FrameLowering::hasBP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  return MFI.hasVarSizedObjects() && TRI->hasStackRealignment(MF);
}

// Determines the size of the frame and maximum call frame size.
void LinxV5FrameLowering::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t FrameSize = MFI.getStackSize();

  // Get the alignment.
  Align StackAlign = getStackAlign();

  // Set Max Call Frame Size
  uint64_t MaxCallSize = alignTo(MFI.getMaxCallFrameSize(), StackAlign);
  MFI.setMaxCallFrameSize(MaxCallSize);

  // Make sure the frame is aligned.
  FrameSize = alignTo(FrameSize, StackAlign);

  // Update frame info.
  MFI.setStackSize(FrameSize);
}

void LinxV5FrameLowering::adjustReg(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const DebugLoc &DL, Register DestReg,
                                    Register SrcReg, int64_t Val,
                                    MachineInstr::MIFlag Flag) const {
  if (DestReg == SrcReg && Val == 0)
    return;

  const LinxV5InstrInfo *TII = STI.getInstrInfo();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  if (isUInt<12>(Val) || isUInt<12>(-Val)) {
    if (Val >= 0)
      BuildMI(MBB, MBBI, DL, TII->get(LinxV5::ADDI), DestReg)
          .addReg(SrcReg)
          .addImm(Val)
          .setMIFlag(Flag);
    else
      BuildMI(MBB, MBBI, DL, TII->get(LinxV5::SUBI), DestReg)
          .addReg(SrcReg)
          .addImm(-Val)
          .setMIFlag(Flag);
  } else {
    Register Scratch = MRI.createVirtualRegister(&LinxV5::LTRRegClass);
    TII->movImm(MBB, MBBI, DL, Scratch, Val, Flag);
    BuildMI(MBB, MBBI, DL, TII->get(LinxV5::ADD), DestReg)
        .addReg(SrcReg)
        .addReg(Scratch, RegState::Kill)
        .addImm(0)
        .setMIFlag(Flag);
  }
}

static SmallVector<CalleeSavedInfo, 8>
getNonLibcallCSI(const std::vector<CalleeSavedInfo> &CSI) {
  SmallVector<CalleeSavedInfo, 8> NonLibcallCSI;

  for (auto &CS : CSI)
    if (CS.getFrameIdx() >= 0)
      NonLibcallCSI.push_back(CS);

  return NonLibcallCSI;
}

/// To avoid repeat-set-bgpr and get-after-set problem in a MBB, this will
/// create empty SaveBlock/RestoreBlock.
/// 1. if in the original SaveBlock/RestoreBlock there is def_instr of SP/T6,
///    create empty MBB to replace original SaveBlock/RestoreBlock;
/// 2. if in the original SaveBlock there is use_instr of SP, create empty MBB
///    to replace original SaveBlock; (This case may be impossible);
/// 3. if in the original SaveBlock there is use_instr of frame index which
///    will be eliminated by FP, create empty MBB to replace original SaveBlock
///    to avoid get-after-set problem of FP.
/// 4. if a stack realignment is demanded, replace every save-block or newly
/// created save-block with an empty block for stack realignment;
void LinxV5FrameLowering::calculateSaveRestoreBlocks(
    MachineFunction &MF, SmallVector<MachineBasicBlock *, 4> &SaveBlocks,
    SmallVector<MachineBasicBlock *, 4> &RestoreBlocks) const {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return;

  // if the restore block is also a save block, create a new save block.
  SmallVector<MachineBasicBlock *, 4> ToSplitBlocks;
  Register SPReg = LinxV5::getSPReg();
  for (MachineBasicBlock *MBB : SaveBlocks)
    if (llvm::find(RestoreBlocks, MBB) != RestoreBlocks.end())
      ToSplitBlocks.push_back(MBB);
  for (MachineBasicBlock *MBB : ToSplitBlocks) {
    auto I = llvm::find(SaveBlocks, MBB);
    if (I == SaveBlocks.end())
      continue;
    SaveBlocks.erase(I);
    MachineBasicBlock *NewSave =
        MF.CreateMachineBasicBlock(MBB->getBasicBlock());
    MF.insert(MBB->getIterator(), NewSave);
    NewSave->addSuccessor(MBB);
    SaveBlocks.push_back(NewSave);
  }

  // avoid write sp twice & avoid read sp after write
  // TODO: If use virtual usp can emiliate read sp after write
  for (auto I = SaveBlocks.begin(), E = SaveBlocks.end(); I != E; ++I) {
    auto *MBB = *I;
    bool NeedNewBlock = false;
    for (auto &MI : *MBB) {
      for (auto &MO : MI.operands()) {
        if (MO.isFI()) {
          NeedNewBlock = true;
          break;
        }
        if (!MO.isReg() || MO.isImplicit())
          continue;
        Register Reg = MO.getReg();
        if (Reg == SPReg)
          NeedNewBlock = true;
      }

      if (NeedNewBlock)
        break;
    }

    // introduce a new predecessor block exclusively for stack re-alignment
    if (hasFP(MF)) {
      const LinxV5RegisterInfo *RI = STI.getRegisterInfo();
      if (RI->hasStackRealignment(MF)) {
        auto *AlignBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
        MF.insert(MBB->getIterator(), AlignBB);
        AlignBB->addSuccessor(MBB);
        MBB = AlignBB;
        *I = AlignBB;
      }
    }

    if (NeedNewBlock) {
      auto *NewSave = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
      MF.insert(MBB->getIterator(), NewSave);
      NewSave->addSuccessor(MBB);
      *I = NewSave;
    }
  }

  // avoid write sp twice & avoid read sp after write
  // TODO: If use virtual usp can emiliate read sp after write
  for (auto I = RestoreBlocks.begin(), E = RestoreBlocks.end(); I != E; ++I) {
    auto *MBB = *I;
    bool NeedNewBlock = false;
    for (auto &MI : *MBB) {
      for (auto &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
          continue;
        Register Reg = MO.getReg();
        if (Reg == SPReg) {
          NeedNewBlock = true;
          break;
        }
      }

      if (NeedNewBlock)
        break;
    }

    if (NeedNewBlock) {
      assert(MBB->getFirstTerminator() != MBB->begin());
      auto *NewRestore =
          MBB->splitAt(*MBB->getFirstTerminator()->getPrevNode());
      *I = NewRestore;
    }
  }
}

void LinxV5FrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return;

  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *LXFI = MF.getInfo<LinxV5MachineFunctionInfo>();
  const LinxV5RegisterInfo *RI = STI.getRegisterInfo();
  const LinxV5InstrInfo *TII = STI.getInstrInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  bool NoProEpiMemBlock = !LXFI->hasTemplatePrologue();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  Register SPReg = LinxV5::getSPReg();
  Register USPReg = LinxV5::getUSPReg();
  Register FPReg = LinxV5::getFPReg();

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;

  // Emit prologue for shadow call stack.
  assert(!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack) &&
         "There is shadow call stack");

  // Since spillCalleeSavedRegisters may have inserted a libcall, skip past
  // any instructions marked as FrameSetup
  // FIXME: LinxV5 won't insert libcall, so this is useless.
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup))
    ++MBBI;

  // Determine the correct frame layout
  determineFrameLayout(MF);

  // FIXME (note copied from Lanai): This appears to be overallocating.  Needs
  // investigation. Get the number of bytes to allocate from the FrameInfo.
  uint64_t StackSize = MFI.getStackSize();
  uint64_t RealStackSize = StackSize;

  // Early exit if there is no need to allocate on the stack
  if (RealStackSize == 0 && !MFI.adjustsStack())
    return;

  const auto &CSI = MFI.getCalleeSavedInfo();

  uint64_t FirstStackAdj = 0;
  if (!NoProEpiMemBlock) {
    /**
     * Input:
     *   f.entry 0
     *   <user code>
     * Output:
     *   f.entry size
     *   <.cfi_def_cfa_offset size>
     *   <.cfi_offset <csr>, offset>
     *   <setup fp> fp = addi sp, imm
     *   <.cfi_def_cfa $fp, ...>
     *   <adjust stack over f.entry> sp = subi sp, imm
     *   <.cfi_def_cfa_offset, ...>
     *   <split block if stack-realignment> maybe not need this
     *   <stack-realignment> <TODO>
     *   <user code>
     */
    uint64_t Limited = std::min(StackSize, MAX_FENTRY_STACK_SIZE);
    StackSize = StackSize - Limited;

    // -- step-1: fillup template prologue instruction.
    MBBI->getOperand(2).setImm(Limited);
    ++MBBI;

    // -- step-2: Emit ".cfi_def_cfa_offset RealStackSize"
    unsigned CFIIndex =
        MF.addFrameInst(MCCFIInstruction::cfiDefCfaOffset(nullptr, Limited));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    // -- step-3: Emit .cfi_offset of CSRs
    // Iterate over list of callee-saved registers and emit .cfi_offset
    // directives.
    for (const auto &Entry : CSI) {
      int FrameIdx = Entry.getFrameIdx();
      int64_t Offset;
      // Offsets for objects with fixed locations (IE: those saved by libcall)
      // are simply calculated from the frame index.
      if (FrameIdx < 0)
        Offset = FrameIdx * (int64_t)STI.getXLen() / 8;
      else
        Offset = MFI.getObjectOffset(Entry.getFrameIdx());
      Register Reg = Entry.getReg();
      unsigned CFIIndex = MF.addFrameInst(MCCFIInstruction::createOffset(
          nullptr, RI->getDwarfRegNum(Reg, true), Offset));
      BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex);
    }

    // -- step-4: setup fp.(fp first setup)
    if (hasFP(MF)) {
      adjustReg(MBB, MBBI, DL, FPReg, SPReg,
                Limited - LXFI->getVarArgsSaveSize(), MachineInstr::FrameSetup);

      // Emit ".cfi_def_cfa $fp, LXFI->getVarArgsSaveSize()"
      unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::cfiDefCfa(nullptr, RI->getDwarfRegNum(FPReg, true),
                                      LXFI->getVarArgsSaveSize()));
      BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex)
          .setMIFlag(MachineInstr::FrameSetup);
    }

    // -- step-5: adjust remaining stack size.(sp first setup)
    if (StackSize) {
      adjustReg(MBB, MBBI, DL, SPReg, SPReg, -StackSize,
                MachineInstr::FrameSetup);

      // If we are using a frame-pointer, and thus emitted ".cfi_def_cfa fp, 0",
      // don't emit an sp-based .cfi_def_cfa_offset
      if (!hasFP(MF)) {
        // Emit ".cfi_def_cfa_offset StackSize"
        unsigned CFIIndex = MF.addFrameInst(
            MCCFIInstruction::cfiDefCfaOffset(nullptr, MFI.getStackSize()));
        BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
            .addCFIIndex(CFIIndex)
            .setMIFlag(MachineInstr::FrameSetup);
      }
    }
  } else {
    /**
     * Input:
     *   <save csr>
     *   <user code>
     * Output:
     *   <setup u-sp> [u-]sp = subi sp, imm(huge imm will use tr)
     *   <if use u-sp> sp = copy u-sp
     *   <.cfi_def_cfa_offset ...>
     *   <save csr>
     *   <.cfi_offset csr, ...>
     *   <setup fp> fp = addi sp, imm(huge imm will use tr)
     *   <.cfi_def_cfa $fp, ...>
     *   <split block if stack-realignment>
     *   <stack-realignment> <TODO>
     *   <user code>
     */

    // -- step-1: adjust stack size and setup sp/u-sp
    if (StackSize) {
      if (RI->hasStackRealignment(MF) || MFI.hasVarSizedObjects()) {
        adjustReg(MBB, MBBI, DL, USPReg, SPReg, -StackSize,
                  MachineInstr::FrameSetup);
        BuildMI(MBB, MBBI, DL, TII->get(LinxV5::COPY), SPReg)
            .addReg(USPReg)
            .setMIFlag(MachineInstr::FrameSetup);
      } else {
        adjustReg(MBB, MBBI, DL, SPReg, SPReg, -StackSize,
                  MachineInstr::FrameSetup);
      }
    }

    // -- step-2: Emit ".cfi_def_cfa_offset StackSize"
    unsigned CFIIndex =
        MF.addFrameInst(MCCFIInstruction::cfiDefCfaOffset(nullptr, StackSize));
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
        .addCFIIndex(CFIIndex);

    // -- step-3: skip save CSRs
    std::advance(MBBI, getNonLibcallCSI(CSI).size());

    // -- step-4: setup fp
    if (hasFP(MF)) {
      adjustReg(MBB, MBBI, DL, FPReg, SPReg,
                StackSize - LXFI->getVarArgsSaveSize(),
                MachineInstr::FrameSetup);

      // Emit ".cfi_def_cfa $fp, LXFI->getVarArgsSaveSize()"
      unsigned CFIIndex = MF.addFrameInst(
          MCCFIInstruction::cfiDefCfa(nullptr, RI->getDwarfRegNum(FPReg, true),
                                      LXFI->getVarArgsSaveSize()));
      BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::CFI_INSTRUCTION))
          .addCFIIndex(CFIIndex)
          .setMIFlag(MachineInstr::FrameSetup);
    }
  }

  // Stack re-alignment;
  if (hasFP(MF)) {
    const LinxV5RegisterInfo *RI = STI.getRegisterInfo();
    if (RI->hasStackRealignment(MF)) {
      assert(0 && "Do not support stack re-alignment!");
    }
  }

  // split inline asm instructions from prologue block
  if (MBB.size() > 1) {
    MachineBasicBlock::reverse_iterator LastMI = MBB.rbegin();
    if (LastMI->isInlineAsm()) {
      MBB.splitAt(*std::next(LastMI));
    }
  }
}

void LinxV5FrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &OldMBB) const {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return;
  const LinxV5RegisterInfo *RI = STI.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  auto *LXFI = MF.getInfo<LinxV5MachineFunctionInfo>();
  bool NoProEpiMemBlock = !LXFI->hasTemplateEpilogue();
  const LinxV5InstrInfo *TII = STI.getInstrInfo();

  Register SPReg = LinxV5::getSPReg();
  Register USPReg = LinxV5::getUSPReg();
  Register FPReg = LinxV5::getFPReg();

  // Get the insert location for the epilogue. If there were no terminators in
  // the block, get the last instruction.
  MachineBasicBlock &MBB =
      OldMBB.getSingleSuccessor() ? *OldMBB.getSingleSuccessor() : OldMBB;
  MachineBasicBlock::iterator MBBI = MBB.end();
  DebugLoc DL;
  if (!MBB.empty()) {
    MBBI = MBB.getFirstTerminator();
    if (MBBI == MBB.end())
      MBBI = MBB.getLastNonDebugInstr();
    DL = MBBI->getDebugLoc();

    // If this is not a terminator, the actual insert location should be after the
    // last instruction.
    if (!MBBI->isTerminator())
      MBBI = std::next(MBBI);

    // If callee-saved registers are saved via libcall or pop instruction,
    // place stack adjustment before this call.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;
  }

  const auto &CSI = getNonLibcallCSI(MFI.getCalleeSavedInfo());
  uint64_t StackSize = MFI.getStackSize();
  auto LastFrameDestroy = MBBI;
  uint64_t RealStackSize = StackSize;
  uint64_t FPOffset = RealStackSize - LXFI->getVarArgsSaveSize();

  if (!NoProEpiMemBlock) {
    /**
     * Input:
     *   <user code>
     *   f.exit/f.ret 0
     *   <tail call>
     * Output(stack-realignment or varargs)(f.exit/f.ret over size):
     *   <user code>
     *   <restore usp> u-sp = subi fp, imm
     *   <adj stack size over f.exit> sp = addi u-sp, imm
     *   f.exit/f.ret size
     *   <tail call>
     * Output(stack-realignment or varargs)(f.exit/f.ret size enough):
     *   <user code>
     *   sp = subi fp, imm
     *   f.exit/f.ret size
     *   <tail call>
     * Output(normal):
     *   <user code>
     *   <adj stack size over f.exit> sp = addi sp, imm
     *   f.exit/f.ret size
     *   <tail call>
     */
    // -- step-1: fillup template epilogue instruction
    uint64_t Limited = std::min(StackSize, MAX_FEXIT_STACK_SIZE);
    StackSize = StackSize - Limited;
    MachineInstr *MI = &*MBBI;
    assert((MI->getOpcode() == LinxV5::FEXIT ||
            MI->getOpcode() == LinxV5::FRET_RA ||
            MI->getOpcode() == LinxV5::FRET_STK) &&
           "Epilogue use non-pop instruction");
    MI->getOperand(2).setImm(Limited);

    // -- step-2: setup sp
    if (RI->hasStackRealignment(MF) || MFI.hasVarSizedObjects()) {
      assert(hasFP(MF) && "frame pointer should not have been eliminated");
      if (StackSize) {
        adjustReg(MBB, MBBI, DL, USPReg, FPReg, -FPOffset,
                  MachineInstr::FrameDestroy);
        adjustReg(MBB, MBBI, DL, SPReg, USPReg, StackSize,
                  MachineInstr::FrameDestroy);
      } else {
        adjustReg(MBB, MBBI, DL, SPReg, FPReg, -FPOffset,
                  MachineInstr::FrameDestroy);
      }
    } else {
      if (StackSize) {
        adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackSize,
                  MachineInstr::FrameDestroy);
      }
    }
  } else {
    /**
     * Input:
     *   <user code>
     *   <restore csr>
     *   ret/tail call
     * Output(stack-realignment or varargs)(with CSR):
     *   <user code>
     *   u-sp = subi fp, imm
     *   <restore csr>
     *   <adjust stack> sp = addi u-sp, imm
     *   ret/tail call
     * Output(normal):
     *   <user code>
     *   <restore csr>
     *   <adjust stack><if need> sp = addi sp, imm
     *   ret/tail call
     */
    // -- step-1: get the insert pos before <restore csr>
    LastFrameDestroy = std::prev(MBBI, CSI.size());

    // -- step-2: setup sp
    if (RI->hasStackRealignment(MF) || MFI.hasVarSizedObjects()) {
      assert(hasFP(MF) && "frame pointer should not have been eliminated");
      assert(StackSize && "use fp without save it?");
      if (StackSize) {
        adjustReg(MBB, LastFrameDestroy, DL, USPReg, FPReg, -FPOffset,
                  MachineInstr::FrameDestroy);
        adjustReg(MBB, MBBI, DL, SPReg, USPReg, StackSize,
                  MachineInstr::FrameDestroy);
      }
    } else {
      if (StackSize) {
        adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackSize,
                  MachineInstr::FrameDestroy);
      }
    }
  }

  // Emit epilogue for shadow call stack.
  assert(!MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack) &&
         "There is shadow call stack");

  // split inline asm instructions from epilogue block
  if (MBB.size() > 1) {
    MachineBasicBlock::iterator FirstMI = MBB.begin();
    if (FirstMI->isInlineAsm()) {
      MBB.splitAt(*FirstMI);
    }
  }
}

StackOffset
LinxV5FrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                            Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  const auto *LXFI = MF.getInfo<LinxV5MachineFunctionInfo>();

  // Callee-saved registers should be referenced relative to the stack
  // pointer (positive offset), otherwise use the frame pointer (negative
  // offset).
  const auto &CSI = getNonLibcallCSI(MFI.getCalleeSavedInfo());
  int MinCSFI = 0;
  int MaxCSFI = -1;

  int Offset = MFI.getObjectOffset(FI) - getOffsetOfLocalArea() +
               MFI.getOffsetAdjustment();

  if (CSI.size()) {
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
  }

  if (FI >= MinCSFI && FI <= MaxCSFI) {
    // lowering CSR spill and reload
    if (RI->hasStackRealignment(MF) || MFI.hasVarSizedObjects())
      FrameReg = LinxV5::getUSPReg();
    else
      FrameReg = LinxV5::getSPReg();
    Offset += static_cast<int>(MFI.getStackSize());
  } else if (RI->hasStackRealignment(MF) && !MFI.isFixedObjectIndex(FI)) {
    // If the stack was realigned, the frame pointer is set in order to allow
    // SP to be restored, so we need another base register to record the stack
    // after realignment.
    if (hasBP(MF))
      assert(0 && "TODO: Support stack realigned!");
    else
      FrameReg = RI->getFrameRegister(MF);
    Offset += static_cast<int>(MFI.getStackSize());
  } else {
    FrameReg = RI->getFrameRegister(MF);
    if (hasFP(MF)) {
      Offset += static_cast<int>(LXFI->getVarArgsSaveSize());
    } else {
      Offset += static_cast<int>(MFI.getStackSize());
    }
  }
  if (!isInt<32>(Offset))
    report_fatal_error("stack offset over int32!");
  return StackOffset::getFixed(Offset);
}

void LinxV5FrameLowering::determineCalleeSaves(MachineFunction &MF,
                                               BitVector &SavedRegs,
                                               RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);
  // Unconditionally spill RA and FP only if the function uses a frame
  // pointer.
  if (hasFP(MF)) {
    SavedRegs.set(LinxV5::R10);
    SavedRegs.set(LinxV5::R11);
  }
  // Mark BP as used if function has dedicated base pointer.
  if (hasBP(MF))
    SavedRegs.set(LinxV5::getBPReg());

  if (!DisableProEpiMemBlock) {
    /**
     * We might meet csr hole when we allocate for global gpr lives
     * in one block. Fill the hole now.
     * This global gpr is generated by spill.
     *
     * before spill:
     *   bb.1:
     *   op %1:ggpr
     * after spill:
     *   bb.1:
     *   %2:tr = ld %stack.1
     *   %3:ggpr = COPY %2:tr
     *   op %3:ggpr
     * after regalloc(generate $s0 uses):
     *   bb.1:
     *   %2:tr = ld %stack.1
     *   $s0 = COPY %2:tr
     *   op $s0
     * after fixup(clear redundant $s0 uses):
     *   bb.1:
     *   %2:tr = ld %stack.1
     *   op %2:tr
     *
     * The best method to avoid generate csr hole is to avoid alloc
     * for some regs that will be removed. Here is just a conservation
     * solution to avoid crash when we truly meet the csr hole.
     */
    const auto *CSRegs = MF.getRegInfo().getCalleeSavedRegs();
    MCRegister MinCSR;
    MCRegister MaxCSR;
    for (unsigned i = 0; CSRegs[i]; ++i) {
      if (SavedRegs.test(CSRegs[i])) {
        if (!MinCSR)
          MinCSR = CSRegs[i];
        MaxCSR = CSRegs[i];
      }
    }

    for (unsigned Reg = MinCSR; Reg < MaxCSR; ++Reg) {
      if (!SavedRegs.test(Reg)) {
        LLVM_DEBUG(dbgs() << "CSR hole " << printReg(Reg, STI.getRegisterInfo())
                          << " found\n");
        SavedRegs.set(Reg);
      }
    }
  }
}

void LinxV5FrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterClass *RC = &LinxV5::GRRegClass;
  // estimateStackSize has been observed to under-estimate the final stack
  // size, so give ourselves wiggle-room by checking for stack size
  // representable an 11-bit signed field rather than 12-bits.
  // FIXME: It may be possible to craft a function with a small stack that
  // still needs an emergency spill slot for branch relaxation. This case
  // would currently be missed.
  if (!isInt<11>(MFI.estimateStackSize(MF))) {
    int RegScavFI = MFI.CreateStackObject(RegInfo->getSpillSize(*RC),
                                          RegInfo->getSpillAlign(*RC), false);
    RS->addScavengingFrameIndex(RegScavFI);
  }
}

static MachineBasicBlock::iterator
findLastDef(MachineBasicBlock &MBB, MachineBasicBlock::iterator Before,
            Register Reg) {
  auto MBBI = Before, MBBB = MBB.begin();
  for (; MBBI != MBBB;) {
    MachineInstr &MI = *(--MBBI);

    for (auto &DefMO : MI.defs()) {
      if (!DefMO.isReg() || DefMO.isImplicit())
        continue;
      Register DefReg = DefMO.getReg();
      if (DefReg == Reg)
        return std::next(MI.getIterator());
    }
  }
  return MBBI;
}

/**
 * we meet
 *
 * bb.1:
 *   <Reg> = yyy
 *   ...
 *   f.exit ...
 *   tail-ind <Reg>
 *
 * split to:
 *
 * bb.1:
 *   <Reg> = yyy
 *   ...
 *   f.exit ...
 * bb.2:
 *   tail-ind <Reg>
 *
 */
void LinxV5FrameLowering::moveTAILIndCalleeWithFEXIT(
    MachineBasicBlock &MBB, MachineInstr *FEXIT) const {
  MachineInstr *TAILInd = &*MBB.getLastNonDebugInstr();
  const auto *TII =
      MBB.getParent()->getSubtarget<LinxV5Subtarget>().getInstrInfo();
  Register Callee = TAILInd->getOperand(0).getReg();
  if (LinxV5::GRRegClass.contains(Callee))
    return;
  // At this moment, we can use any of caller-save register.
  BuildMI(MBB, findLastDef(MBB, FEXIT, Callee), TAILInd->getDebugLoc(),
          TII->get(LinxV5::COPY), LinxV5::R20 /*x0*/)
      .addReg(Callee);
  TAILInd->getOperand(0).setReg(LinxV5::R20);
}

/// Split fentry/fexit/fret as single MBB because they are block instructions;
void LinxV5FrameLowering::processFunctionAfterFrameIndicesReplaced(
    MachineFunction &MF, RegScavenger *RS) const {
  // split blocks for fentry/fexit/fret
  for (MachineBasicBlock &MBB : MF) {
    if (MBB.empty())
      continue;
    auto MBBI = MBB.begin();
    MachineInstr *MI = &*MBBI++;
    // ignoring debug-insts before Fentry-inst
    while (MBB.getNumber() == MF.front().getNumber() && MI->isDebugInstr()) {
      if (MBBI == MBB.end()) {
        break;
      }
      MI = &*MBBI++;
    }

    if (MI->getOpcode() == LinxV5::FENTRY) {
      auto MBBE = MBB.end();
      for (; MBBI != MBBE; MBBI++) {
        MI = &*MBBI;
        if (!MI->isCFIInstruction())
          break;
      }
      MI = &*(--MBBI);
      MBB.splitAt(*MI);
      continue;
    }

    MBBI = MBB.getLastNonDebugInstr();
    if (MBBI == MBB.begin() || MBBI == MBB.end()) {
      continue;
    }
    MI = &*MBBI;

    // threre are three scenes need to split:
    // Case1: normal-insts + fret
    // Case2: normal-insts + fexit + tail-call
    // Case3: normal-insts + fexit + ind-branch
    // ...
    bool isEpilogueBlock = llvm::any_of(MBB.instrs(), [](MachineInstr &I) {
      return I.getOpcode() == LinxV5::FRET_STK ||
             I.getOpcode() == LinxV5::FRET_RA || I.getOpcode() == LinxV5::FEXIT;
    });
    if (isEpilogueBlock) {
      auto Range = instructionsWithoutDebug(MBB.begin(), MBB.end());
      unsigned Size = std::distance(Range.begin(), Range.end());
      if (Size >= 2) {
        auto Prev = prev_nodbg(MBBI, MBB.begin());
        if (MI->getOpcode() == LinxV5::FRET_STK ||
            MI->getOpcode() == LinxV5::FRET_RA) {
          MBB.splitAt(*Prev);
          continue;
        } else if (Prev->getOpcode() == LinxV5::FEXIT &&
                   MI->getOpcode() == LinxV5::PseudoTAIL) {
          if (Size == 2) {
            MBB.splitAt(*Prev);
          } else {
            MBB.splitAt(*Prev);
            Prev = prev_nodbg(Prev, MBB.begin());
            MBB.splitAt(*Prev);
          }
          continue;
        } else if (Prev->getOpcode() == LinxV5::FEXIT &&
                   MI->getOpcode() == LinxV5::PseudoTAILInd) {
          if (Size == 2) {
            MBB.splitAt(*Prev);
          } else {
            DenseSet<Register> CallerSaveTempRegs = {LinxV5::R20, LinxV5::R21,
                                                     LinxV5::R22, LinxV5::R23};
            Register Callee = MI->getOperand(0).getReg();
            if (!CallerSaveTempRegs.count(Callee)) {
              auto CalleeDefI = findLastDef(MBB, Prev, Callee);
              // At this moment, we can use any of caller-save register.
              // Build copy directly:
              // * If Def is BGPR, MCP will eliminate BGPR->BGPR copy
              // * If Def is TR, copy can not be eliminated.
              BuildMI(MBB, CalleeDefI, MI->getDebugLoc(),
                      MF.getSubtarget().getInstrInfo()->get(LinxV5::COPY),
                      LinxV5::R20)
                  .addReg(Callee);
              Callee = LinxV5::R20;
              MI->getOperand(0).setReg(Callee);
            }
            MBB.splitAt(*Prev);
            Prev = prev_nodbg(Prev, MBB.begin());
            MBB.splitAt(*Prev);
          }
          continue;
        }
        LLVM_DEBUG(dbgs() << MBB << "\n");
        assert(0 && "Exist epilogue not be split!");
      }
    }
  }

  // shift to2, to3 if no spills
  if (STI.isSIMT()) {
    auto &MRI = MF.getRegInfo();
    auto &MFI = MF.getFrameInfo();
    if (MF.getFunction().hasFnAttribute("blkv-no-spill")) {
      assert(MFI.getStackSize() == 0 &&
             "Exist spills with `blkv-no-spill' attr");
    } else {
      if (MFI.getStackSize() == 0) {
        MRI.replaceRegWith(LinxV5::SIMT_TO2, LinxV5::SIMT_TO1);
        MRI.replaceRegWith(LinxV5::SIMT_TO3, LinxV5::SIMT_TO2);
      }
    }
  }
}

// Not preserve stack space within prologue for outgoing variables when the
// function contains variable size objects and let eliminateCallFramePseudoInstr
// preserve stack space for it.
bool LinxV5FrameLowering::hasReservedCallFrame(
    const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator LinxV5FrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  Register SPReg = LinxV5::getSPReg();
  DebugLoc DL = MI->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = MI->getOperand(0).getImm();
    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);

      if (MI->getOpcode() == LinxV5::ADJCALLSTACKDOWN)
        Amount = -Amount;

      auto *TII = MF.getSubtarget().getInstrInfo();
      adjustReg(MBB, MI, DL, SPReg, SPReg, Amount, MachineInstr::NoFlags);
    }
  }

  return MBB.erase(MI);
}

bool LinxV5FrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();
  int BeginIdx = MFI.getObjectIndexBegin();
  int Offset = -1;
  if (!MFI.isDeadObjectIndex(BeginIdx))
    Offset = MFI.getObjectOffset(BeginIdx) - getOffsetOfLocalArea();
  bool NoProEpiMemBlock = DisableProEpiMemBlock || (BeginIdx < 0 && Offset < 0);
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  // Manually spill values not spilled by libcall.
  const auto &NonLibcallCSI = getNonLibcallCSI(CSI);
  unsigned  CSMask = 0;
  Register CS_RegB = LinxV5::NoRegister;
  Register CS_RegE = LinxV5::NoRegister;
  for (auto &CS : NonLibcallCSI) {
    // Insert the spill to the stack frame.
    Register Reg = CS.getReg();
    if (NoProEpiMemBlock) {
      const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
      assert(LinxV5::GRRegClass.contains(Reg));
      TII.storeRegToStackSlot(MBB, MI, Reg, true, CS.getFrameIdx(), RC, TRI);
    } else{
      CSMask |= 1 << (Reg - LinxV5::R0);
      if (CS_RegB == LinxV5::NoRegister || Reg < CS_RegB)
        CS_RegB = Reg;
      if (CS_RegE == LinxV5::NoRegister || Reg > CS_RegE)
        CS_RegE = Reg;
    }
  }

  if (!NoProEpiMemBlock) {
    // The following is used to track whether there is a non-contiguous scene
    for (unsigned RegID = CS_RegB.id(); RegID <= CS_RegE.id(); RegID++) {
      unsigned Temp = 1 << (RegID - LinxV5::R0);
      if (!(CSMask & Temp)) {
        // TODO:For convenience, actually should enabled only in debug mode.
        errs() << "CSMask:0x" << utohexstr(CSMask) << "\n";
        assert(0 && "CS Regs are not consecutive");
      }
    }

    BuildMI(MBB, MI, DL, TII.get(LinxV5::FENTRY))
        .addReg(CS_RegB)
        .addReg(CS_RegE)
        .addImm(0);
    MF->getInfo<LinxV5MachineFunctionInfo>()->setTemplatePrologue();
  }

  return true;
}

bool LinxV5FrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;
  SmallVector<Register> GPRS;
  MachineInstr *TailInd = nullptr;
  for (auto &MI : MBB) {
    for (auto &MO : MI.operands()) {
      if (!MO.isReg() || MO.isImplicit())
        continue;
      if (MO.isDef())
        GPRS.push_back(MO.getReg());
    }
    if (MI.getOpcode() == LinxV5::PseudoTAILInd)
      TailInd = &MI;
  }
  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  // Manually restore values not restored by libcall. Insert in reverse order.
  // loadRegFromStackSlot can insert multiple instructions.
  const auto &NonLibcallCSI = getNonLibcallCSI(CSI);
  Register CS_RegB = LinxV5::NoRegister;
  Register CS_RegE = LinxV5::NoRegister;
  MachineFrameInfo &MFI = MF->getFrameInfo();
  int BeginIdx = MFI.getObjectIndexBegin();
  int Offset = -1;
  if (!MFI.isDeadObjectIndex(BeginIdx))
    Offset = MFI.getObjectOffset(BeginIdx) - getOffsetOfLocalArea();
  bool NoProEpiMemBlock = DisableProEpiMemBlock || (BeginIdx < 0 && Offset < 0);
  MachineBasicBlock::iterator SplitIt = std::prev(MI);
  bool IsSplit = false;
  if (TailInd) {
    auto UseReg = TailInd->getOperand(0).getReg();
    for (auto &CS : reverse(NonLibcallCSI)) {
      Register Reg = CS.getReg();
      if (Reg != UseReg)
        continue;
      BuildMI(MBB, MI, MBB.findDebugLoc(MBB.begin()), TII.get(LinxV5::COPY),
              LinxV5::R20)
          .addReg(UseReg);
      UseReg = LinxV5::R20;
      TailInd->getOperand(0).setReg(UseReg);
      break;
    }
  }

  for (auto &CS : reverse(NonLibcallCSI)) {
    Register Reg = CS.getReg();
    if (NoProEpiMemBlock) {
      int FI = CS.getFrameIdx();
      const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
      MachineMemOperand *MMO = MF->getMachineMemOperand(
          MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
          MFI.getObjectSize(FI), MFI.getObjectAlign(FI));
      if (std::find(GPRS.begin(), GPRS.end(), Reg) != GPRS.end())
        IsSplit = true;
      BuildMI(MBB, MI, DL, TII.get(LinxV5::LDI), Reg)
          .addFrameIndex(FI)
          .addImm(0)
          .addMemOperand(MMO);
      assert(MI != MBB.begin() &&
             "loadRegFromStackSlot didn't insert any code!");
    } else{
      if (CS_RegB == LinxV5::NoRegister || Reg < CS_RegB)
        CS_RegB = Reg;
      if (CS_RegE == LinxV5::NoRegister || Reg > CS_RegE)
        CS_RegE = Reg;
    }
  }

  if (IsSplit) {
    MBB.splitAt(*SplitIt);
  }

  if (NoProEpiMemBlock) {
    return true;
  }

  MF->getInfo<LinxV5MachineFunctionInfo>()->setTemplateEpilogue();

  if (MI != MBB.end() && MI->getOpcode() == LinxV5::PseudoRET) {
    if (CS_RegB == LinxV5::R10) {
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::IMPLICIT_DEF))
          .addReg(CS_RegB, RegState::Define);
      BuildMI(MBB, MI, DL, TII.get(TargetOpcode::IMPLICIT_DEF))
          .addReg(CS_RegE, RegState::Define);
      BuildMI(MBB, MI, DL, TII.get(LinxV5::FRET_STK))
          .addReg(CS_RegB, RegState::Kill)
          .addReg(CS_RegE, RegState::Kill)
          .addImm(0)
          .setMIFlag(MachineInstr::FrameDestroy);
    } else
      BuildMI(MBB, MI, DL, TII.get(LinxV5::FRET_RA))
          .addReg(CS_RegB)
          .addReg(CS_RegE)
          .addImm(0)
          .setMIFlag(MachineInstr::FrameDestroy);
    MI->eraseFromParent();
  } else if (MI != MBB.end() && MI->getOpcode() == LinxV5::PseudoTAILInd) {
    BuildMI(MBB, MI, DL, TII.get(LinxV5::FEXIT))
        .addReg(CS_RegB)
        .addReg(CS_RegE)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameDestroy);
  } else if (MI != MBB.end() && MI->getOpcode() == LinxV5::PseudoTAIL) {
    BuildMI(MBB, MI, DL, TII.get(LinxV5::FEXIT))
        .addReg(CS_RegB)
        .addReg(CS_RegE)
        .addImm(0)
        .setMIFlag(MachineInstr::FrameDestroy);
  }

  return true;
}
