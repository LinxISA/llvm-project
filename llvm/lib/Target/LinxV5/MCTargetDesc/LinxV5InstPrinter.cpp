//===-- LinxV5InstPrinter.cpp - Convert LinxV5 MCInst to asm syntax ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an LinxV5 MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "LinxV5InstPrinter.h"
#include "LinxV5BaseInfo.h"
#include "LinxV5MCExpr.h"
#include "MCTargetDesc/LinxV5CompressInst.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"
#include <unordered_map>

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "LinxV5GenAsmWriter.inc"

static cl::opt<bool>
    NoAliases("linxv5-no-aliases",
              cl::desc("Disable the emission of assembler pseudo instructions"),
              cl::init(false), cl::Hidden);

static cl::opt<bool>
    ArchRegNames("linxv5-arch-reg-names",
                 cl::desc("Print architectural register names rather than the "
                          "ABI names (such as r1 instead of sp)"),
                 cl::init(false), cl::Hidden);

void LinxV5InstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot, const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  if (NoAliases || !printAliasInstr(MI, Address, STI, O)) {
    printInstruction(MI, Address, STI, O);
  }
  printAnnotation(O, Annot);
}

void LinxV5InstPrinter::printRegName(raw_ostream &O, unsigned RegNo) const {
  O << getRegisterName(RegNo);
}

void LinxV5InstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI, raw_ostream &O,
                                     const char *Modifier) {
  assert((!Modifier || Modifier[0] == 0) && "No modifiers supported");
  const MCOperand &MO = MI->getOperand(OpNo);

  if (MO.isReg()) {
    printRegName(O, MO.getReg());
    return;
  }

  if (MO.isImm()) {
    O << MO.getImm();
    return;
  }

  assert(MO.isExpr() && "Unknown operand kind in printOperand");
  MO.getExpr()->print(O, &MAI);
}

void LinxV5InstPrinter::printBranchOperand(const MCInst *MI, uint64_t Address,
                                           unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  if (!MO.isImm())
    return printOperand(MI, OpNo, STI, O);

  if (PrintBranchImmAsAddress) {
    uint64_t Target = Address + MO.getImm();
    O << formatHex(Target);
  } else {
    O << MO.getImm();
  }
}

void LinxV5InstPrinter::printBareBlock(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  assert(MO.isBareSymbolRef() &&
         "printBareBlock can only print block operands");
  printOperand(MI, OpNo, STI, O);
  O << ".bstart";
}

void LinxV5InstPrinter::printSIMTRegOp(const MCInst *MI, unsigned OpNo,
                          const MCSubtargetInfo &STI, raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  switch(MI->getOpcode()) {
    default:
      break;
    case LinxV5::SIMT_ADD:
    case LinxV5::SIMT_SUB:
    case LinxV5::SIMT_CSEL:
      if (MO.isImm() && MO.getImm() == 0b11)
        O << ".neg";
      break;
    case LinxV5::SIMT_AND:
    case LinxV5::SIMT_OR:
    case LinxV5::SIMT_XOR:
     if (MO.isImm() && MO.getImm() == 0b11)
        O << ".not";
      break;
  }
}

const char *LinxV5InstPrinter::getRegisterName(unsigned RegNo) {
  return getRegisterName(RegNo, ArchRegNames ? LinxV5::NoRegAltName
                                             : LinxV5::ABIRegAltName);
}

void LinxV5InstPrinter::printLinxV5SrcRType(const MCInst *MI, unsigned OpNo,
                                            const MCSubtargetInfo &STI,
                                            raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);

  switch (MI->getOpcode()) {
  default:
    llvm_unreachable("Unsupport Inst for LinxV5 SrcType!");
  case LinxV5::ADD: {
    std::string TypeStr[4] = {"", ".sw", ".uw", ".neg"};
    O << TypeStr[MO.getImm()];
  }
  }
}

void LinxV5InstPrinter::printRegWithSrcRTypeImpl(const MCInst *MI,
                                                 unsigned OpNo,
                                                 const MCSubtargetInfo &STI,
                                                 raw_ostream &O,
                                                 StringRef Asm) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << getRegisterName(Reg) << Asm;
}

void LinxV5InstPrinter::printLinxV5SrcRShamt(const MCInst *MI, unsigned OpNo,
                                             const MCSubtargetInfo &STI,
                                             raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  if (MO.getImm() != 0)
    O << "<<" << MO.getImm();
}

