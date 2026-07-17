//===-- LinxISAAsmPrinter.cpp - LinxISA Assembly Printer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISAMCInstLower.h"
#include "LinxISARegisterInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {

class LinxISAAsmPrinter : public llvm::AsmPrinter {
  std::unique_ptr<LinxISAMCInstLower> MCInstLowering;
  SmallPtrSet<const MachineBasicBlock *, 32> BodyLabelsEmitted;
  SmallPtrSet<const MachineInstr *, 32> SkippedFusedSetRet;

public:
  explicit LinxISAAsmPrinter(TargetMachine &TM,
                             std::unique_ptr<MCStreamer> Streamer)
      : llvm::AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "Linx Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MCInstLowering = std::make_unique<LinxISAMCInstLower>(
        OutContext, *this, *MF.getSubtarget().getRegisterInfo());
    BodyLabelsEmitted.clear();
    SkippedFusedSetRet.clear();
    return llvm::AsmPrinter::runOnMachineFunction(MF);
  }

  void emitInstruction(const MachineInstr *MI) override;

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode,
                       raw_ostream &OS) override;

  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode,
                             raw_ostream &OS) override;

  static char ID;
};

} // end anonymous namespace

void LinxISAAsmPrinter::emitInstruction(const MachineInstr *MI) {
  if (SkippedFusedSetRet.contains(MI))
    return;
  if (MI->isDebugInstr())
    return;

  switch (MI->getOpcode()) {
  // Call frame pseudos should have been eliminated by PEI.
  case LinxISA::ADJCALLSTACKDOWN:
  case LinxISA::ADJCALLSTACKUP:
    return;
  default:
    break;
  }

  bool Emitted = false;
  if (MI->getOpcode() == LinxISA::BSTART_STD_CALL &&
      OutStreamer->hasRawTextSupport()) {
    MachineBasicBlock *MBB = const_cast<MachineBasicBlock *>(MI->getParent());
    if (MBB) {
      auto It = const_cast<MachineInstr *>(MI)->getIterator();
      auto NextIt = std::next(It);
      while (NextIt != MBB->end() && NextIt->isDebugInstr())
        ++NextIt;

      if (NextIt != MBB->end() && NextIt->getOpcode() == LinxISA::SETRET) {
        MCInst BStartInst;
        MCInstLowering->Lower(MI, BStartInst);
        MCInst SetRetInst;
        MCInstLowering->Lower(&*NextIt, SetRetInst);

        SmallString<128> Line;
        raw_svector_ostream OS(Line);
        OS << "BSTART\tCALL, ";

        if (BStartInst.getNumOperands() >= 1) {
          const MCOperand &TargetOp = BStartInst.getOperand(0);
          if (TargetOp.isExpr())
            MAI->printExpr(OS, *TargetOp.getExpr());
          else if (TargetOp.isImm())
            OS << TargetOp.getImm();
        }

        OS << ", ra=";
        if (SetRetInst.getNumOperands() >= 1) {
          const MCOperand &RetOp = SetRetInst.getOperand(0);
          if (RetOp.isExpr())
            MAI->printExpr(OS, *RetOp.getExpr());
          else if (RetOp.isImm())
            OS << RetOp.getImm();
        }

        OutStreamer->emitRawText(OS.str());
        SkippedFusedSetRet.insert(&*NextIt);
        Emitted = true;
      }
    }
  }

  if (!Emitted) {
    MCSubtargetInfo STI = getSubtargetInfo();
    MCInst TmpInst;
    MCInstLowering->Lower(MI, TmpInst);
    OutStreamer->emitInstruction(TmpInst, STI);
  }
}

static StringRef linxReg5Name(unsigned Code) {
  static constexpr const char *Names[32] = {
      "zero", "sp",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
      "a6",   "a7",  "ra",  "s0",  "s1",  "s2",  "s3",  "s4",
      "s5",   "s6",  "s7",  "s8",  "x0",  "x1",  "x2",  "x3",
      "t#1",  "t#2", "t#3", "t#4", "u#1", "u#2", "u#3", "u#4",
  };
  if (Code < 32)
    return Names[Code];
  return "r?";
}

