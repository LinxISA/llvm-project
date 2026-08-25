//===-- LinxV5AsmPrinter.cpp - LinxV5 LLVM assembly writer ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to the LinxV5 assembly language.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5CompressInst.h"
#include "MCTargetDesc/LinxV5InstPrinter.h"
#include "MCTargetDesc/LinxV5MCExpr.h"
#include "MCTargetDesc/LinxV5TargetStreamer.h"
#include "MCTargetDesc/LinxV5TileOpExpand.h"
#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

STATISTIC(LinxV5NumInstrsCompressed,
          "Number of LinxV5 Compressed instructions emitted");

STATISTIC(NumVecInstrs, "Number of jcore-vec instructions");
STATISTIC(NumVecMems, "Number of jcore-vec memory instructions");
STATISTIC(NumVecFloats, "Number of jcore-vec float instructions");

namespace {
class LinxV5AsmPrinter : public AsmPrinter {
  LinxV5TargetStreamer &getTargetStreamer() {
    return static_cast<LinxV5TargetStreamer &>(
        *OutStreamer->getTargetStreamer());
  }
  const LinxV5Subtarget *STI;
  const LinxV5InstrInfo *TII;

public:
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AsmPrinter::getAnalysisUsage(AU);
  }

  explicit LinxV5AsmPrinter(TargetMachine &TM,
                            std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "LinxV5 Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override;
  void emitInstruction(const MachineInstr *MI) override;
  void emitStackSizeGlobal(const MachineFunction &MF);
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;
  bool
  shouldEmitLabelForBasicBlock(const MachineBasicBlock &MBB) const override {
    return true;
  }

  void emitFunctionBodyStart() override;
  void emitFunctionBodyEnd() override;

  void EmitToStreamer(MCStreamer &S, const MCInst &Inst);

  void emitStartOfAsmFile(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;
};
}

void LinxV5AsmPrinter::EmitToStreamer(MCStreamer &S, const MCInst &Inst) {
  MCInst CInst;
  bool Res = false;
  if (!STI->isSIMT()) {
    Res = LinxV5::tryCompressInst(CInst, Inst, *TM.getMCSubtargetInfo(),
                                  OutStreamer->getContext());
    if (Res)
      ++LinxV5NumInstrsCompressed;
  }

  if (STI->isSIMT()) {
    ++NumVecInstrs;
    auto &II = TII->get(Inst.getOpcode());
    if (II.mayLoad() || II.mayStore())
      ++NumVecMems;
    else if (LinxV5II::isFPInstr(II.TSFlags))
      ++NumVecFloats;
  }
  AsmPrinter::EmitToStreamer(*OutStreamer, Res ? CInst : Inst);
}

void LinxV5AsmPrinter::emitInstruction(const MachineInstr *MI) {
  if (MI->getOpcode() == LinxV5::PseudoLABEL) {
    OutStreamer->emitLabel(MI->getOperand(0).getMCSymbol());
    return;
  }

  if (MI->isCall())
    return;

  MCInst TmpInst;
  LowerLinxV5MachineInstrToMCInst(MI, TmpInst, *this);
  EmitToStreamer(*OutStreamer, TmpInst);
}

void LinxV5AsmPrinter::emitStackSizeGlobal(const MachineFunction &MF) {
  if (!MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return;

  const std::string &FuncName = MF.getName().str();
  std::string GlobalVarName = FuncName + "_stack_size";

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  uint64_t StackSize = MFI.getStackSize();

  MCSection *StackSizeSection = getObjFileLowering().getContext()
                                  .getELFSection(".stack_size", ELF::SHT_PROGBITS, ELF::SHF_ALLOC);

  MCSection *CurSection = OutStreamer->getCurrentSection().first;
  OutStreamer->switchSection(StackSizeSection);

  MCSymbol *GlobalSym = OutContext.getOrCreateSymbol(GlobalVarName);
  OutStreamer->emitSymbolAttribute(GlobalSym, MCSA_Global);
  OutStreamer->emitLabel(GlobalSym);

  OutStreamer->emitIntValue(StackSize, 4);

  OutStreamer->switchSection(CurSection);
}

bool LinxV5AsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode, raw_ostream &OS) {
  const MachineOperand &MO = MI->getOperand(OpNo);

  // Handle LinxV5-specific modifiers before the generic printer. In
  // particular, %S carries a Shared_ABS register and must always print the
  // architectural S#n name rather than being consumed as a generic modifier.
  if (ExtraCode && ExtraCode[0] == 'S' && ExtraCode[1] == 0) {
    if (!MO.isReg() ||
        !LinxV5::Shared_ABSRegClass.contains(MO.getReg()))
      return true;
    OS << LinxV5InstPrinter::getRegisterName(MO.getReg(),
                                             LinxV5::ABIRegAltName);
    return false;
  }
  // v5: %Z carries a SizeCode imm (0..12) and prints the size text
  // ("128B"/.."256KB") without brackets; the surrounding "<" ">" are
  // written literally in the asm string ("->%[Dst]<%Z[TileSize]>"). SizeCode
  // is encoded at PE granularity (the hardware multiplies by 4 for the core).
  if (ExtraCode && ExtraCode[0] == 'Z' && ExtraCode[1] == 0) {
    if (!MO.isImm()) {
      // The TSize must be a compile-time immediate ("i" constraint). If an
      // optimization pass degraded it to a register, do not emit the generic
      // "<>" empty box (which would corrupt the B.IOT into "->u<>"); print a
      // visible sentinel so the malformed bundle fails loudly downstream.
      OS << "0B";
      return false;
    }
    static const char *TileSizes[] = {
        "0B",   "128B", "256B", "512B",  "1KB",  "2KB",  "4KB",
        "8KB",  "16KB", "32KB", "64KB",  "128KB", "256KB"};
    if ((unsigned)MO.getImm() < sizeof(TileSizes) / sizeof(TileSizes[0]))
      OS << TileSizes[MO.getImm()];
    return false;
  }

  if (ExtraCode && ExtraCode[0] == 'D' && ExtraCode[1] == 0) {
    if (!MO.isImm())
      return true;
    switch (MO.getImm()) {
    case LinxV5Op::DataType::FP64: OS << "FP64"; break;
    case LinxV5Op::DataType::FP32: OS << "FP32"; break;
    case LinxV5Op::DataType::TF32: OS << "TF32"; break;
    case LinxV5Op::DataType::HF32: OS << "HF32"; break;
    case LinxV5Op::DataType::FP16: OS << "FP16"; break;
    case LinxV5Op::DataType::BF16: OS << "BF16"; break;
    case LinxV5Op::DataType::HiF8: OS << "HiF8"; break;
    case LinxV5Op::DataType::e4m3: OS << "e4m3"; break;
    case LinxV5Op::DataType::e5m2: OS << "e5m2"; break;
    case LinxV5Op::DataType::e3m2: OS << "e3m2"; break;
    case LinxV5Op::DataType::e2m3: OS << "e2m3"; break;
    case LinxV5Op::DataType::e2m1x2: OS << "e2m1x2"; break;
    case LinxV5Op::DataType::e1m2x2: OS << "e1m2x2"; break;
    case LinxV5Op::DataType::e8m0: OS << "e8m0"; break;
    case LinxV5Op::DataType::HiF4x2: OS << "HiF4x2"; break;
    case LinxV5Op::DataType::S64: OS << "S64"; break;
    case LinxV5Op::DataType::S32: OS << "S32"; break;
    case LinxV5Op::DataType::S16: OS << "S16"; break;
    case LinxV5Op::DataType::S8: OS << "S8"; break;
    case LinxV5Op::DataType::S4x2: OS << "S4x2"; break;
    case LinxV5Op::DataType::U64: OS << "U64"; break;
    case LinxV5Op::DataType::U32: OS << "U32"; break;
    case LinxV5Op::DataType::U16: OS << "U16"; break;
    case LinxV5Op::DataType::U8: OS << "U8"; break;
    case LinxV5Op::DataType::U4x2: OS << "U4x2"; break;
    case LinxV5Op::DataType::EMPTY_DataType: OS << "DTYPE_NONE"; break;
    default: OS << "<invalid-dtype>"; break;
    }
    return false;
  }

  // First try the generic code, which knows about modifiers like 'c' and 'n'.
  if (!AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS))
    return false;

  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0)
      return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default:
      return true; // Unknown modifier.
    case 'z':      // Print zero register if zero, regular printing otherwise.
      if (MO.isImm() && MO.getImm() == 0) {
        OS << LinxV5InstPrinter::getRegisterName(LinxV5::R0);
        return false;
      }
      break;
    case 'i': // Literal 'i' if operand is not a register.
      if (!MO.isReg())
        OS << 'i';
      return false;
    }
  }

  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  case MachineOperand::MO_Register:
    OS << LinxV5InstPrinter::getRegisterName(MO.getReg());
    return false;
  case MachineOperand::MO_GlobalAddress:
    PrintSymbolOperand(MO, OS);
    return false;
  default:
    break;
  }

  return true;
}