template <unsigned S>
void LinxV5InstPrinter::printImmShifted(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  int64_t Imm = MI->getOperand(OpNo).getImm();
  O << (Imm << S);
}

template <unsigned S>
void LinxV5InstPrinter::printShamtImmPlus(const MCInst *MI, unsigned OpNo,
                                          const MCSubtargetInfo &STI,
                                          raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  if (Imm + S != 0)
    O << "<<" << (Imm + S);
}

void LinxV5InstPrinter::printShamtImm(const MCInst *MI, unsigned OpNo,
                                      const MCSubtargetInfo &STI,
                                      raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  if (Imm != 0)
    O << "<<" << Imm;
}

void LinxV5InstPrinter::printSIMTIntSrcRegType(const MCInst *MI, unsigned OpNo,
                                               const MCSubtargetInfo &STI,
                                               raw_ostream &O) {

  if (OpNo > 0) {
    auto RegNo = MI->getOperand(OpNo - 1).getReg();
    if (LinxV5MCRegisterClasses[LinxV5::SIMT_TileBaseRegClassID].contains(
            RegNo) ||
        RegNo == LinxV5::SIMT_P)
        return;
  }
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  default:
    O << "unexpected SIMT src register type!";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD:
    O << ".ud";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW:
    O << ".uw";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH:
    O << ".uh";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB:
    O << ".ub";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD:
    O << ".sd";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_SW:
    O << ".sw";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_SH:
    O << ".sh";
    break;
  case LinxV5Op::SIMT_INT_SRC_REG_TYPE_SB:
    O << ".sb";
    break;
  }
}

void LinxV5InstPrinter::printSIMTFloatSrcRegType(const MCInst *MI,
                                                 unsigned OpNo,
                                                 const MCSubtargetInfo &STI,
                                                 raw_ostream &O) {
  if (OpNo > 0) {
    auto RegNo = MI->getOperand(OpNo - 1).getReg();
    if (LinxV5MCRegisterClasses[LinxV5::SIMT_TileBaseRegClassID].contains(
            RegNo)) {
        O << "unexpected SIMT src register type!";
        return;
    }
  }
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  default:
    O << "unexpected SIMT src register type!";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FD:
    O << ".fd";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FS:
    O << ".fs";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FH:
    O << ".fh";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FB:
    O << ".fb";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_BF:
    O << ".bf";
    break;
  case LinxV5Op::SIMT_FLOAT_SRC_REG_TYPE_FLB:
    O << ".flb";
    break;
  }
}

void LinxV5InstPrinter::printSIMTDstRegType(const MCInst *MI, unsigned OpNo,
                                            const MCSubtargetInfo &STI,
                                            raw_ostream &O) {
  if (OpNo > 0) {
    auto RegNo = MI->getOperand(OpNo - 1).getReg();
    if (RegNo == LinxV5::SIMT_P)
        return;
  }
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  default:
    O << "unexpected SIMT dst register type!";
    break;
  case LinxV5Op::SIMT_INT_DST_REG_TYPE_D:
    O << ".d";
    break;
  case LinxV5Op::SIMT_INT_DST_REG_TYPE_W:
    O << ".w";
    break;
  case LinxV5Op::SIMT_INT_DST_REG_TYPE_H:
    O << ".h";
    break;
  case LinxV5Op::SIMT_INT_DST_REG_TYPE_B:
    O << ".b";
    break;
  }
}

void LinxV5InstPrinter::printDstRWithArrow(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << "\t->" << getRegisterName(Reg);
}

void LinxV5InstPrinter::printGPRPlusImm(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  if (Reg == LinxV5::R0 && MI->getOperand(OpNo + 1).getImm() != 0) {
    return;
  }
  O << getRegisterName(Reg);
}

void LinxV5InstPrinter::printDRImm(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case 0:
    O << "MR";
    break;
  case 1:
    O << "DR";
    break;
  default:
    return;
  }
}

void LinxV5InstPrinter::printPlusImm17(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  if (Imm == 0) {
    return;
  }
  if (MI->getOperand(OpNo - 1).getReg() == LinxV5::R0) {
    O << Imm;
    return;
  }
  O << "+" << Imm;
}

void LinxV5InstPrinter::printGroupOp(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  assert(Imm <= 0b1 && "Group only can be 0 or 1");
  if (Imm == 1) {
    O << ", last";
  }
}

void LinxV5InstPrinter::printSIMTSrcReg(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << getRegisterName(Reg);
}

