//===-- LinxISAPCRelPairing.cpp - Preserve ADDTPC/LO pairing -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbol.h"

using namespace llvm;

#define DEBUG_TYPE "linx-pcrel-pairing"

namespace {

class LinxISAPCRelPairing : public MachineFunctionPass {
public:
  static char ID;
  LinxISAPCRelPairing() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  StringRef getPassName() const override { return "Linx PC-relative pairing"; }
};

} // namespace

char LinxISAPCRelPairing::ID = 0;

INITIALIZE_PASS(LinxISAPCRelPairing, "linx-pcrel-pairing",
                "Linx PC-relative pairing", false, false)

bool LinxISAPCRelPairing::runOnMachineFunction(MachineFunction &MF) {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  DenseMap<MachineInstr *, MCSymbol *> Anchors;
  bool Changed = false;

  for (MachineBasicBlock &MBB : MF) {
    for (auto It = MBB.begin(); It != MBB.end();) {
      MachineInstr &MI = *It++;
      if (MI.getOpcode() != LinxISA::ADDIri &&
          MI.getOpcode() != LinxISA::ADDIWri)
        continue;
      if (MI.getNumOperands() < 3 || !MI.getOperand(1).isReg() ||
          !MI.getOperand(1).getReg().isVirtual())
        continue;

      MachineInstr *Hi = MRI.getVRegDef(MI.getOperand(1).getReg());
      if (!Hi || Hi->getOpcode() != LinxISA::ADDTPC ||
          Hi->getNumOperands() < 2 ||
          !Hi->getOperand(1).isIdenticalTo(MI.getOperand(2)))
        continue;

      MCSymbol *&Anchor = Anchors[Hi];
      if (!Anchor) {
        Anchor = MF.getContext().createNamedTempSymbol("pcrel_hi");
        Hi->setPreInstrSymbol(MF, Anchor);
      }
      const unsigned TargetFlags = MI.getOperand(2).getTargetFlags();
      MI.getOperand(2).ChangeToMCSymbol(Anchor, TargetFlags);
      Changed = true;
    }
  }
  return Changed;
}

FunctionPass *llvm::createLinxISAPCRelPairingPass() {
  return new LinxISAPCRelPairing();
}
