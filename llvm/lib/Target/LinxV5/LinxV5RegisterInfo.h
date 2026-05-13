//===- LinxV5RegisterInfo.h - LinxV5 Register Information Impl -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the LinxV5 implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5REGISTERINFO_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5REGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "LinxV5GenRegisterInfo.inc"

namespace llvm {
class LinxV5Subtarget;

struct LinxV5RegisterInfo : public LinxV5GenRegisterInfo {
  const LinxV5Subtarget &STI;
  explicit LinxV5RegisterInfo(unsigned HwMode, const LinxV5Subtarget &ST);

  const TargetRegisterClass *getSTDRC() const;

  unsigned getRegPressureLimit(const TargetRegisterClass *RC,
                               MachineFunction &MF) const override;

  unsigned getRegPressureSetLimit(const MachineFunction &MF,
                                  unsigned Idx) const override;

  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;
  bool isAsmClobberable(const MachineFunction &MF,
                        MCRegister PhysReg) const override;

  const uint32_t *getNoPreservedMask() const override;

  void eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;

  bool requiresRegisterScavenging(const MachineFunction &MF) const override {
    return true;
  }

  bool requiresFrameIndexScavenging(const MachineFunction &MF) const override {
    return true;
  }

  bool isConstantPhysReg(MCRegister Phys) const override;

  bool
  shouldDoFullStageRAFromSpill(Register NewVReg, Register Spill,
                               const MachineRegisterInfo *MRI) const override;

  bool isUniformReg(const MachineRegisterInfo &MRI, Register Reg,
                    bool Continuous) const;
};

namespace LinxV5 {
Register getSPReg();
Register getRAReg();
Register getFPReg();
Register getBPReg();
Register getUSPReg();
Register getVSPReg();

MCRegister getLoopCounter(unsigned Depth = 0);
MCRegister getLoopBoundary(unsigned Depth = 0);
MCRegister getReduceDst(unsigned RetNo = 0);
} // end namespace LinxV5
} // namespace llvm

#endif