void LinxV5InstPrinter::printLoopBRegDstRWithArrow(const MCInst *MI, unsigned OpNo,
                                           const MCSubtargetInfo &STI,
                                           raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  O << "\t->lb" << Imm;
}

void LinxV5InstPrinter::printDepSrcReg(const MCInst *MI, unsigned OpNo,
                  const MCSubtargetInfo &STI,raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  if(Reg != 0 && Reg != LinxV5::Dep_DOS0)
    O << getRegisterName(Reg);
}

void LinxV5InstPrinter::printTileSrcReg(const MCInst *MI, unsigned OpNo,
                  const MCSubtargetInfo &STI,raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << getRegisterName(Reg);
}
void LinxV5InstPrinter::printTileDstReg(const MCInst *MI, unsigned OpNo,
                  const MCSubtargetInfo &STI,raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << "\t->" << getRegisterName(Reg);
}

void LinxV5InstPrinter::printGPRWithBracket(const MCInst *MI, unsigned OpNo,
                  const MCSubtargetInfo &STI,raw_ostream &O) {
  unsigned Reg = MI->getOperand(OpNo).getReg();
  O << "<" << getRegisterName(Reg) << ">";
}

void LinxV5InstPrinter::printTileSizeWithBracket(const MCInst *MI, unsigned OpNo,
                    const MCSubtargetInfo &STI,raw_ostream &O) {
  if (MI->getOperand(OpNo).isExpr()) {
    const MCExpr *Expr = MI->getOperand(OpNo).getExpr();
    O << "<"; Expr->print(O, &MAI); O << ">";
    return;
  }

  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch(Imm) {
    case(0):
    O << "<0B>";
    break;
    case(1):
    O << "<32B>";
    break;
    case(2):
    O << "<64B>";
    break;
    case(3):
    O << "<128B>";
    break;
    case(4):
    O << "<256B>";
    break;
    case(5):
    O << "<512B>";
    break;
    case(6):
    O << "<1KB>";
    break;
    case(7):
    O << "<2KB>";
    break;
    case(8):
    O << "<4KB>";
    break;
    case(9):
    O << "<8KB>";
    break;
    case(10):
    O << "<16KB>";
    break;
    case(11):
    O << "<32KB>";
    break;
    case(12):
    O << "<64KB>";
    break;
    case(13):
    O << "<128KB>";
    break;
    case(14):
    O << "<256KB>";
    break;
    case(15):
    O << "<512KB>";
    break;
    default:
    return;
  }
}

void LinxV5InstPrinter::printBStartWithoutTargetBrType(
    const MCInst *MI, unsigned OpNo, const MCSubtargetInfo &STI,
    raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  default:
    O << "unexpected BStart Branch Type!";
    break;
  case LinxV5Op::BranchType::FALL:
    O << "";
    break;
  case LinxV5Op::BranchType::IND:
    O << "IND";
    break;
  case LinxV5Op::BranchType::ICALL:
    O << "ICALL";
    break;
  case LinxV5Op::BranchType::RET:
    O << "RET";
    break;
  }
}

void LinxV5InstPrinter::printBAttrType(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  std::vector<std::string> vec;
  if ((Imm & LinxV5Op::AttrType::TRAP) == LinxV5Op::AttrType::TRAP)
    vec.push_back("TRAP");
  if ((Imm & LinxV5Op::AttrType::ATOMIC) == LinxV5Op::AttrType::ATOMIC)
    vec.push_back("ATOMIC");

  if ((Imm & LinxV5Op::AttrType::AQRL) == LinxV5Op::AttrType::AQRL)
    vec.push_back("AQRL");
  else if ((Imm & LinxV5Op::AttrType::RL) == LinxV5Op::AttrType::RL)
    vec.push_back("RL");
  else if ((Imm & LinxV5Op::AttrType::AQ) == LinxV5Op::AttrType::AQ)
    vec.push_back("AQ");

  if ((Imm & LinxV5Op::AttrType::FAR) == LinxV5Op::AttrType::FAR)
    vec.push_back("FAR");

  for (int i = 0; i < vec.size(); ++i) {
    O << vec[i];
    if (i != vec.size() - 1)
      O << ",";
  }
}

void LinxV5InstPrinter::printBArgFormat(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
#define TRANS(NAME, CODE)                                                      \
  case LinxV5Op::ArgFormat::NAME:                                              \
    O << #NAME;                                                                \
    break;
#include "LinxV5TileTrans.def"
#undef TRANS
  default:
    O << "reserve";
    break;
  }
}

