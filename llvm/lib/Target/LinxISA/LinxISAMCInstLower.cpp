//===-- LinxISAMCInstLower.cpp - Lower LinxISA MachineInstr to MCInst ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISAMCInstLower.h"
#include "LinxISA.h"
#include "LinxISABaseInfo.h"
#include "MCTargetDesc/LinxISAMCAsmInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>
#include <string>
#include <vector>

using namespace llvm;

static unsigned findSpecOpcode(StringRef Mnemonic, unsigned LengthBits,
                               unsigned FieldCount) {
  for (unsigned Opc = 0; Opc < linxisa_inst_forms_count; ++Opc) {
    const linxisa_inst_form &F = linxisa_inst_forms[Opc];
    if (unsigned(F.length_bits) != LengthBits)
      continue;
    if (!F.mnemonic || Mnemonic != StringRef(F.mnemonic))
      continue;
    if (FieldCount && unsigned(F.field_count) != FieldCount)
      continue;
    return Opc;
  }

  SmallString<64> Msg;
  raw_svector_ostream OS(Msg);
  OS << "Linx: missing spec opcode for " << Mnemonic << " (" << LengthBits
     << "b)";
  report_fatal_error(OS.str());
}

static unsigned getSpecOpcode(StringRef Mnemonic, unsigned LengthBits,
                              unsigned FieldCount) {
  // Memoize by a stable key to keep lowering cheap.
  struct CacheEntry {
    std::string Key;
    unsigned Opcode;
  };
  static std::vector<CacheEntry> Cache;

  std::string Key =
      (Mnemonic + "/" + Twine(LengthBits) + "/" + Twine(FieldCount)).str();
  for (const CacheEntry &E : Cache)
    if (E.Key == Key)
      return E.Opcode;

  unsigned Opc = findSpecOpcode(Mnemonic, LengthBits, FieldCount);
  Cache.push_back({Key, Opc});
  return Opc;
}

static unsigned findSpecOpcodeByAsmFmt(StringRef AsmFmt, unsigned LengthBits) {
  for (unsigned Opc = 0; Opc < linxisa_inst_forms_count; ++Opc) {
    const linxisa_inst_form &F = linxisa_inst_forms[Opc];
    if (unsigned(F.length_bits) != LengthBits)
      continue;
    if (!F.asm_fmt || AsmFmt != StringRef(F.asm_fmt))
      continue;
    return Opc;
  }

  SmallString<96> Msg;
  raw_svector_ostream OS(Msg);
  OS << "Linx: missing spec opcode for asm fmt '" << AsmFmt << "' (" << LengthBits
     << "b)";
  report_fatal_error(OS.str());
}

static unsigned getSpecOpcodeByAsmFmt(StringRef AsmFmt, unsigned LengthBits) {
  struct CacheEntry {
    std::string Key;
    unsigned Opcode;
  };
  static std::vector<CacheEntry> Cache;

  std::string Key = (AsmFmt + "/" + Twine(LengthBits)).str();
  for (const CacheEntry &E : Cache)
    if (E.Key == Key)
      return E.Opcode;

  unsigned Opc = findSpecOpcodeByAsmFmt(AsmFmt, LengthBits);
  Cache.push_back({Key, Opc});
  return Opc;
}

unsigned LinxISAMCInstLower::getReg5Encoding(unsigned Reg) const {
  return TRI.getEncodingValue(Reg) & 0x1F;
}

static const MCExpr *withOffset(const MCExpr *Expr, int64_t Offset,
                                MCContext &Ctx) {
  if (!Offset)
    return Expr;
  return MCBinaryExpr::createAdd(Expr, MCConstantExpr::create(Offset, Ctx),
                                 Ctx);
}

static bool splitShiftedSignedImm(int64_t Imm, unsigned BaseBits,
                                  unsigned &ShamtOut,
                                  int64_t &BaseImmOut) {
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    int64_t Pow = (1LL << Sh);
    if (Imm % Pow != 0)
      continue;
    int64_t Base = Imm / Pow;
    if (isIntN(BaseBits, Base)) {
      ShamtOut = Sh;
      BaseImmOut = Base;
      return true;
    }
  }
  return false;
}

static bool splitShiftedUnsignedImm(uint64_t Imm, unsigned BaseBits,
                                    unsigned &ShamtOut,
                                    uint64_t &BaseImmOut) {
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    uint64_t Pow = (1ULL << Sh);
    if (Imm % Pow != 0)
      continue;
    uint64_t Base = Imm / Pow;
    if (isUIntN(BaseBits, Base)) {
      ShamtOut = Sh;
      BaseImmOut = Base;
      return true;
    }
  }
  return false;
}

