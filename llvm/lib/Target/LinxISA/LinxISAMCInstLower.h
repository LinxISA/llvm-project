//===-- LinxISAMCInstLower.h - Lower LinxISA MachineInstr to MCInst -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISAMCINSTLOWER_H
#define LLVM_LIB_TARGET_LINXISA_LINXISAMCINSTLOWER_H

#include "llvm/MC/MCInst.h"

namespace llvm {

class AsmPrinter;
class MCContext;
class MachineInstr;
class MachineOperand;
class TargetRegisterInfo;

class LinxISAMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;
  const TargetRegisterInfo &TRI;

public:
  LinxISAMCInstLower(MCContext &Ctx, AsmPrinter &Printer,
                     const TargetRegisterInfo &TRI)
      : Ctx(Ctx), Printer(Printer), TRI(TRI) {}

  void Lower(const MachineInstr *MI, MCInst &OutMI) const;

  bool lowerOperand(const MachineOperand &MO, MCOperand &OutOp) const;

  unsigned getReg5Encoding(unsigned Reg) const;
  unsigned getRegEncoding(unsigned Reg) const;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISAMCINSTLOWER_H