void LinxV5InstPrinter::printRMode(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
#define RMODE(NAME, CODE)                                                      \
  case LinxV5Op::RMode::NAME:                                                  \
    O << #NAME;                                                                \
    break;
#include "LinxV5TileRMode.def"
#undef RMODE
  default:
    O << "reserve";
    break;
  }
}

void LinxV5InstPrinter::printPadValue(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::PadValue::Zero:
    O << "Zero";
    break;
  case LinxV5Op::PadValue::Max:
    O << "Max";
    break;
  case LinxV5Op::PadValue::Min:
    O << "Min";
    break;
  case LinxV5Op::PadValue::Null:
    O << "Null";
    break;
  default:
    O << "Others";
    break;
  }
}

void LinxV5InstPrinter::printCmpMode(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI,
                                     raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::CmpMode::EQ:
    O << "Eq";
    break;
  case LinxV5Op::CmpMode::NE:
    O << "NE";
    break;
  case LinxV5Op::CmpMode::LT:
    O << "LT";
    break;
  case LinxV5Op::CmpMode::GT:
    O << "GT";
    break;
  case LinxV5Op::CmpMode::LE:
    O << "LE";
    break;
  case LinxV5Op::CmpMode::GE:
    O << "GE";
    break;
  default:
    O << "Others";
    break;
  }
}

void LinxV5InstPrinter::printCanon(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::Canon::NORMAL_CANON:
    O << ".normal";
    break;
  case LinxV5Op::Canon::CANON:
    O << ".canon";
    break;
  default:
    O << "<invalid-canon>";
    break;
  }
}

void LinxV5InstPrinter::printSat(const MCInst *MI, unsigned OpNo,
                                 const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::Sat::NOSAT:
    O << "nosat";
    break;
  case LinxV5Op::Sat::SAT:
    O << "sat";
    break;
  default:
    O << "<invalid-sat>";
    break;
  }
}

void LinxV5InstPrinter::printByteID(const MCInst *MI, unsigned OpNo,
                                    const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::ByteID::BYTE0:
    O << "byte0";
    break;
  case LinxV5Op::ByteID::BYTE1:
    O << "byte1";
    break;
  case LinxV5Op::ByteID::BYTE2:
    O << "byte2";
    break;
  case LinxV5Op::ByteID::BYTE3:
    O << "byte3";
    break;
  default:
    O << "<invalid-byteid>";
    break;
  }
}

void LinxV5InstPrinter::printGPRBitMap(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned long Imm = MI->getOperand(OpNo).getImm();
  bool First = true;
  for (unsigned i = 0, e = 24; i != e; ++i) {
    if (Imm & (0b1 << i)) {
      if (First)
        First = false;
      else
        O << ", ";
      unsigned Reg = LinxV5::R0 + i;
      O << getRegisterName(Reg);
    }
  }
}

void LinxV5InstPrinter::printPseudoBIO(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned long mask = MI->getOperand(OpNo).getImm();
  std::vector<std::string> vec;
  for (unsigned i = 0; i < 24; i++) {
    if ((mask >> i) & 1) {
      vec.push_back(getRegisterName(LinxV5::R0 + i));
    }
  }

  for (int i = 0; i < vec.size(); ++i) {
    O << vec[i];
    if (i != vec.size() - 1)
      O << ",";
  }
}

void LinxV5InstPrinter::printGPRList(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI,
                                     raw_ostream &O) {
  unsigned Reg;
  std::vector<std::string> vec;
  for (unsigned i = 0; (OpNo + i) < MI->getNumOperands(); i++) {
    Reg = MI->getOperand(OpNo + i).getReg();
    vec.push_back(getRegisterName(Reg));
  }

  for (int i = 0; i < vec.size(); ++i) {
    O << vec[i];
    if (i != vec.size() - 1)
      O << ",";
  }
}

void LinxV5InstPrinter::printFenceFlag(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned Flag = MI->getOperand(OpNo).getImm();
  if (!(Flag & LinxV5Op::FF_MASK)) {
    O << "unexpected fence.d operand";
  }
  if (Flag & LinxV5Op::FF_DEVI)
    O << 'i';
  if (Flag & LinxV5Op::FF_DEVO)
    O << 'o';
  if (Flag & LinxV5Op::FF_MEMR)
    O << 'r';
  if (Flag & LinxV5Op::FF_MEMW)
    O << 'w';
}