bool LinxISAMCInstLower::lowerOperand(const MachineOperand &MO,
                                      MCOperand &OutOp) const {
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    if (MO.isImplicit())
      return false;
    OutOp = MCOperand::createImm(getReg5Encoding(MO.getReg()));
    return true;
  case MachineOperand::MO_Immediate:
    OutOp = MCOperand::createImm(MO.getImm());
    return true;
  case MachineOperand::MO_MachineBasicBlock: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_GlobalAddress: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(Printer.getSymbol(MO.getGlobal()), Ctx);
    Expr = withOffset(Expr, MO.getOffset(), Ctx);
    const unsigned TF = MO.getTargetFlags();
    if (TF & LinxII::MO_PLT)
      Expr = MCSpecifierExpr::create(Expr, LinxISA::S_PLT, Ctx);
    else if (TF & LinxII::MO_GOT)
      Expr = MCSpecifierExpr::create(Expr, LinxISA::S_GOT, Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_ExternalSymbol: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(
            Printer.GetExternalSymbolSymbol(MO.getSymbolName()), Ctx);
    Expr = withOffset(Expr, MO.getOffset(), Ctx);
    const unsigned TF = MO.getTargetFlags();
    if (TF & LinxII::MO_PLT)
      Expr = MCSpecifierExpr::create(Expr, LinxISA::S_PLT, Ctx);
    else if (TF & LinxII::MO_GOT)
      Expr = MCSpecifierExpr::create(Expr, LinxISA::S_GOT, Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_ConstantPoolIndex: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(Printer.GetCPISymbol(MO.getIndex()), Ctx);
    Expr = withOffset(Expr, MO.getOffset(), Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_JumpTableIndex: {
    const MCExpr *Expr =
        MCSymbolRefExpr::create(Printer.GetJTISymbol(MO.getIndex()), Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_MCSymbol: {
    const MCExpr *Expr = MCSymbolRefExpr::create(MO.getMCSymbol(), Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_BlockAddress: {
    const MCExpr *Expr = MCSymbolRefExpr::create(
        Printer.GetBlockAddressSymbol(MO.getBlockAddress()), Ctx);
    Expr = withOffset(Expr, MO.getOffset(), Ctx);
    OutOp = MCOperand::createExpr(Expr);
    return true;
  }
  case MachineOperand::MO_RegisterMask:
    return false;
  default:
    return false;
  }
}

void LinxISAMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  OutMI.clear();

  const unsigned Opc = MI->getOpcode();
  auto R = [&](unsigned OpNo) -> int64_t {
    if (OpNo >= MI->getNumOperands()) {
      MI->print(errs());
      report_fatal_error("Linx: missing register operand in MC lowering");
    }
    const MachineOperand &MO = MI->getOperand(OpNo);
    if (!MO.isReg()) {
      MI->print(errs());
      report_fatal_error("Linx: expected register operand in MC lowering");
    }
    Register Reg = MO.getReg();
    if (!Reg.isPhysical()) {
      MI->print(errs());
      report_fatal_error("Linx: expected physical register operand in MC lowering");
    }
    return static_cast<int64_t>(getReg5Encoding(Reg));
  };
  auto I = [&](unsigned OpNo) -> int64_t {
    if (OpNo >= MI->getNumOperands()) {
      MI->print(errs());
      report_fatal_error("Linx: missing immediate operand in MC lowering");
    }
    const MachineOperand &MO = MI->getOperand(OpNo);
    if (!MO.isImm()) {
      MI->print(errs());
      report_fatal_error("Linx: expected immediate operand in MC lowering");
    }
    return MO.getImm();
  };

  auto lowerBranchTarget = [&](unsigned OpNo) -> MCOperand {
    const MachineOperand &MO = MI->getOperand(OpNo);
    MCOperand Op;
    if (!lowerOperand(MO, Op) || !Op.isExpr()) {
      MI->print(errs());
      report_fatal_error("Linx: expected branch target expression operand");
    }
    return Op;
  };

  auto emitNamedImmFields =
      [&](unsigned SpecOpc,
          ArrayRef<std::pair<StringRef, int64_t>> NamedValues) -> void {
    if (SpecOpc >= linxisa_inst_forms_count)
      report_fatal_error("Linx: invalid spec opcode index");
    const linxisa_inst_form &Form = linxisa_inst_forms[SpecOpc];
    OutMI.setOpcode(SpecOpc);

    for (unsigned i = 0; i < Form.field_count; ++i) {
      const linxisa_field &Field = linxisa_fields[Form.field_start + i];
      const StringRef FieldName(Field.name ? Field.name : "");
      bool Found = false;
      int64_t Value = 0;
      for (const auto &KV : NamedValues) {
        if (KV.first == FieldName) {
          Value = KV.second;
          Found = true;
          break;
        }
      }
      if (!Found) {
        SmallString<96> Msg;
        raw_svector_ostream OS(Msg);
        OS << "Linx: missing value for field '" << FieldName
           << "' in custom MC lowering";
        report_fatal_error(OS.str());
      }
      OutMI.addOperand(MCOperand::createImm(Value));
    }
  };

  switch (Opc) {
  case LinxISA::CBSTART_STD: {
    // Compressed block start marker: `C.BSTART.STD BrType`.
    OutMI.setOpcode(getSpecOpcode("C.BSTART.STD", /*LengthBits=*/16, /*Fields=*/1));
    OutMI.addOperand(MCOperand::createImm(I(0))); // BrType
    return;
  }

  case LinxISA::BSTART_STD_FALL: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.STD FALL<, fixup_label>", /*LengthBits=*/32));
    return;
  }
  case LinxISA::BSTART_STD_DIRECT: {
    // Prefer the compressed form; the assembler can relax to wider forms if
    // the target is out of range.
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("C.BSTART DIRECT, label", /*LengthBits=*/16));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }
  case LinxISA::BSTART_STD_COND: {
    // Prefer the compressed form; the assembler can relax to wider forms if
    // the target is out of range.
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("C.BSTART COND,  label", /*LengthBits=*/16));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }
  case LinxISA::BSTART_STD_CALL: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.STD CALL, <label>", /*LengthBits=*/32));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }
  case LinxISA::BSTART_STD_IND: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.STD IND", /*LengthBits=*/32));
    return;
  }
  case LinxISA::BSTART_STD_ICALL: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.STD ICALL", /*LengthBits=*/32));
    return;
  }
  case LinxISA::BSTART_STD_RET: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.STD RET", /*LengthBits=*/32));
    return;
  }

  case LinxISA::BSTOP: {
    // Block stop marker: prefer the compressed form `C.BSTOP` (16-bit, 0x0000).
    OutMI.setOpcode(getSpecOpcode("C.BSTOP", /*LengthBits=*/16, /*Fields=*/0));
    return;
  }

  case LinxISA::BSTART_TMA: {
    const int64_t DataType = I(0) & 0x1f;
    const unsigned Func = static_cast<unsigned>(I(1)) & 0x1fu;
    static constexpr const char *TMAAsmFormats[] = {
        "BSTART.TLOAD DataType",
        "BSTART.TSTORE DataType",
        "BSTART.TMOV DataType",
        "BSTART.TPREFETCH DataType",
        "BSTART.MGATHER DataType",
        "BSTART.MSCATTER DataType",
        "BSTART.MGATHER.MASK DataType",
        "BSTART.MSCATTER.MASK DataType",
        "BSTART.MGATHER.CAS DataType",
    };
    if (Func >= std::size(TMAAsmFormats))
      report_fatal_error("LinxISA: invalid v0.57 TMA Function");
    OutMI.setOpcode(getSpecOpcodeByAsmFmt(TMAAsmFormats[Func],
                                          /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(DataType));
    return;
  }
  case LinxISA::BSTART_CUBE: {
    const int64_t DataType = I(0) & 0x1f;
    const int64_t Func = I(1) & 0x1f;
    OutMI.setOpcode(
        getSpecOpcodeByAsmFmt("BSTART.CUBE Function, DataType",
                              /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(DataType));
    OutMI.addOperand(MCOperand::createImm(Func));
    return;
  }
  case LinxISA::BSTART_TEPL: {
    const int64_t DataType = I(0) & 0x1f;
    const int64_t TileOpcode = I(1) & 0x3ff;
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("BSTART.TEPL TileOpcode, DataType",
                                          /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(DataType));
    OutMI.addOperand(MCOperand::createImm(TileOpcode));
    return;
  }

  case LinxISA::BSTART_VPAR:
  case LinxISA::BSTART_VSEQ: {
    const StringRef Mnem = (Opc == LinxISA::BSTART_VPAR) ? "BSTART.VPAR"
                                                         : "BSTART.VSEQ";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(MCOperand::createImm(I(0))); // Mode
    return;
  }
  case LinxISA::BSTART_MPAR:
  case LinxISA::BSTART_MSEQ: {
    const StringRef Mnem = (Opc == LinxISA::BSTART_MPAR) ? "BSTART.MPAR"
                                                          : "BSTART.MSEQ";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(MCOperand::createImm(I(0))); // Mode
    return;
  }

  case LinxISA::B_TEXT: {
    OutMI.setOpcode(getSpecOpcode("B.TEXT", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }

  case LinxISA::B_ARG: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("B.ARG format", /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(I(0))); // format
    return;
  }

  case LinxISA::B_CATR: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt(
        "B.CATR {trap, atomic, <aq, rl, aqrl>, far, dr}",
        /*LengthBits=*/32));
    // Canonical catalog field order: DR, aq, atom, far, reserve, rl, trap.
    OutMI.addOperand(MCOperand::createImm(I(1))); // DR
    OutMI.addOperand(MCOperand::createImm(I(2))); // aq
    OutMI.addOperand(MCOperand::createImm(I(3))); // atom
    OutMI.addOperand(MCOperand::createImm(I(4))); // far
    OutMI.addOperand(MCOperand::createImm(0));    // reserve
    OutMI.addOperand(MCOperand::createImm(I(5))); // rl
    OutMI.addOperand(MCOperand::createImm(I(0))); // trap
    return;
  }

  case LinxISA::B_DATR: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt(
        "B.DATR {layout.{canon, normal}, datatype, padvalue, cmode, rmode, sat}",
        /*LengthBits=*/32));
    // Canonical catalog field order: CMode, DataLayout, DataType, PadValue,
    // RMode, Sat.
    for (unsigned Index = 0; Index != 6; ++Index)
      OutMI.addOperand(MCOperand::createImm(I(Index)));
    return;
  }

  case LinxISA::B_DIM_LB0:
  case LinxISA::B_DIM_LB1:
  case LinxISA::B_DIM_LB2: {
    StringRef AsmFmt;
    switch (Opc) {
    case LinxISA::B_DIM_LB0:
      AsmFmt = "B.DIM RegSrc, uimm, ->LB0";
      break;
    case LinxISA::B_DIM_LB1:
      AsmFmt = "B.DIM RegSrc, uimm, ->LB1";
      break;
    case LinxISA::B_DIM_LB2:
      AsmFmt = "B.DIM RegSrc, uimm, ->LB2";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }
    OutMI.setOpcode(getSpecOpcodeByAsmFmt(AsmFmt, /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegSrc
    OutMI.addOperand(MCOperand::createImm(I(1))); // uimm17
    return;
  }

  case LinxISA::C_B_DIMI: {
    OutMI.setOpcode(getSpecOpcodeByAsmFmt("C.B.DIMI imm, ->{LB0, LB1, LB2}",
                                          /*LengthBits=*/16));
    OutMI.addOperand(MCOperand::createImm(I(0))); // LoopNest
    OutMI.addOperand(MCOperand::createImm(I(1))); // imm8
    return;
  }

  case LinxISA::B_IOR: {
    OutMI.setOpcode(
        getSpecOpcodeByAsmFmt("B.IOR [RegSrc0, RegSrc1, RegSrc2],[RegDst]",
                              /*LengthBits=*/32));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // RegSrc0
    OutMI.addOperand(MCOperand::createImm(R(2))); // RegSrc1
    OutMI.addOperand(MCOperand::createImm(R(3))); // RegSrc2
    return;
  }

  case LinxISA::B_IOT_SIZE_G0:
  case LinxISA::B_IOT_SIZE_G1: {
    const bool Last = Opc == LinxISA::B_IOT_SIZE_G1;
    const bool Src0Valid = I(2) == 0;
    const bool Src1Valid = I(4) == 0;
    const bool HasSrc0 = Src0Valid || Src1Valid;
    const bool HasSrc1 = Src0Valid && Src1Valid;
    StringRef AsmFmt = HasSrc1
                           ? "B.IOT SrcTile0<.reuse>, SrcTile1<.reuse>, <last>, ->DstTile<Size>"
                       : HasSrc0
                           ? "B.IOT SrcTile0<.reuse>, <last>, ->DstTile<Size>"
                           : "B.IOT <last>, ->DstTile<Size>";
    OutMI.setOpcode(getSpecOpcodeByAsmFmt(AsmFmt, /*LengthBits=*/32));
    // Emit operands in the selected canonical form's catalog field order.
    OutMI.addOperand(MCOperand::createImm(I(0))); // DstTile
    OutMI.addOperand(MCOperand::createImm(Last)); // L
    if (HasSrc0) {
      const unsigned SrcIndex = Src0Valid ? 5 : 6;
      const unsigned ReuseIndex = Src0Valid ? 1 : 3;
      OutMI.addOperand(MCOperand::createImm(I(ReuseIndex))); // S0R
      if (HasSrc1)
        OutMI.addOperand(MCOperand::createImm(I(3))); // S1R
      OutMI.addOperand(MCOperand::createImm(I(SrcIndex))); // SrcTile0
      if (HasSrc1)
        OutMI.addOperand(MCOperand::createImm(I(6))); // SrcTile1
    }
    OutMI.addOperand(MCOperand::createImm(I(7))); // imm4 (Size)
    return;
  }

  case LinxISA::PSEUDO_V_ADD: {
    emitNamedImmFields(
        getSpecOpcode("V.ADD", /*LengthBits=*/64, /*Fields=*/5),
        {{"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"SrcRType", I(3)},
         {"shamt", I(4)}});
    return;
  }
  case LinxISA::PSEUDO_V_SUB: {
    emitNamedImmFields(
        getSpecOpcode("V.SUB", /*LengthBits=*/64, /*Fields=*/5),
        {{"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"SrcRType", I(3)},
         {"shamt", I(4)}});
    return;
  }
  case LinxISA::PSEUDO_V_MUL: {
    emitNamedImmFields(getSpecOpcode("V.MUL", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FADD: {
    emitNamedImmFields(getSpecOpcode("V.FADD", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FSUB: {
    emitNamedImmFields(getSpecOpcode("V.FSUB", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FMUL: {
    emitNamedImmFields(getSpecOpcode("V.FMUL", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FDIV: {
    emitNamedImmFields(getSpecOpcode("V.FDIV", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FABS: {
    emitNamedImmFields(
        getSpecOpcode("V.FABS", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_FSQRT: {
    emitNamedImmFields(
        getSpecOpcode("V.FSQRT", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_ICVTF: {
    emitNamedImmFields(
        getSpecOpcode("V.ICVTF", /*LengthBits=*/64, /*Fields=*/4),
        {{"DstType", I(2)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcType", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_EQ: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.EQ", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_NE: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.NE", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_LT: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.LT", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_LTU: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.LTU", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_GE: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.GE", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CMP_GEU: {
    emitNamedImmFields(
        getSpecOpcode("V.CMP.GEU", /*LengthBits=*/64, /*Fields=*/3),
        {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FEQ: {
    emitNamedImmFields(getSpecOpcode("V.FEQ", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FNE: {
    emitNamedImmFields(getSpecOpcode("V.FNE", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FLT: {
    emitNamedImmFields(getSpecOpcode("V.FLT", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_FGE: {
    emitNamedImmFields(getSpecOpcode("V.FGE", /*LengthBits=*/64, /*Fields=*/3),
                       {{"RegDst", I(0)}, {"SrcL", I(1)}, {"SrcR", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_CSEL: {
    emitNamedImmFields(
        getSpecOpcode("V.CSEL", /*LengthBits=*/64, /*Fields=*/5),
        {{"RegDst", I(0)},
         {"SrcP", I(1)},
         {"SrcL", I(2)},
         {"SrcR", I(3)},
         {"SrcRType", I(4)}});
    return;
  }
  case LinxISA::PSEUDO_V_PSEL: {
    emitNamedImmFields(
        getSpecOpcode("V.PSEL", /*LengthBits=*/64, /*Fields=*/5),
        {{"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"SrcRType", 3},
         {"SrcZero", 1}});
    return;
  }
  case LinxISA::PSEUDO_V_LB_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.LB.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_LH_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.LH.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_LBU_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.LBU.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_LHU_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.LHU.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_LW_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.LW.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"RegDst", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_LWI_U: {
    const char *Mnem = I(4) ? "V.LWI.U.BRG" : "V.LWI.U";
    emitNamedImmFields(getSpecOpcode(Mnem, /*LengthBits=*/64, /*Fields=*/5),
                       {{"C", 0},
                        {"L", I(3)},
                        {"RegDst", I(0)},
                        {"SrcL", I(1)},
                        {"simm24", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_LDI_U: {
    const char *Mnem = I(4) ? "V.LDI.U.BRG" : "V.LDI.U";
    emitNamedImmFields(getSpecOpcode(Mnem, /*LengthBits=*/64, /*Fields=*/5),
                       {{"C", 0},
                        {"L", I(3)},
                        {"RegDst", I(0)},
                        {"SrcL", I(1)},
                        {"simm24", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_SW_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.SW.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"SrcD", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_SWI_U: {
    const char *Mnem = I(4) ? "V.SWI.U.BRG" : "V.SWI.U";
    emitNamedImmFields(getSpecOpcode(Mnem, /*LengthBits=*/64, /*Fields=*/5),
                       {{"C", 0},
                        {"L", I(3)},
                        {"SrcL", I(0)},
                        {"SrcR", I(1)},
                        {"simm24", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_SDI_U: {
    const char *Mnem = I(4) ? "V.SDI.U.BRG" : "V.SDI.U";
    emitNamedImmFields(getSpecOpcode(Mnem, /*LengthBits=*/64, /*Fields=*/5),
                       {{"C", 0},
                        {"L", I(3)},
                        {"SrcL", I(0)},
                        {"SrcR", I(1)},
                        {"simm24", I(2)}});
    return;
  }
  case LinxISA::PSEUDO_V_SB_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.SB.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"SrcD", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_SH_BRG: {
    emitNamedImmFields(
        getSpecOpcode("V.SH.BRG", /*LengthBits=*/64, /*Fields=*/6),
        {{"C", 0},
         {"L", I(4)},
         {"SrcD", I(0)},
         {"SrcL", I(1)},
         {"SrcR", I(2)},
         {"shamt", I(3)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDADD: {
    emitNamedImmFields(
        getSpecOpcode("V.RDADD", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDAND: {
    emitNamedImmFields(
        getSpecOpcode("V.RDAND", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDFADD: {
    emitNamedImmFields(
        getSpecOpcode("V.RDFADD", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDFMAX: {
    emitNamedImmFields(
        getSpecOpcode("V.RDFMAX", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDFMIN: {
    emitNamedImmFields(
        getSpecOpcode("V.RDFMIN", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDMAX: {
    emitNamedImmFields(
        getSpecOpcode("V.RDMAX", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDMIN: {
    emitNamedImmFields(
        getSpecOpcode("V.RDMIN", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDOR: {
    emitNamedImmFields(
        getSpecOpcode("V.RDOR", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_RDXOR: {
    emitNamedImmFields(
        getSpecOpcode("V.RDXOR", /*LengthBits=*/64, /*Fields=*/2),
        {{"RegDst", I(0)}, {"SrcL", I(1)}});
    return;
  }
  case LinxISA::PSEUDO_V_B_EQ: {
    OutMI.setOpcode(getSpecOpcode("B.EQ", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_NE: {
    OutMI.setOpcode(getSpecOpcode("B.NE", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_LT: {
    OutMI.setOpcode(getSpecOpcode("B.LT", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_GE: {
    OutMI.setOpcode(getSpecOpcode("B.GE", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_LTU: {
    OutMI.setOpcode(getSpecOpcode("B.LTU", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_GEU: {
    OutMI.setOpcode(getSpecOpcode("B.GEU", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_Z: {
    OutMI.setOpcode(getSpecOpcode("B.Z", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0)); // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_B_NZ: {
    OutMI.setOpcode(getSpecOpcode("B.NZ", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0)); // simm12 (pcrel)
    return;
  }
  case LinxISA::PSEUDO_V_C_MOVR: {
    OutMI.setOpcode(getSpecOpcode("C.MOVR", /*LengthBits=*/16, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(I(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(I(1))); // SrcL
    return;
  }
  case LinxISA::PSEUDO_V_J: {
    OutMI.setOpcode(getSpecOpcode("J", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0)); // simm20 (pcrel)
    return;
  }

  case LinxISA::CSETC_EQ:
  case LinxISA::CSETC_NE: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::CSETC_EQ:
      Mnem = "C.SETC.EQ";
      break;
    case LinxISA::CSETC_NE:
      Mnem = "C.SETC.NE";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/16, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcR
    return;
  }

  case LinxISA::CSETC_TGT: {
    OutMI.setOpcode(getSpecOpcode("C.SETC.TGT", /*LengthBits=*/16, /*Fields=*/1));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    return;
  }

  case LinxISA::C_ZEXT_B:
  case LinxISA::C_ZEXT_H:
  case LinxISA::C_ZEXT_W: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::C_ZEXT_B:
      Mnem = "C.ZEXT.B";
      break;
    case LinxISA::C_ZEXT_H:
      Mnem = "C.ZEXT.H";
      break;
    case LinxISA::C_ZEXT_W:
      Mnem = "C.ZEXT.W";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/16, /*Fields=*/1));
    // Encoding only needs SrcL; destination is implicit in the compressed form.
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    return;
  }

  case LinxISA::SETC_EQ:
  case LinxISA::SETC_NE:
  case LinxISA::SETC_LT:
  case LinxISA::SETC_GE:
  case LinxISA::SETC_LTU:
  case LinxISA::SETC_GEU: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SETC_EQ:
      Mnem = "SETC.EQ";
      break;
    case LinxISA::SETC_NE:
      Mnem = "SETC.NE";
      break;
    case LinxISA::SETC_LT:
      Mnem = "SETC.LT";
      break;
    case LinxISA::SETC_GE:
      Mnem = "SETC.GE";
      break;
    case LinxISA::SETC_LTU:
      Mnem = "SETC.LTU";
      break;
    case LinxISA::SETC_GEU:
      Mnem = "SETC.GEU";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcR
    int64_t SrcRType = 3; // default: no modifier
    if (MI->getNumOperands() > 1) {
      const MachineOperand &SrcRMO = MI->getOperand(1);
      if (SrcRMO.isReg()) {
        if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_SW) != 0)
          SrcRType = 0; // .sw
        else if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_UW) != 0)
          SrcRType = 1; // .uw
      }
    }
    OutMI.addOperand(MCOperand::createImm(SrcRType));
    return;
  }

  case LinxISA::SETC_AND:
  case LinxISA::SETC_OR: {
    StringRef Mnem = (Opc == LinxISA::SETC_AND) ? "SETC.AND" : "SETC.OR";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcR
    int64_t SrcRType = 3; // default: no modifier
    if (MI->getNumOperands() > 1) {
      const MachineOperand &SrcRMO = MI->getOperand(1);
      if (SrcRMO.isReg()) {
        if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_SW) != 0)
          SrcRType = 0; // .sw
        else if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_UW) != 0)
          SrcRType = 1; // .uw
      }
    }
    OutMI.addOperand(MCOperand::createImm(SrcRType));
    return;
  }

  case LinxISA::SETC_EQI:
  case LinxISA::SETC_NEI:
  case LinxISA::SETC_LTI:
  case LinxISA::SETC_GEI:
  case LinxISA::SETC_LTUI:
  case LinxISA::SETC_GEUI: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SETC_EQI:
      Mnem = "SETC.EQI";
      break;
    case LinxISA::SETC_NEI:
      Mnem = "SETC.NEI";
      break;
    case LinxISA::SETC_LTI:
      Mnem = "SETC.LTI";
      break;
    case LinxISA::SETC_GEI:
      Mnem = "SETC.GEI";
      break;
    case LinxISA::SETC_LTUI:
      Mnem = "SETC.LTUI";
      break;
    case LinxISA::SETC_GEUI:
      Mnem = "SETC.GEUI";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    unsigned Shamt = 0;
    int64_t BaseImmS = 0;
    uint64_t BaseImmU = 0;
    if (Opc == LinxISA::SETC_LTUI || Opc == LinxISA::SETC_GEUI) {
      if (I(1) < 0)
        report_fatal_error("Linx: SETC.*UI immediate must be non-negative");
      uint64_t Imm = static_cast<uint64_t>(I(1));
      if (!splitShiftedUnsignedImm(Imm, /*BaseBits=*/12, Shamt, BaseImmU))
        report_fatal_error("Linx: SETC.*UI immediate not encodable");
      OutMI.addOperand(MCOperand::createImm(Shamt));
      OutMI.addOperand(MCOperand::createImm(static_cast<int64_t>(BaseImmU)));
      return;
    }

    if (!splitShiftedSignedImm(I(1), /*BaseBits=*/12, Shamt, BaseImmS))
      report_fatal_error("Linx: SETC.*I immediate not encodable");
    OutMI.addOperand(MCOperand::createImm(Shamt));
    OutMI.addOperand(MCOperand::createImm(BaseImmS));
    return;
  }

  case LinxISA::SETC_ANDI:
  case LinxISA::SETC_ORI: {
    StringRef Mnem = (Opc == LinxISA::SETC_ANDI) ? "SETC.ANDI" : "SETC.ORI";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    unsigned Shamt = 0;
    int64_t BaseImm = 0;
    if (!splitShiftedSignedImm(I(1), /*BaseBits=*/12, Shamt, BaseImm))
      report_fatal_error("Linx: SETC.ANDI/ORI immediate not encodable");
    OutMI.addOperand(MCOperand::createImm(Shamt));
    OutMI.addOperand(MCOperand::createImm(BaseImm));
    return;
  }

  case LinxISA::HLSETC_ANDI:
  case LinxISA::HLSETC_ORI: {
    StringRef Mnem = (Opc == LinxISA::HLSETC_ANDI) ? "HL.SETC.ANDI" : "HL.SETC.ORI";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    // Keep the same operand order as the 32-bit SETC.*I forms: SrcL, shamt, simm.
    unsigned Shamt = 0;
    int64_t BaseImm = 0;
    if (!splitShiftedSignedImm(I(1), /*BaseBits=*/24, Shamt, BaseImm))
      report_fatal_error("Linx: HL.SETC.ANDI/ORI immediate not encodable");
    OutMI.addOperand(MCOperand::createImm(Shamt));
    OutMI.addOperand(MCOperand::createImm(BaseImm));
    return;
  }

  case LinxISA::SETC_TGT: {
    OutMI.setOpcode(getSpecOpcode("SETC.TGT", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    return;
  }

  case LinxISA::SETRET: {
    // Prefer the compressed form; the assembler can relax to wider forms if
    // the target is out of range.
    OutMI.setOpcode(getSpecOpcode("C.SETRET", /*LengthBits=*/16, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }

  case LinxISA::C_SEXT_B:
  case LinxISA::C_SEXT_H:
  case LinxISA::C_SEXT_W: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::C_SEXT_B:
      Mnem = "C.SEXT.B";
      break;
    case LinxISA::C_SEXT_H:
      Mnem = "C.SEXT.H";
      break;
    case LinxISA::C_SEXT_W:
      Mnem = "C.SEXT.W";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/16, /*Fields=*/1));
    // Encoding only needs SrcL; destination is implicit in the compressed form.
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    return;
  }

  case LinxISA::MADD:
  case LinxISA::MADDW: {
    StringRef Mnem = (Opc == LinxISA::MADD) ? "MADD" : "MADDW";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    // Field order for the spec form is: RegDst, SrcD, SrcL, SrcR.
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(3))); // SrcD (addend)
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    return;
  }

  case TargetOpcode::COPY: {
    // Prefer the compressed form.
    OutMI.setOpcode(getSpecOpcode("C.MOVR", /*LengthBits=*/16, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    return;
  }

  case LinxISA::ADDrr_SH:
  case LinxISA::SUBrr_SH:
  case LinxISA::ANDrr_SH:
  case LinxISA::ORrr_SH:
  case LinxISA::XORrr_SH:
  case LinxISA::ADDWrr_SH:
  case LinxISA::SUBWrr_SH:
  case LinxISA::ANDWrr_SH:
  case LinxISA::ORWrr_SH:
  case LinxISA::XORWrr_SH: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::ADDrr_SH:
      Mnem = "ADD";
      break;
    case LinxISA::SUBrr_SH:
      Mnem = "SUB";
      break;
    case LinxISA::ANDrr_SH:
      Mnem = "AND";
      break;
    case LinxISA::ORrr_SH:
      Mnem = "OR";
      break;
    case LinxISA::XORrr_SH:
      Mnem = "XOR";
      break;
    case LinxISA::ADDWrr_SH:
      Mnem = "ADDW";
      break;
    case LinxISA::SUBWrr_SH:
      Mnem = "SUBW";
      break;
    case LinxISA::ANDWrr_SH:
      Mnem = "ANDW";
      break;
    case LinxISA::ORWrr_SH:
      Mnem = "ORW";
      break;
    case LinxISA::XORWrr_SH:
      Mnem = "XORW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/5));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    OutMI.addOperand(MCOperand::createImm(I(3))); // shamt
    return;
  }

  case LinxISA::ADDrr:
  case LinxISA::SUBrr:
  case LinxISA::ANDrr:
  case LinxISA::ORrr:
  case LinxISA::XORrr:
  case LinxISA::ADDWrr:
  case LinxISA::SUBWrr:
  case LinxISA::ANDWrr:
  case LinxISA::ORWrr:
  case LinxISA::XORWrr: {
    // Prefer compressed arithmetic when the destination is the T-hand output.
    const Register DstReg = MI->getOperand(0).getReg();
    if (DstReg == LinxISA::U4) {
      StringRef CMnem;
      switch (Opc) {
      case LinxISA::ADDrr:
        CMnem = "C.ADD";
        break;
      case LinxISA::SUBrr:
        CMnem = "C.SUB";
        break;
      case LinxISA::ANDrr:
        CMnem = "C.AND";
        break;
      case LinxISA::ORrr:
        CMnem = "C.OR";
        break;
      default:
        break;
      }

      if (!CMnem.empty()) {
        OutMI.setOpcode(getSpecOpcode(CMnem, /*LengthBits=*/16, /*Fields=*/2));
        OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
        OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
        return;
      }
    }

    StringRef Mnem;
    switch (Opc) {
    case LinxISA::ADDrr:
      Mnem = "ADD";
      break;
    case LinxISA::SUBrr:
      Mnem = "SUB";
      break;
    case LinxISA::ANDrr:
      Mnem = "AND";
      break;
    case LinxISA::ORrr:
      Mnem = "OR";
      break;
    case LinxISA::XORrr:
      Mnem = "XOR";
      break;
    case LinxISA::ADDWrr:
      Mnem = "ADDW";
      break;
    case LinxISA::SUBWrr:
      Mnem = "SUBW";
      break;
    case LinxISA::ANDWrr:
      Mnem = "ANDW";
      break;
    case LinxISA::ORWrr:
      Mnem = "ORW";
      break;
    case LinxISA::XORWrr:
      Mnem = "XORW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/5));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    OutMI.addOperand(MCOperand::createImm(0));    // shamt
    return;
  }

  case LinxISA::ADDIri:
  case LinxISA::SUBIri:
  case LinxISA::ANDIri:
  case LinxISA::ORIri:
  case LinxISA::XORIri:
  case LinxISA::ADDIWri:
  case LinxISA::SUBIWri:
  case LinxISA::ANDIWri:
  case LinxISA::ORIWri:
  case LinxISA::XORIWri: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::ADDIri:
      Mnem = "ADDI";
      break;
    case LinxISA::SUBIri:
      Mnem = "SUBI";
      break;
    case LinxISA::ANDIri:
      Mnem = "ANDI";
      break;
    case LinxISA::ORIri:
      Mnem = "ORI";
      break;
    case LinxISA::XORIri:
      Mnem = "XORI";
      break;
    case LinxISA::ADDIWri:
      Mnem = "ADDIW";
      break;
    case LinxISA::SUBIWri:
      Mnem = "SUBIW";
      break;
    case LinxISA::ANDIWri:
      Mnem = "ANDIW";
      break;
    case LinxISA::ORIWri:
      Mnem = "ORIW";
      break;
    case LinxISA::XORIWri:
      Mnem = "XORIW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    const MachineOperand &Op2 = MI->getOperand(2);
    if (Op2.isImm()) {
      const Register DstReg = MI->getOperand(0).getReg();
      const Register SrcReg = MI->getOperand(1).getReg();
      const int64_t Imm = I(2);

      // Prefer C.MOVR for common reg copies.
      if ((Opc == LinxISA::ADDIri || Opc == LinxISA::SUBIri ||
           Opc == LinxISA::ADDIWri || Opc == LinxISA::SUBIWri) &&
          Imm == 0) {
        OutMI.setOpcode(getSpecOpcode("C.MOVR", /*LengthBits=*/16, /*Fields=*/2));
        OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
        OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
        return;
      }

      // Prefer C.MOVI for small immediates materialized from zero.
      if ((Opc == LinxISA::ADDIri || Opc == LinxISA::SUBIri ||
           Opc == LinxISA::ADDIWri || Opc == LinxISA::SUBIWri) &&
          SrcReg == LinxISA::R0 && DstReg != LinxISA::R10) {
        int64_t SImm = (Opc == LinxISA::SUBIri || Opc == LinxISA::SUBIWri)
                           ? -Imm
                           : Imm;
        if (isInt<5>(SImm)) {
          OutMI.setOpcode(getSpecOpcode("C.MOVI", /*LengthBits=*/16, /*Fields=*/2));
          OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
          OutMI.addOperand(MCOperand::createImm(SImm)); // simm5
          return;
        }
      }

      // Prefer C.ADDI when writing the T-hand output.
      if ((Opc == LinxISA::ADDIri || Opc == LinxISA::SUBIri) &&
          DstReg == LinxISA::U4) {
        int64_t SImm = (Opc == LinxISA::SUBIri) ? -Imm : Imm;
        if (isInt<5>(SImm)) {
          OutMI.setOpcode(getSpecOpcode("C.ADDI", /*LengthBits=*/16, /*Fields=*/2));
          OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
          OutMI.addOperand(MCOperand::createImm(SImm)); // simm5
          return;
        }
      }

      OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
      OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
      OutMI.addOperand(MCOperand::createImm(I(2))); // imm12
      return;
    }

    // Allow symbolic immediates for ADDI/ADDIW (used by the global address
    // materialization sequence: ADDTPC + ADDI/ADDIW with LO12 relocation).
    if (!Op2.isReg()) {
      if (Opc == LinxISA::ADDIri || Opc == LinxISA::ADDIWri) {
        OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
        OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
        OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL

        MCOperand ExprOp;
        if (!lowerOperand(Op2, ExprOp)) {
          MI->print(errs());
          report_fatal_error("Linx: failed to lower ADDI/ADDIW immediate operand");
        }
        OutMI.addOperand(ExprOp);
        return;
      }

      MI->print(errs());
      report_fatal_error("Linx: expected imm/reg operand for *I instruction");
    }

    // If a constant got materialized into a register late, fall back to the
    // corresponding reg-reg instruction.
    StringRef RegMnem;
    switch (Opc) {
    case LinxISA::ADDIri:
      RegMnem = "ADD";
      break;
    case LinxISA::SUBIri:
      RegMnem = "SUB";
      break;
    case LinxISA::ANDIri:
      RegMnem = "AND";
      break;
    case LinxISA::ORIri:
      RegMnem = "OR";
      break;
    case LinxISA::XORIri:
      RegMnem = "XOR";
      break;
    case LinxISA::ADDIWri:
      RegMnem = "ADDW";
      break;
    case LinxISA::SUBIWri:
      RegMnem = "SUBW";
      break;
    case LinxISA::ANDIWri:
      RegMnem = "ANDW";
      break;
    case LinxISA::ORIWri:
      RegMnem = "ORW";
      break;
    case LinxISA::XORIWri:
      RegMnem = "XORW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(RegMnem, /*LengthBits=*/32, /*Fields=*/5));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    OutMI.addOperand(MCOperand::createImm(0));    // shamt
    return;
  }

  case LinxISA::SLLrr:
  case LinxISA::SRLrr:
  case LinxISA::SRArr:
  case LinxISA::SLLWrr:
  case LinxISA::SRLWrr:
  case LinxISA::SRAWrr:
  case LinxISA::MULrr:
  case LinxISA::DIVrr:
  case LinxISA::DIVUrr:
  case LinxISA::REMrr:
  case LinxISA::REMUrr:
  case LinxISA::MULWrr:
  case LinxISA::DIVWrr:
  case LinxISA::DIVUWrr:
  case LinxISA::REMWrr:
  case LinxISA::REMUWrr: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SLLrr:
      Mnem = "SLL";
      break;
    case LinxISA::SRLrr:
      Mnem = "SRL";
      break;
    case LinxISA::SRArr:
      Mnem = "SRA";
      break;
    case LinxISA::SLLWrr:
      Mnem = "SLLW";
      break;
    case LinxISA::SRLWrr:
      Mnem = "SRLW";
      break;
    case LinxISA::SRAWrr:
      Mnem = "SRAW";
      break;
    case LinxISA::MULrr:
      Mnem = "MUL";
      break;
    case LinxISA::DIVrr:
      Mnem = "DIV";
      break;
    case LinxISA::DIVUrr:
      Mnem = "DIVU";
      break;
    case LinxISA::REMrr:
      Mnem = "REM";
      break;
    case LinxISA::REMUrr:
      Mnem = "REMU";
      break;
    case LinxISA::MULWrr:
      Mnem = "MULW";
      break;
    case LinxISA::DIVWrr:
      Mnem = "DIVW";
      break;
    case LinxISA::DIVUWrr:
      Mnem = "DIVUW";
      break;
    case LinxISA::REMWrr:
      Mnem = "REMW";
      break;
    case LinxISA::REMUWrr:
      Mnem = "REMUW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    return;
  }

  case LinxISA::FADDrr:
  case LinxISA::FSUBrr:
  case LinxISA::FMULrr:
  case LinxISA::FDIVrr: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::FADDrr:
      Mnem = "FADD";
      break;
    case LinxISA::FSUBrr:
      Mnem = "FSUB";
      break;
    case LinxISA::FMULrr:
      Mnem = "FMUL";
      break;
    case LinxISA::FDIVrr:
      Mnem = "FDIV";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    OutMI.addOperand(MCOperand::createImm(I(3))); // SrcType
    return;
  }

  case LinxISA::FABSrr: {
    OutMI.setOpcode(getSpecOpcode("FABS", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(2))); // SrcType
    return;
  }

  case LinxISA::FEQrr:
  case LinxISA::FLTrr:
  case LinxISA::FGErr: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::FEQrr:
      Mnem = "FEQ";
      break;
    case LinxISA::FLTrr:
      Mnem = "FLT";
      break;
    case LinxISA::FGErr:
      Mnem = "FGE";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    OutMI.addOperand(MCOperand::createImm(I(3))); // SrcType
    return;
  }

  case LinxISA::FCVT:
  case LinxISA::FCVTZ:
  case LinxISA::SCVTF:
  case LinxISA::UCVTF: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::FCVT:
      Mnem = "FCVT";
      break;
    case LinxISA::FCVTZ:
      Mnem = "FCVTZ";
      break;
    case LinxISA::SCVTF:
      Mnem = "SCVTF";
      break;
    case LinxISA::UCVTF:
      Mnem = "UCVTF";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(I(2))); // DstType
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(3))); // SrcType
    return;
  }

  case LinxISA::SLLIri:
  case LinxISA::SRLIri:
  case LinxISA::SRAIri:
  case LinxISA::SLLIWri:
  case LinxISA::SRLIWri:
  case LinxISA::SRAIWri: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SLLIri:
      Mnem = "SLLI";
      break;
    case LinxISA::SRLIri:
      Mnem = "SRLI";
      break;
    case LinxISA::SRAIri:
      Mnem = "SRAI";
      break;
    case LinxISA::SLLIWri:
      Mnem = "SLLIW";
      break;
    case LinxISA::SRLIWri:
      Mnem = "SRLIW";
      break;
    case LinxISA::SRAIWri:
      Mnem = "SRAIW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    const MachineOperand &Op2 = MI->getOperand(2);
    if (Op2.isImm()) {
      const Register DstReg = MI->getOperand(0).getReg();
      const Register SrcReg = MI->getOperand(1).getReg();
      const int64_t Imm = I(2);
      if (linxEnableCShift16() && DstReg == LinxISA::U4 &&
          SrcReg == LinxISA::T1 && isUInt<5>(Imm) &&
          (Opc == LinxISA::SLLIri || Opc == LinxISA::SRLIri)) {
        const StringRef Cmnem =
            (Opc == LinxISA::SLLIri) ? "C.SLLI" : "C.SRLI";
        OutMI.setOpcode(getSpecOpcode(Cmnem, /*LengthBits=*/16, /*Fields=*/1));
        OutMI.addOperand(MCOperand::createImm(Imm)); // uimm5
        return;
      }

      OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
      OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
      OutMI.addOperand(MCOperand::createImm(I(2))); // shamt
      return;
    }

    if (!Op2.isReg()) {
      MI->print(errs());
      report_fatal_error("Linx: expected imm/reg operand for *I shift instruction");
    }

    // If the shift amount got materialized into a register, fall back to the
    // corresponding reg-reg shift instruction.
    StringRef RegMnem;
    switch (Opc) {
    case LinxISA::SLLIri:
      RegMnem = "SLL";
      break;
    case LinxISA::SRLIri:
      RegMnem = "SRL";
      break;
    case LinxISA::SRAIri:
      RegMnem = "SRA";
      break;
    case LinxISA::SLLIWri:
      RegMnem = "SLLW";
      break;
    case LinxISA::SRLIWri:
      RegMnem = "SRLW";
      break;
    case LinxISA::SRAIWri:
      RegMnem = "SRAW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(RegMnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    return;
  }

  case LinxISA::HLADDIri:
  case LinxISA::HLSUBIri:
  case LinxISA::HLANDIri:
  case LinxISA::HLORIri:
  case LinxISA::HLXORIri:
  case LinxISA::HLADDIWri:
  case LinxISA::HLSUBIWri:
  case LinxISA::HLANDIWri:
  case LinxISA::HLORIWri:
  case LinxISA::HLXORIWri: {
    const MachineOperand &Op2 = MI->getOperand(2);
    if (!Op2.isImm()) {
      if (!Op2.isReg()) {
        MI->print(errs());
        report_fatal_error("Linx: expected imm/reg operand for HL.*I instruction");
      }

      // If the immediate got materialized into a register, fall back to the
      // corresponding reg-reg instruction.
      StringRef RegMnem;
      switch (Opc) {
      case LinxISA::HLADDIri:
        RegMnem = "ADD";
        break;
      case LinxISA::HLSUBIri:
        RegMnem = "SUB";
        break;
      case LinxISA::HLANDIri:
        RegMnem = "AND";
        break;
      case LinxISA::HLORIri:
        RegMnem = "OR";
        break;
      case LinxISA::HLXORIri:
        RegMnem = "XOR";
        break;
      case LinxISA::HLADDIWri:
        RegMnem = "ADDW";
        break;
      case LinxISA::HLSUBIWri:
        RegMnem = "SUBW";
        break;
      case LinxISA::HLANDIWri:
        RegMnem = "ANDW";
        break;
      case LinxISA::HLORIWri:
        RegMnem = "ORW";
        break;
      case LinxISA::HLXORIWri:
        RegMnem = "XORW";
        break;
      default:
        llvm_unreachable("Unexpected opcode");
      }

      OutMI.setOpcode(getSpecOpcode(RegMnem, /*LengthBits=*/32, /*Fields=*/5));
      OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
      OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
      OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
      OutMI.addOperand(MCOperand::createImm(0));    // shamt
      return;
    }

    StringRef Mnem;
    switch (Opc) {
    case LinxISA::HLADDIri:
      Mnem = "HL.ADDI";
      break;
    case LinxISA::HLSUBIri:
      Mnem = "HL.SUBI";
      break;
    case LinxISA::HLANDIri:
      Mnem = "HL.ANDI";
      break;
    case LinxISA::HLORIri:
      Mnem = "HL.ORI";
      break;
    case LinxISA::HLXORIri:
      Mnem = "HL.XORI";
      break;
    case LinxISA::HLADDIWri:
      Mnem = "HL.ADDIW";
      break;
    case LinxISA::HLSUBIWri:
      Mnem = "HL.SUBIW";
      break;
    case LinxISA::HLANDIWri:
      Mnem = "HL.ANDIW";
      break;
    case LinxISA::HLORIWri:
      Mnem = "HL.ORIW";
      break;
    case LinxISA::HLXORIWri:
      Mnem = "HL.XORIW";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(2))); // uimm24/simm24
    return;
  }

  case LinxISA::CLZ:
  case LinxISA::CTZ:
  case LinxISA::BCNT: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::CLZ:
      Mnem = "CLZ";
      break;
    case LinxISA::CTZ:
      Mnem = "CTZ";
      break;
    case LinxISA::BCNT:
      Mnem = "BCNT";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(2))); // imml
    OutMI.addOperand(MCOperand::createImm(I(3))); // imms
    return;
  }

  case LinxISA::BXS:
  case LinxISA::BXU: {
    const StringRef Mnem = (Opc == LinxISA::BXS) ? "BXS" : "BXU";
    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(2))); // imml
    OutMI.addOperand(MCOperand::createImm(I(3))); // imms
    return;
  }

  case LinxISA::LUI: {
    OutMI.setOpcode(getSpecOpcode("LUI", /*LengthBits=*/32, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(I(1))); // imm20
    return;
  }

  case LinxISA::HLLUI: {
    OutMI.setOpcode(getSpecOpcode("HL.LUI", /*LengthBits=*/48, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(I(1))); // imm
    return;
  }

  case LinxISA::ADDTPC: {
    // ADDTPC is used for PC-relative addressing of global symbols.
    // Format: ADDTPC rd, imm20  (rd = PC + sext(imm20))
    OutMI.setOpcode(getSpecOpcode("ADDTPC", /*LengthBits=*/32, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst

    // The second operand can be an immediate or a global address expression
    const MachineOperand &MO = MI->getOperand(1);
    MCOperand Op;
    if (lowerOperand(MO, Op)) {
      OutMI.addOperand(Op);
    } else {
      report_fatal_error("Linx ADDTPC: failed to lower operand");
    }
    return;
  }

  case LinxISA::LBI:
  case LinxISA::LBUI:
  case LinxISA::LHI:
  case LinxISA::LHUI:
  case LinxISA::LWI:
  case LinxISA::LWUI:
  case LinxISA::LDI: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::LBI:
      Mnem = "LBI";
      break;
    case LinxISA::LBUI:
      Mnem = "LBUI";
      break;
    case LinxISA::LHI:
      Mnem = "LHI";
      break;
    case LinxISA::LHUI:
      Mnem = "LHUI";
      break;
    case LinxISA::LWI:
      Mnem = "LWI";
      break;
    case LinxISA::LWUI:
      Mnem = "LWUI";
      break;
    case LinxISA::LDI:
      Mnem = "LDI";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    const Register DstReg = MI->getOperand(0).getReg();
    if (DstReg == LinxISA::U4) {
      if (Opc == LinxISA::LWI && isInt<5>(I(2))) {
        OutMI.setOpcode(getSpecOpcode("C.LWI", /*LengthBits=*/16, /*Fields=*/2));
        OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
        OutMI.addOperand(MCOperand::createImm(I(2))); // simm5 (scaled)
        return;
      }
      if (Opc == LinxISA::LDI && isInt<5>(I(2))) {
        OutMI.setOpcode(getSpecOpcode("C.LDI", /*LengthBits=*/16, /*Fields=*/2));
        OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
        OutMI.addOperand(MCOperand::createImm(I(2))); // simm5 (scaled)
        return;
      }
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
    OutMI.addOperand(MCOperand::createImm(I(2))); // simm12 (scaled)
    return;
  }

  case LinxISA::LB:
  case LinxISA::LBU:
  case LinxISA::LH:
  case LinxISA::LHU:
  case LinxISA::LW:
  case LinxISA::LWU:
  case LinxISA::LD: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::LB:
      Mnem = "LB";
      break;
    case LinxISA::LBU:
      Mnem = "LBU";
      break;
    case LinxISA::LH:
      Mnem = "LH";
      break;
    case LinxISA::LHU:
      Mnem = "LHU";
      break;
    case LinxISA::LW:
      Mnem = "LW";
      break;
    case LinxISA::LWU:
      Mnem = "LWU";
      break;
    case LinxISA::LD:
      Mnem = "LD";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/5));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR (index)
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    OutMI.addOperand(MCOperand::createImm(I(3))); // shamt
    return;
  }

  case LinxISA::LB_PCR:
  case LinxISA::LBU_PCR:
  case LinxISA::LH_PCR:
  case LinxISA::LHU_PCR:
  case LinxISA::LW_PCR:
  case LinxISA::LWU_PCR:
  case LinxISA::LD_PCR: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::LB_PCR:
      Mnem = "LB.PCR";
      break;
    case LinxISA::LBU_PCR:
      Mnem = "LBU.PCR";
      break;
    case LinxISA::LH_PCR:
      Mnem = "LH.PCR";
      break;
    case LinxISA::LHU_PCR:
      Mnem = "LHU.PCR";
      break;
    case LinxISA::LW_PCR:
      Mnem = "LW.PCR";
      break;
    case LinxISA::LWU_PCR:
      Mnem = "LWU.PCR";
      break;
    case LinxISA::LD_PCR:
      Mnem = "LD.PCR";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    MCOperand Op;
    if (lowerOperand(MI->getOperand(1), Op)) {
      OutMI.addOperand(Op);
    } else {
      report_fatal_error("Linx *.PCR load: failed to lower symbol operand");
    }
    return;
  }

  case LinxISA::HL_LB_PCR:
  case LinxISA::HL_LBU_PCR:
  case LinxISA::HL_LH_PCR:
  case LinxISA::HL_LHU_PCR:
  case LinxISA::HL_LW_PCR:
  case LinxISA::HL_LWU_PCR:
  case LinxISA::HL_LD_PCR: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::HL_LB_PCR:
      Mnem = "HL.LB.PCR";
      break;
    case LinxISA::HL_LBU_PCR:
      Mnem = "HL.LBU.PCR";
      break;
    case LinxISA::HL_LH_PCR:
      Mnem = "HL.LH.PCR";
      break;
    case LinxISA::HL_LHU_PCR:
      Mnem = "HL.LHU.PCR";
      break;
    case LinxISA::HL_LW_PCR:
      Mnem = "HL.LW.PCR";
      break;
    case LinxISA::HL_LWU_PCR:
      Mnem = "HL.LWU.PCR";
      break;
    case LinxISA::HL_LD_PCR:
      Mnem = "HL.LD.PCR";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    MCOperand Op;
    if (lowerOperand(MI->getOperand(1), Op)) {
      OutMI.addOperand(Op);
    } else {
      report_fatal_error("Linx HL.*.PCR load: failed to lower symbol operand");
    }
    return;
  }

  case LinxISA::HL_LWI_PO:
  case LinxISA::HL_LWI_PR:
  case LinxISA::HL_LWI_UPO:
  case LinxISA::HL_LWI_UPR:
  case LinxISA::HL_LWUI_PO:
  case LinxISA::HL_LWUI_PR:
  case LinxISA::HL_LWUI_UPO:
  case LinxISA::HL_LWUI_UPR:
  case LinxISA::HL_LDI_PO:
  case LinxISA::HL_LDI_PR:
  case LinxISA::HL_LDI_UPO:
  case LinxISA::HL_LDI_UPR:
  case LinxISA::HL_LWIP:
  case LinxISA::HL_LWIP_U:
  case LinxISA::HL_LWUIP:
  case LinxISA::HL_LWUIP_U:
  case LinxISA::HL_LDIP:
  case LinxISA::HL_LDIP_U: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::HL_LWI_PO:
      Mnem = "HL.LWI.PO";
      break;
    case LinxISA::HL_LWI_PR:
      Mnem = "HL.LWI.PR";
      break;
    case LinxISA::HL_LWI_UPO:
      Mnem = "HL.LWI.UPO";
      break;
    case LinxISA::HL_LWI_UPR:
      Mnem = "HL.LWI.UPR";
      break;
    case LinxISA::HL_LWUI_PO:
      Mnem = "HL.LWUI.PO";
      break;
    case LinxISA::HL_LWUI_PR:
      Mnem = "HL.LWUI.PR";
      break;
    case LinxISA::HL_LWUI_UPO:
      Mnem = "HL.LWUI.UPO";
      break;
    case LinxISA::HL_LWUI_UPR:
      Mnem = "HL.LWUI.UPR";
      break;
    case LinxISA::HL_LDI_PO:
      Mnem = "HL.LDI.PO";
      break;
    case LinxISA::HL_LDI_PR:
      Mnem = "HL.LDI.PR";
      break;
    case LinxISA::HL_LDI_UPO:
      Mnem = "HL.LDI.UPO";
      break;
    case LinxISA::HL_LDI_UPR:
      Mnem = "HL.LDI.UPR";
      break;
    case LinxISA::HL_LWIP:
      Mnem = "HL.LWIP";
      break;
    case LinxISA::HL_LWIP_U:
      Mnem = "HL.LWIP.U";
      break;
    case LinxISA::HL_LWUIP:
      Mnem = "HL.LWUIP";
      break;
    case LinxISA::HL_LWUIP_U:
      Mnem = "HL.LWUIP.U";
      break;
    case LinxISA::HL_LDIP:
      Mnem = "HL.LDIP";
      break;
    case LinxISA::HL_LDIP_U:
      Mnem = "HL.LDIP.U";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst0
    OutMI.addOperand(MCOperand::createImm(R(1))); // RegDst1
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcL (base)
    OutMI.addOperand(MCOperand::createImm(I(3))); // simm17
    return;
  }

  case LinxISA::SBI:
  case LinxISA::SHI:
  case LinxISA::SWI:
  case LinxISA::SDI: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SBI:
      Mnem = "SBI";
      break;
    case LinxISA::SHI:
      Mnem = "SHI";
      break;
    case LinxISA::SWI:
      Mnem = "SWI";
      break;
    case LinxISA::SDI:
      Mnem = "SDI";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    // Prefer compressed stores when storing the most recent T-hand value.
    // C.SWI/C.SDI implicitly store t#1.
    if ((Opc == LinxISA::SWI || Opc == LinxISA::SDI) &&
        MI->getOperand(0).isReg() && MI->getOperand(0).getReg() == LinxISA::T1 &&
        isInt<5>(I(2))) {
      OutMI.setOpcode(getSpecOpcode(Opc == LinxISA::SWI ? "C.SWI" : "C.SDI",
                                    /*LengthBits=*/16, /*Fields=*/2));
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
      OutMI.addOperand(MCOperand::createImm(I(2))); // simm5 (scaled)
      return;
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL (value)
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcR (base)
    OutMI.addOperand(MCOperand::createImm(I(2))); // simm12 (scaled)
    return;
  }

  case LinxISA::HL_SWI_PO:
  case LinxISA::HL_SWI_PR:
  case LinxISA::HL_SWI_UPO:
  case LinxISA::HL_SWI_UPR:
  case LinxISA::HL_SDI_PO:
  case LinxISA::HL_SDI_PR:
  case LinxISA::HL_SDI_UPO:
  case LinxISA::HL_SDI_UPR:
  case LinxISA::HL_SWIP:
  case LinxISA::HL_SWIP_U:
  case LinxISA::HL_SDIP:
  case LinxISA::HL_SDIP_U: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::HL_SWI_PO:
      Mnem = "HL.SWI.PO";
      break;
    case LinxISA::HL_SWI_PR:
      Mnem = "HL.SWI.PR";
      break;
    case LinxISA::HL_SWI_UPO:
      Mnem = "HL.SWI.UPO";
      break;
    case LinxISA::HL_SWI_UPR:
      Mnem = "HL.SWI.UPR";
      break;
    case LinxISA::HL_SDI_PO:
      Mnem = "HL.SDI.PO";
      break;
    case LinxISA::HL_SDI_PR:
      Mnem = "HL.SDI.PR";
      break;
    case LinxISA::HL_SDI_UPO:
      Mnem = "HL.SDI.UPO";
      break;
    case LinxISA::HL_SDI_UPR:
      Mnem = "HL.SDI.UPR";
      break;
    case LinxISA::HL_SWIP:
      Mnem = "HL.SWIP";
      break;
    case LinxISA::HL_SWIP_U:
      Mnem = "HL.SWIP.U";
      break;
    case LinxISA::HL_SDIP:
      Mnem = "HL.SDIP";
      break;
    case LinxISA::HL_SDIP_U:
      Mnem = "HL.SDIP.U";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/4));

    if (Opc == LinxISA::HL_SWIP || Opc == LinxISA::HL_SWIP_U ||
        Opc == LinxISA::HL_SDIP || Opc == LinxISA::HL_SDIP_U) {
      OutMI.addOperand(MCOperand::createImm(R(0))); // SrcD
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcD1
      OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR (base)
      OutMI.addOperand(MCOperand::createImm(I(3))); // simm17
    } else {
      OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst (writeback)
      OutMI.addOperand(MCOperand::createImm(R(1))); // SrcD (value)
      OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR (base)
      OutMI.addOperand(MCOperand::createImm(I(3))); // simm17
    }
    return;
  }

  case LinxISA::SB_PCR:
  case LinxISA::SH_PCR:
  case LinxISA::SW_PCR:
  case LinxISA::SD_PCR: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SB_PCR:
      Mnem = "SB.PCR";
      break;
    case LinxISA::SH_PCR:
      Mnem = "SH.PCR";
      break;
    case LinxISA::SW_PCR:
      Mnem = "SW.PCR";
      break;
    case LinxISA::SD_PCR:
      Mnem = "SD.PCR";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    MCOperand Op;
    if (lowerOperand(MI->getOperand(1), Op)) {
      OutMI.addOperand(Op);
    } else {
      report_fatal_error("Linx *.PCR store: failed to lower symbol operand");
    }
    return;
  }

  case LinxISA::HL_SB_PCR:
  case LinxISA::HL_SH_PCR:
  case LinxISA::HL_SW_PCR:
  case LinxISA::HL_SD_PCR: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::HL_SB_PCR:
      Mnem = "HL.SB.PCR";
      break;
    case LinxISA::HL_SH_PCR:
      Mnem = "HL.SH.PCR";
      break;
    case LinxISA::HL_SW_PCR:
      Mnem = "HL.SW.PCR";
      break;
    case LinxISA::HL_SD_PCR:
      Mnem = "HL.SD.PCR";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/48, /*Fields=*/2));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    MCOperand Op;
    if (lowerOperand(MI->getOperand(1), Op)) {
      OutMI.addOperand(Op);
    } else {
      report_fatal_error("Linx HL.*.PCR store: failed to lower symbol operand");
    }
    return;
  }

  case LinxISA::SB:
  case LinxISA::SH:
  case LinxISA::SW:
  case LinxISA::SD: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::SB:
      Mnem = "SB";
      break;
    case LinxISA::SH:
      Mnem = "SH";
      break;
    case LinxISA::SW:
      Mnem = "SW";
      break;
    case LinxISA::SD:
      Mnem = "SD";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcD (value)
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL (base)
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR (index)
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    return;
  }

  case LinxISA::CMPEQ:
  case LinxISA::CMPNE:
  case LinxISA::CMPLT:
  case LinxISA::CMPGE:
  case LinxISA::CMPLTU:
  case LinxISA::CMPGEU:
  case LinxISA::CMPAND:
  case LinxISA::CMPOR: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::CMPEQ:
      Mnem = "CMP.EQ";
      break;
    case LinxISA::CMPNE:
      Mnem = "CMP.NE";
      break;
    case LinxISA::CMPLT:
      Mnem = "CMP.LT";
      break;
    case LinxISA::CMPGE:
      Mnem = "CMP.GE";
      break;
    case LinxISA::CMPLTU:
      Mnem = "CMP.LTU";
      break;
    case LinxISA::CMPGEU:
      Mnem = "CMP.GEU";
      break;
    case LinxISA::CMPAND:
      Mnem = "CMP.AND";
      break;
    case LinxISA::CMPOR:
      Mnem = "CMP.OR";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/4));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcR
    int64_t SrcRType = 3; // default: no modifier
    if (MI->getNumOperands() > 2) {
      const MachineOperand &SrcRMO = MI->getOperand(2);
      if (SrcRMO.isReg()) {
        if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_SW) != 0)
          SrcRType = 0; // .sw
        else if ((SrcRMO.getTargetFlags() & LinxII::MO_SRCR_UW) != 0)
          SrcRType = 1; // .uw
      }
    }
    OutMI.addOperand(MCOperand::createImm(SrcRType));
    return;
  }

  case LinxISA::CMPEQI:
  case LinxISA::CMPNEI:
  case LinxISA::CMPLTI:
  case LinxISA::CMPGEI:
  case LinxISA::CMPLTUI:
  case LinxISA::CMPGEUI:
  case LinxISA::CMPANDI:
  case LinxISA::CMPORI:
  case LinxISA::HLCMPANDI:
  case LinxISA::HLCMPORI: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::CMPEQI:
      Mnem = "CMP.EQI";
      break;
    case LinxISA::CMPNEI:
      Mnem = "CMP.NEI";
      break;
    case LinxISA::CMPLTI:
      Mnem = "CMP.LTI";
      break;
    case LinxISA::CMPGEI:
      Mnem = "CMP.GEI";
      break;
    case LinxISA::CMPLTUI:
      Mnem = "CMP.LTUI";
      break;
    case LinxISA::CMPGEUI:
      Mnem = "CMP.GEUI";
      break;
    case LinxISA::CMPANDI:
      Mnem = "CMP.ANDI";
      break;
    case LinxISA::CMPORI:
      Mnem = "CMP.ORI";
      break;
    case LinxISA::HLCMPANDI:
      Mnem = "HL.CMP.ANDI";
      break;
    case LinxISA::HLCMPORI:
      Mnem = "HL.CMP.ORI";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    // HL compare forms are always 48-bit.
    const unsigned LenBits = (Mnem.starts_with("HL.")) ? 48u : 32u;

    // Try the 16-bit compare-immediate forms when the operand constraints match:
    //   C.CMP.{EQI,NEI}  t#1, simm5, ->t
    if (LenBits == 32 && (Opc == LinxISA::CMPEQI || Opc == LinxISA::CMPNEI) &&
        MI->getNumOperands() >= 3) {
      const Register Dst = MI->getOperand(0).getReg();
      const Register SrcL = MI->getOperand(1).getReg();
      const int64_t Imm = I(2);
      if (Dst == LinxISA::U4 && SrcL == LinxISA::T1 && isInt<5>(Imm)) {
        const StringRef Cmnem = (Opc == LinxISA::CMPEQI) ? "C.CMP.EQI" : "C.CMP.NEI";
        OutMI.setOpcode(getSpecOpcode(Cmnem, /*LengthBits=*/16, /*Fields=*/1));
        OutMI.addOperand(MCOperand::createImm(Imm)); // simm5
        return;
      }
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/LenBits, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcL
    OutMI.addOperand(MCOperand::createImm(I(2))); // simm12/uimm12
    return;
  }

  case LinxISA::CSELrrr: {
    // `CSEL SrcP, SrcL, SrcR<.neg>, ->{t, u, Rd}`
    // LinxISA csel semantics: if pred != 0, use SrcL (true case), else SrcR (false)
    // LLVM CSELrrr operands: (rd, pred, src_true, src_false)
    // Map: SrcL = true case, SrcR = false case, SrcP = predicate
    OutMI.setOpcode(getSpecOpcode("CSEL", /*LengthBits=*/32, /*Fields=*/5));
    OutMI.addOperand(MCOperand::createImm(R(0))); // RegDst
    OutMI.addOperand(MCOperand::createImm(R(2))); // SrcL = true case
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcP = predicate
    OutMI.addOperand(MCOperand::createImm(R(3))); // SrcR = false case
    OutMI.addOperand(MCOperand::createImm(3));    // SrcRType (default: no modifier)
    return;
  }

  case LinxISA::JUMP: {
    OutMI.setOpcode(getSpecOpcode("J", /*LengthBits=*/32, /*Fields=*/1));
    OutMI.addOperand(lowerBranchTarget(0));
    return;
  }

  case LinxISA::BEQ:
  case LinxISA::BNE:
  case LinxISA::BLT:
  case LinxISA::BGE:
  case LinxISA::BLTU:
  case LinxISA::BGEU: {
    StringRef Mnem;
    switch (Opc) {
    case LinxISA::BEQ:
      Mnem = "B.EQ";
      break;
    case LinxISA::BNE:
      Mnem = "B.NE";
      break;
    case LinxISA::BLT:
      Mnem = "B.LT";
      break;
    case LinxISA::BGE:
      Mnem = "B.GE";
      break;
    case LinxISA::BLTU:
      Mnem = "B.LTU";
      break;
    case LinxISA::BGEU:
      Mnem = "B.GEU";
      break;
    default:
      llvm_unreachable("Unexpected opcode");
    }

    OutMI.setOpcode(getSpecOpcode(Mnem, /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(R(1))); // SrcR
    OutMI.addOperand(lowerBranchTarget(2));       // simm12 (pcrel)
    return;
  }

  case LinxISA::JR: {
    OutMI.setOpcode(getSpecOpcode("JR", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // SrcL
    OutMI.addOperand(MCOperand::createImm(0));    // SrcZero (zero)
    OutMI.addOperand(MCOperand::createImm(0));    // simm12
    return;
  }

  //===----------------------------------------------------------------------===//
  // Function Entry/Exit Macro Instructions (LinxISA spec)
  //===----------------------------------------------------------------------===//

  case LinxISA::FENTRY: {
    // FENTRY [Begin ~ End], sp!, stacksize
    // Fields: SrcBegin, SrcEnd, uimm (split encoding)
    OutMI.setOpcode(getSpecOpcode("FENTRY", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // reg_begin
    OutMI.addOperand(MCOperand::createImm(I(1))); // reg_end
    OutMI.addOperand(MCOperand::createImm(I(2))); // stacksize
    return;
  }

  case LinxISA::FEXIT: {
    // FEXIT [Begin ~ End], sp!, stacksize
    OutMI.setOpcode(getSpecOpcode("FEXIT", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // reg_begin
    OutMI.addOperand(MCOperand::createImm(I(1))); // reg_end
    OutMI.addOperand(MCOperand::createImm(I(2))); // stacksize
    return;
  }

  case LinxISA::FRET_RA: {
    // FRET.RA [Begin ~ End], sp!, stacksize
    OutMI.setOpcode(getSpecOpcode("FRET.RA", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // reg_begin
    OutMI.addOperand(MCOperand::createImm(I(1))); // reg_end
    OutMI.addOperand(MCOperand::createImm(I(2))); // stacksize
    return;
  }

  case LinxISA::FRET_STK: {
    // FRET.STK [Begin ~ End], sp!, stacksize
    OutMI.setOpcode(getSpecOpcode("FRET.STK", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(I(0))); // reg_begin
    OutMI.addOperand(MCOperand::createImm(I(1))); // reg_end
    OutMI.addOperand(MCOperand::createImm(I(2))); // stacksize
    return;
  }

  case LinxISA::MCOPY: {
    // MCOPY [DstAddr, SrcAddr, Size]
    OutMI.setOpcode(getSpecOpcode("MCOPY", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // dst
    OutMI.addOperand(MCOperand::createImm(R(1))); // src
    OutMI.addOperand(MCOperand::createImm(R(2))); // size
    return;
  }

  case LinxISA::MSET: {
    // MSET [DstAddr, Value, Size]
    OutMI.setOpcode(getSpecOpcode("MSET", /*LengthBits=*/32, /*Fields=*/3));
    OutMI.addOperand(MCOperand::createImm(R(0))); // dst
    OutMI.addOperand(MCOperand::createImm(R(1))); // val
    OutMI.addOperand(MCOperand::createImm(R(2))); // size
    return;
  }

  default:
    MI->print(errs());
    report_fatal_error("Linx: unsupported machine instruction in MC lowering");
  }
}