static void printLinxInlineAsmRegister(raw_ostream &OS, unsigned Reg,
                                       unsigned Enc) {
  if (LinxISA::TILERegClass.contains(Reg)) {
    static constexpr char Banks[4] = {'t', 'u', 'm', 'n'};
    OS << Banks[(Enc >> 3) & 0x3] << '#' << ((Enc & 0x7) + 1);
    return;
  }
  OS << linxReg5Name(Enc);
}

bool LinxISAAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode,
                                       raw_ostream &OS) {
  // Clang/GCC use %cN for an immediate without target punctuation. Linx block
  // templates rely on that conventional modifier for selector and dimension
  // fields. %qN prints only a tile register's destination queue bank because
  // canonical B.IOT destinations are written as ->t/u/m/n<Size>, while source
  // operands name a concrete queue slot such as t#1.
  if (ExtraCode && ExtraCode[0] != 0 &&
      !((ExtraCode[0] == 'c' || ExtraCode[0] == 'q') &&
        ExtraCode[1] == 0))
    return true;

  if (!MCInstLowering)
    return true;
  if (OpNo >= MI->getNumOperands())
    return true;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (MO.isReg()) {
    const unsigned Enc = MCInstLowering->getReg5Encoding(MO.getReg());
    if (ExtraCode && ExtraCode[0] == 'q') {
      if (!LinxISA::TILERegClass.contains(MO.getReg()))
        return true;
      static constexpr char Banks[4] = {'t', 'u', 'm', 'n'};
      OS << Banks[(Enc >> 3) & 0x3];
      return false;
    }
    printLinxInlineAsmRegister(OS, MO.getReg(), Enc);
    return false;
  }

  if (MO.isImm()) {
    OS << MO.getImm();
    return false;
  }

  if (MO.isCImm()) {
    OS << MO.getCImm()->getSExtValue();
    return false;
  }

  // Symbols and expressions.
  MCOperand MCOp;
  if (MCInstLowering->lowerOperand(MO, MCOp) && MCOp.isExpr()) {
    MAI->printExpr(OS, *MCOp.getExpr());
    return false;
  }

  return true;
}

bool LinxISAAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                             unsigned OpNo,
                                             const char *ExtraCode,
                                             raw_ostream &OS) {
  if (ExtraCode && ExtraCode[0] != 0)
    return true;

  if (!MCInstLowering)
    return true;
  if (OpNo >= MI->getNumOperands())
    return true;

  auto PrintOne = [&](const MachineOperand &MO) -> bool {
    if (MO.isReg()) {
      const unsigned Enc = MCInstLowering->getReg5Encoding(MO.getReg());
      printLinxInlineAsmRegister(OS, MO.getReg(), Enc);
      return false;
    }

    if (MO.isImm()) {
      OS << MO.getImm();
      return false;
    }

    if (MO.isCImm()) {
      OS << MO.getCImm()->getSExtValue();
      return false;
    }

    MCOperand MCOp;
    if (MCInstLowering->lowerOperand(MO, MCOp) && MCOp.isExpr()) {
      MAI->printExpr(OS, *MCOp.getExpr());
      return false;
    }

    return true;
  };

  const MachineOperand &BaseMO = MI->getOperand(OpNo);
  OS << "[";
  if (PrintOne(BaseMO))
    return true;

  if (OpNo + 1 < MI->getNumOperands()) {
    const MachineOperand &OffMO = MI->getOperand(OpNo + 1);
    bool EmitOffset = true;
    if (OffMO.isImm() && OffMO.getImm() == 0)
      EmitOffset = false;
    if (OffMO.isCImm() && OffMO.getCImm()->isZero())
      EmitOffset = false;

    if (EmitOffset) {
      OS << ", ";
      if (PrintOne(OffMO))
        return true;
    }
  }

  OS << "]";
  return false;
}

char LinxISAAsmPrinter::ID = 0;

INITIALIZE_PASS(LinxISAAsmPrinter, "linx-asm-printer",
                "Linx Assembly Printer", false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxISAAsmPrinter() {
  RegisterAsmPrinter<LinxISAAsmPrinter> X32(getTheLinx32Target());
  RegisterAsmPrinter<LinxISAAsmPrinter> X64(getTheLinx64Target());
}
