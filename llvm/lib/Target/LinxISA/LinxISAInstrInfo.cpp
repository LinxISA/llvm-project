//===-- LinxISAInstrInfo.cpp - LinxISA Instruction Information ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISAInstrInfo.h"
#include "LinxISASubtarget.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "LinxISAGenInstrInfo.inc"

LinxISAInstrInfo::LinxISAInstrInfo(const LinxISASubtarget &STI)
    : LinxISAGenInstrInfo(STI, RI, LinxISA::ADJCALLSTACKDOWN,
                          LinxISA::ADJCALLSTACKUP),
      RI() {}

void LinxISAInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator I,
                                   const DebugLoc &DL, Register DestReg,
                                   Register SrcReg, bool KillSrc,
                                   bool RenamableDest,
                                   bool RenamableSrc) const {
  if (!LinxISA::GPRRegClass.contains(DestReg, SrcReg))
    report_fatal_error("Linx: unsupported reg-to-reg copy");

  BuildMI(MBB, I, DL, get(LinxISA::ADDIri), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addImm(0);
}

void LinxISAInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
    bool IsKill, int FrameIndex, const TargetRegisterClass *RC,
    Register /*VReg*/, MachineInstr::MIFlag /*Flags*/) const {
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  if (LinxISA::GPRRegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MBBI, DL, get(LinxISA::SDI))
        .addReg(SrcReg, getKillRegState(IsKill))
        .addFrameIndex(FrameIndex)
        .addImm(0);
    return;
  }

  if (LinxISA::TILERegClass.hasSubClassEq(RC)) {
    // Tile spills use TSTORE with a fixed 4KiB payload (TSize=6).
    BuildMI(MBB, MBBI, DL, get(LinxISA::PSEUDO_TLSU_TSTORE))
        .addFrameIndex(FrameIndex)
        .addReg(SrcReg, getKillRegState(IsKill))
        .addImm(6);
    return;
  }

  report_fatal_error("Linx: cannot store this register class");
}

void LinxISAInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC, Register /*VReg*/,
    unsigned /*SubIdx*/, MachineInstr::MIFlag /*Flags*/) const {
  DebugLoc DL;
  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  if (LinxISA::GPRRegClass.hasSubClassEq(RC)) {
    BuildMI(MBB, MBBI, DL, get(LinxISA::LDI), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(0);
    return;
  }

  if (LinxISA::TILERegClass.hasSubClassEq(RC)) {
    // Tile reloads use TLOAD with a fixed 4KiB payload (TSize=6).
    BuildMI(MBB, MBBI, DL, get(LinxISA::PSEUDO_TLSU_TLOAD_ANY), DestReg)
        .addFrameIndex(FrameIndex)
        .addImm(6);
    return;
  }

  report_fatal_error("Linx: cannot load this register class");
}

unsigned LinxISAInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  if (!FBB) {
    if (Cond.empty()) {
      BuildMI(&MBB, DL, get(LinxISA::JUMP)).addMBB(TBB);
      if (BytesAdded)
        *BytesAdded = 4;
      return 1;
    }

    assert(Cond.size() == 3 && Cond[0].isImm() && Cond[1].isReg() &&
           Cond[2].isReg() && "Invalid branch condition");
    unsigned Opc = static_cast<unsigned>(Cond[0].getImm());
    BuildMI(&MBB, DL, get(Opc))
        .addReg(Cond[1].getReg())
        .addReg(Cond[2].getReg())
        .addMBB(TBB);
    if (BytesAdded)
      *BytesAdded = 4;
    return 1;
  }

  // Two-way branch: emit conditional to TBB, then unconditional to FBB.
  assert(Cond.size() == 3 && Cond[0].isImm() && Cond[1].isReg() &&
         Cond[2].isReg() && "Invalid branch condition");
  unsigned Opc = static_cast<unsigned>(Cond[0].getImm());
  BuildMI(&MBB, DL, get(Opc))
      .addReg(Cond[1].getReg())
      .addReg(Cond[2].getReg())
      .addMBB(TBB);
  BuildMI(&MBB, DL, get(LinxISA::JUMP)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded = 8;
  return 2;
}

unsigned LinxISAInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                        int *BytesRemoved) const {
  unsigned Count = 0;
  if (BytesRemoved)
    *BytesRemoved = 0;

  for (auto I = MBB.end(); I != MBB.begin();) {
    --I;
    if (I->isDebugInstr())
      continue;
    if (I->getOpcode() != LinxISA::JUMP && !I->isConditionalBranch())
      break;
    I->eraseFromParent();
    ++Count;
    if (BytesRemoved)
      *BytesRemoved += 4;
    if (Count == 2)
      break;
    I = MBB.end();
  }
  return Count;
}

bool LinxISAInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *&TBB,
                                     MachineBasicBlock *&FBB,
                                     SmallVectorImpl<MachineOperand> &Cond,
                                     bool AllowModify) const {
  TBB = nullptr;
  FBB = nullptr;
  Cond.clear();

  // Walk backwards to find the first terminator.
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    auto Prev = std::prev(I);
    if (Prev->isDebugInstr()) {
      I = Prev;
      continue;
    }
    if (!Prev->isTerminator())
      break;
    I = Prev;
  }

  // Collect up to two terminators (conditional + optional unconditional).
  SmallVector<MachineInstr *, 2> Terms;
  for (auto It = I; It != MBB.end(); ++It) {
    if (It->isDebugInstr())
      continue;
    if (!It->isTerminator())
      break;
    Terms.push_back(&*It);
  }

  if (Terms.empty())
    return false;
  if (Terms.size() > 2)
    return true;

  // Unconditional branch only.
  if (Terms.size() == 1 && Terms[0]->getOpcode() == LinxISA::JUMP) {
    TBB = Terms[0]->getOperand(0).getMBB();
    return false;
  }

  // Conditional only (fallthrough on false).
  if (Terms.size() == 1 && Terms[0]->isConditionalBranch()) {
    TBB = Terms[0]->getOperand(2).getMBB();
    Cond.push_back(MachineOperand::CreateImm(Terms[0]->getOpcode()));
    Cond.push_back(Terms[0]->getOperand(0));
    Cond.push_back(Terms[0]->getOperand(1));
    return false;
  }

  // Conditional + unconditional.
  if (Terms.size() == 2 && Terms[1]->getOpcode() == LinxISA::JUMP &&
      Terms[0]->isConditionalBranch()) {
    TBB = Terms[0]->getOperand(2).getMBB();
    FBB = Terms[1]->getOperand(0).getMBB();
    Cond.push_back(MachineOperand::CreateImm(Terms[0]->getOpcode()));
    Cond.push_back(Terms[0]->getOperand(0));
    Cond.push_back(Terms[0]->getOperand(1));
    return false;
  }

  return true;
}
