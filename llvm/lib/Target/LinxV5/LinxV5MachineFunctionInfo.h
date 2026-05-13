//=- LinxV5MachineFunctionInfo.h - LinxV5 machine function info --*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares LinxV5-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5MACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5MACHINEFUNCTIONINFO_H

#include "LinxV5Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

/// LinxV5MachineFunctionInfo - This class is derived from MachineFunctionInfo
/// and contains private LinxV5-specific information for each MachineFunction.
class LinxV5MachineFunctionInfo : public MachineFunctionInfo {
public:
  explicit LinxV5MachineFunctionInfo(const MachineFunction &MF) {}

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  unsigned getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(int Size) { VarArgsSaveSize = Size; }

  int getMoveF64FrameIndex(MachineFunction &MF) {
    if (MoveF64FrameIndex == -1)
      MoveF64FrameIndex =
          MF.getFrameInfo().CreateStackObject(8, Align(8), false);
    return MoveF64FrameIndex;
  }

  void addSExt32Register(Register Reg) { SExt32Registers.push_back(Reg); }
  bool isSExt32Register(Register Reg) const {
    return is_contained(SExt32Registers, Reg);
  }

  void setTemplatePrologue(bool V = true) { TemplatePrologue = V; }
  void setTemplateEpilogue(bool V = true) { TemplateEpilogue = V; }

  bool hasTemplatePrologue() { return TemplatePrologue; }
  bool hasTemplateEpilogue() { return TemplateEpilogue; }

  void startClockhandsAllocation() { IsClockhandsAllocation = true; }

  void exitClockhandsAllocation() { IsClockhandsAllocation = false; }

  bool isClockhandsAllocation() const { return IsClockhandsAllocation; }

private:
  /// FrameIndex for start of varargs area
  int VarArgsFrameIndex = 0;
  /// Size of the save area used for varargs
  int VarArgsSaveSize = 0;
  /// FrameIndex used for transferring values between 64-bit FPRs and a pair
  /// of 32-bit GPRs via the stack.
  int MoveF64FrameIndex = -1;
  /// Size of any opaque stack adjustment due to save/restore libcalls.
  unsigned LibCallStackSize = 0;

  /// Use Template block as Prologue
  bool TemplatePrologue = false;
  /// Use Template block as Epilogue
  bool TemplateEpilogue = false;

  bool IsClockhandsAllocation = false;

  /// Registers that have been sign extended from i32.
  SmallVector<Register, 8> SExt32Registers;
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_LINX_LINXMACHINEFUNCTIONINFO_H
