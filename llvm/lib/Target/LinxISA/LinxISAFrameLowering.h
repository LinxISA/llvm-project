//===-- LinxISAFrameLowering.h - Frame lowering for LinxISA -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISAFRAMELOWERING_H
#define LLVM_LIB_TARGET_LINXISA_LINXISAFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class BitVector;
class RegScavenger;

class LinxISAFrameLowering : public TargetFrameLowering {
public:
  LinxISAFrameLowering()
      : TargetFrameLowering(StackGrowsDown, Align(16), /*LocalAreaOffset=*/0) {}

  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    // LinxISA uses explicit call-sequence stack adjustments for outgoing stack
    // arguments. The fixed "home" area is provided by the FENTRY/FRET template
    // blocks (see QEMU model and LinxISATargetLowering::LowerFormalArguments).
    return false;
  }

  void determineCalleeSaves(MachineFunction &MF, BitVector &SavedRegs,
                            RegScavenger *RS = nullptr) const override;

  void processFunctionBeforeFrameFinalized(
      MachineFunction &MF, RegScavenger *RS = nullptr) const override;

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  bool spillCalleeSavedRegisters(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MI,
                                 ArrayRef<CalleeSavedInfo> CSI,
                                 const TargetRegisterInfo *TRI) const override;

  bool restoreCalleeSavedRegisters(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MI,
                                   MutableArrayRef<CalleeSavedInfo> CSI,
                                   const TargetRegisterInfo *TRI) const override;

  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;

protected:
  bool hasFPImpl(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISAFRAMELOWERING_H