bool LinxV5AsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                             unsigned OpNo,
                                             const char *ExtraCode,
                                             raw_ostream &OS) {
  if (!ExtraCode) {
    const MachineOperand &MO = MI->getOperand(OpNo);
    // For now, we only support register memory operands in registers and
    // assume there is no addend
    if (!MO.isReg())
      return true;

    OS << "0(" << LinxV5InstPrinter::getRegisterName(MO.getReg()) << ")";
    return false;
  }

  return AsmPrinter::PrintAsmMemoryOperand(MI, OpNo, ExtraCode, OS);
}

bool LinxV5AsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<LinxV5Subtarget>();
  TII = STI->getInstrInfo();
  SetupMachineFunction(MF);
  emitFunctionBody();
  return false;
}

// SIMT Region for LLVM-MCA.
void LinxV5AsmPrinter::emitFunctionBodyStart() {
  if (!MF->getSubtarget<LinxV5Subtarget>().isSIMT())
    return;
  getTargetStreamer().emitRawText("# LLVM-MCA-BEGIN\n");
}

// SIMT Region for LLVM-MCA.
void LinxV5AsmPrinter::emitFunctionBodyEnd() {
  if (!MF->getSubtarget<LinxV5Subtarget>().isSIMT())
    return;

  getTargetStreamer().emitRawText("# LLVM-MCA-END\n");
  emitStackSizeGlobal(*MF);
}

void LinxV5AsmPrinter::emitStartOfAsmFile(Module &M) {}

void LinxV5AsmPrinter::emitEndOfAsmFile(Module &M) {}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5AsmPrinter() {
  RegisterAsmPrinter<LinxV5AsmPrinter> V4(getTheLinx64V5Target());
  RegisterAsmPrinter<LinxV5AsmPrinter> BE(getTheLinx64V5beTarget());
}