void LinxV5InstPrinter::printBstartDataType(const MCInst *MI, unsigned OpNo,
                                            const MCSubtargetInfo &STI,
                                            raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::DataType::FP64:
    O << "FP64";
    break;
  case LinxV5Op::DataType::FP32:
    O << "FP32";
    break;
  case LinxV5Op::DataType::TF32:
    O << "TF32";
    break;
  case LinxV5Op::DataType::HF32:
    O << "HF32";
    break;
  case LinxV5Op::DataType::FP16:
    O << "FP16";
    break;
  case LinxV5Op::DataType::BF16:
    O << "BF16";
    break;
  case LinxV5Op::DataType::HiF8:
    O << "HiF8";
    break;
  case LinxV5Op::DataType::e4m3:
    O << "e4m3";
    break;
  case LinxV5Op::DataType::e5m2:
    O << "e5m2";
    break;
  case LinxV5Op::DataType::e3m2:
    O << "e3m2";
    break;
  case LinxV5Op::DataType::e2m3:
    O << "e2m3";
    break;
  case LinxV5Op::DataType::e2m1x2:
    O << "e2m1x2";
    break;
  case LinxV5Op::DataType::e1m2x2:
    O << "e1m2x2";
    break;
  case LinxV5Op::DataType::e8m0:
    O << "e8m0";
    break;
  case LinxV5Op::DataType::HiF4x2:
    O << "HiF4x2";
    break;
  case LinxV5Op::DataType::S64:
    O << "S64";
    break;
  case LinxV5Op::DataType::S32:
    O << "S32";
    break;
  case LinxV5Op::DataType::S16:
    O << "S16";
    break;
  case LinxV5Op::DataType::S8:
    O << "S8";
    break;
  case LinxV5Op::DataType::S4x2:
    O << "S4x2";
    break;
  case LinxV5Op::DataType::U64:
    O << "U64";
    break;
  case LinxV5Op::DataType::U32:
    O << "U32";
    break;
  case LinxV5Op::DataType::U16:
    O << "U16";
    break;
  case LinxV5Op::DataType::U8:
    O << "U8";
    break;
  case LinxV5Op::DataType::U4x2:
    O << "U4x2";
    break;
  case LinxV5Op::DataType::EMPTY_DataType:
    O << "";
    break;
  default:
    O << "<invalid-dtype>";
    break;
  }
}

void LinxV5InstPrinter::printTileOPTMA(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::TileOPTMA::TLOAD:
    O << "TLOAD";
    break;
  case LinxV5Op::TileOPTMA::TSTORE:
    O << "TSTORE";
    break;
  case LinxV5Op::TileOPTMA::TMOV:
    O << "TMOV";
    break;
  case LinxV5Op::TileOPTMA::MGATHER:
    O << "MGATHER";
    break;
  case LinxV5Op::TileOPTMA::MSCATTER:
    O << "MSCATTER";
    break;
  case LinxV5Op::TileOPTMA::MGATHER_MASK:
    O << "MGATHER.MASK";
    break;
  case LinxV5Op::TileOPTMA::MSCATTER_MASK:
    O << "MSCATTER.MASK";
    break;
  default:
    O << Imm;
    break;
  }
}

void LinxV5InstPrinter::printTileOPCUBE(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::TileOPCUBE::MAMULB:
    O << "TMATMUL";
    break;
  case LinxV5Op::TileOPCUBE::MAMULBAC:
    O << "TMATMUL.BIAS";
    break;
  case LinxV5Op::TileOPCUBE::MAMULB_ACC:
    O << "TMATMUL.ACC";
    break;
  case LinxV5Op::TileOPCUBE::MAMULBMX:
    O << "TMATMULMX";
    break;
  case LinxV5Op::TileOPCUBE::MAMULBMXAC:
    O << "TMATMULMX.BIAS";
    break;
  case LinxV5Op::TileOPCUBE::MAMULBMX_ACC:
    O << "TMATMULMX.ACC";
    break;
  case LinxV5Op::TileOPCUBE::ACCCVT:
    O << "ACCCVT";
    break;
  default:
    O << Imm;
    break;
  }
}

static const std::unordered_map<unsigned, const char *> TileOpMap = {
    {0b00000, "TADD"},
    {0b00001, "TSUB"},
    {0b00010, "TMUL"},
    {0b00011, "TDIV"},
    {0b00100, "TREM"},
    {0b00101, "TFMOD"},
    {0b00110, "TAND"},
    {0b00111, "TOR"},
    {0b01000, "TXOR"},
    {0b01001, "TSHL"},
    {0b01010, "TSHR"},
    {0b01011, "TMAX"},
    {0b01100, "TMIN"},
    {0b01101, "TCMP"},
    {0b01110, "TPRELU"},
    {0b01111, "TABS"},
    {0b10000, "TNOT"},
    {0b10001, "TNEG"},
    {0b10010, "TEXP"},
    {0b10011, "TLOG"},
    {0b10100, "TRECIP"},
    {0b10101, "TSQRT"},
    {0b10110, "TRSQRT"},
    {0b10111, "TRELU"},
    {0b11000, "TADDC"},
    {0b11001, "TSUBC"},
    {0b11010, "TSEL"},
    {0b11011, "TCVT"},

    {0b0100000, "TADDS"},
    {0b0100001, "TSUBS"},
    {0b0100010, "TMULS"},
    {0b0100011, "TDIVS"},
    {0b0100100, "TREMS"},
    {0b0100101, "TFMODS"},
    {0b0100110, "TANDS"},
    {0b0100111, "TORS"},
    {0b0101000, "TXORS"},
    {0b0101001, "TSHLS"},
    {0b0101010, "TSHRS"},
    {0b0101011, "TMAXS"},
    {0b0101100, "TMINS"},
    {0b0101101, "TCMPS"},
    {0b0101110, "TLRELU"},

    {0b0111000, "TADDSC"},
    {0b0111001, "TSUBSC"},
    {0b0111010, "TSELS"},
    {0b0111011, "TEXPANDS"},

    {0b1000000, "TROWSUM"},
    {0b1000001, "TROWMAX"},
    {0b1000010, "TROWMIN"},
    {0b1000011, "TROWPROD"},
    {0b1000100, "TROWEXPAND"},
    {0b1000101, "TROWEXPANDADD"},
    {0b1000110, "TROWEXPANDSUB"},
    {0b1000111, "TROWEXPANDMUL"},
    {0b1001000, "TROWEXPANDDIV"},
    {0b1001001, "TROWEXPANDMAX"},
    {0b1001010, "TROWEXPANDMIN"},
    {0b1001011, "TROWEXPANDEXPDIF"},

    {0b1010000, "TCOLSUM"},
    {0b1010001, "TCOLMAX"},
    {0b1010010, "TCOLMIN"},
    {0b1010011, "TCOLPROD"},
    {0b1010100, "TCOLEXPAND"},
    {0b1010101, "TCOLEXPANDADD"},
    {0b1010110, "TCOLEXPANDSUB"},
    {0b1010111, "TCOLEXPANDMUL"},
    {0b1011000, "TCOLEXPANDDIV"},
    {0b1011001, "TCOLEXPANDMAX"},
    {0b1011010, "TCOLEXPANDMIN"},
    {0b1011011, "TCOLEXPANDEXPDIF"},

    {0b1101000, "THISTOGRAM"},
};

void LinxV5InstPrinter::printTileOPTEPL(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::TileOPTEPL::ESAVE:
    O << "ESAVE";
    break;
  case LinxV5Op::TileOPTEPL::ERCOV:
    O << "ERCOV";
    break;
  default: {
    auto It = TileOpMap.find(Imm);
    if (It != TileOpMap.end()) {
      O << It->second;
    } else {
      O << Imm;
    }
    break;
  }
  }
}

void LinxV5InstPrinter::printTileOPMode(const MCInst *MI, unsigned OpNo,
                                        const MCSubtargetInfo &STI,
                                        raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::TileOPMode::VS8:
    O << "VS8";
    break;
  case LinxV5Op::TileOPMode::VS16:
    O << "VS16";
    break;
  case LinxV5Op::TileOPMode::VS32:
    O << "VS32";
    break;
  case LinxV5Op::TileOPMode::VS64:
    O << "VS64";
    break;
  default:
    O << Imm;
    break;
  }
}

void LinxV5InstPrinter::printTEPLMode(const MCInst *MI, unsigned OpNo,
                                      const MCSubtargetInfo &STI,
                                      raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  switch (Imm) {
  case LinxV5Op::TEPLMode::gprs:
    O << "gprs";
    break;
  case LinxV5Op::TEPLMode::tile:
    O << "tile";
    break;
  default:
    O << Imm;
    break;
  }
}
