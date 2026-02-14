#include "MCTargetDesc/LinxISAInstPrinter.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace llvm;

static StringRef reg5Name(unsigned Code) {
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

static void printReg10Name(raw_ostream &OS, unsigned Code) {
  Code &= 0x3ffu;
  if (Code < 32u) {
    OS << reg5Name(Code);
    return;
  }

  const unsigned Class = (Code >> 5) & 0x1fu;
  const unsigned Index = Code & 0x1fu;

  switch (Class) {
  case 1: // ri*
    OS << "ri" << utostr(Index);
    return;
  case 3: // lc*
    OS << "lc" << utostr(Index);
    return;
  case 4: // vt*
    OS << "vt";
    if (Index)
      OS << "#" << utostr(Index);
    return;
  case 5: // vu*
    OS << "vu";
    if (Index)
      OS << "#" << utostr(Index);
    return;
  case 6: // vm*
    OS << "vm";
    if (Index)
      OS << "#" << utostr(Index);
    return;
  case 7: // vn*
    OS << "vn";
    if (Index)
      OS << "#" << utostr(Index);
    return;
  case 8: { // tile bases
    static constexpr const char *Bases[] = {"ta", "tb", "tc", "td", "to", "ts"};
    if (Index < std::size(Bases)) {
      OS << Bases[Index];
      return;
    }
    break;
  }
  default:
    break;
  }

  OS << "vreg" << utostr(Code);
}

static void printTileRef(raw_ostream &OS, unsigned TileId) {
  const unsigned Hand = (TileId >> 3) & 0x3u;
  const unsigned Depth = TileId & 0x7u;
  const char Prefix = (Hand == 0) ? 't'
                     : (Hand == 1) ? 'u'
                     : (Hand == 2) ? 'm'
                                   : 'n';
  OS << Prefix << "#" << utostr(Depth + 1u);
}

static StringRef dtypeName(unsigned DT) {
  switch (DT & 0x1f) {
  case 0:
    return "INT32";
  case 1:
    return "FP32";
  case 4:
    return "FP16";
  case 6:
    return "BF16";
  default:
    return StringRef();
  }
}

static StringRef parTileOpName(unsigned TileOp10) {
  switch (TileOp10 & 0x3ffu) {
  case 0:
    return "VCALL";
  case 2:
    return "MAMULB";
  case 33:
    return "TLOAD";
  case 65:
    return "TSTORE";
  case 66:
    return "MAMULB.ACC";
  case 258:
    return "ACCCVT";
  default:
    return StringRef();
  }
}

static bool isCubeTileOp(unsigned TileOp10) {
  switch (TileOp10 & 0x3ffu) {
  case 2:
  case 66:
  case 258:
    return true;
  default:
    return false;
  }
}

static StringRef brTypeName(unsigned BrType) {
  switch (BrType & 0x7) {
  case 1:
    return "FALL";
  case 2:
    return "DIRECT";
  case 3:
    return "COND";
  case 4:
    return "CALL";
  case 5:
    return "IND";
  case 6:
    return "ICALL";
  case 7:
    return "RET";
  default:
    return "BR?";
  }
}

static StringRef blockTypeSuffix(unsigned BlockType) {
  switch (BlockType & 0x1f) {
  case 0:
    return "STD";
  case 1:
    return "SYS";
  case 2:
    return "FP";
  default:
    return StringRef();
  }
}

static const char *ssrIdSymbol(uint64_t Id) {
  // SSR_ID is encoded as a 12-bit field in the base forms (SSRGET/SSRSET/SSRSWAP).
  // Keep this mapping aligned with isa.txt (and the ISA manual SSR table).
  unsigned V = static_cast<unsigned>(Id) & 0xfffu;

  // Debug SSRs (v0.2 bring-up subset).
  if (V == 0xf80)
    return "DBGID_ACRn";
  if (V >= 0xf90 && V <= 0xf97) {
    static constexpr const char *Names[8] = {
        "DBCR0_ACRn", "DBVR0_ACRn", "DBCR1_ACRn", "DBVR1_ACRn",
        "DBCR2_ACRn", "DBVR2_ACRn", "DBCR3_ACRn", "DBVR3_ACRn",
    };
    return Names[V - 0xf90];
  }
  if (V == 0xfa0)
    return "DCCR0_ACRn";
  if (V == 0xfa1)
    return "DCVR0_ACRn";
  if (V >= 0xfb0 && V <= 0xfb7) {
    static constexpr const char *Names[8] = {
        "DWCR0_ACRn", "DWVR0_ACRn", "DWCR1_ACRn", "DWVR1_ACRn",
        "DWCR2_ACRn", "DWVR2_ACRn", "DWCR3_ACRn", "DWVR3_ACRn",
    };
    return Names[V - 0xfb0];
  }

  switch (V) {
  case 0x000:
    return "TP";
  case 0x001:
    return "GP";
  case 0x010:
    return "TIME";
  case 0xc00:
    return "CYCLE";
  case 0x020:
    return "CSTATE";
  case 0x021:
    return "LXLCID";
  case 0x022:
    return "VENDOR";
  case 0x023:
    return "VERSION";
  case 0x024:
    return "LCFR";
  case 0x025:
    return "LCFR_EN";
  case 0x800:
    return "TR1";
  case 0x801:
    return "TR2";
  case 0x810:
    return "SYSCNT";
  case 0x820:
    return "CW";
  case 0x830:
    return "MSGBCR";
  case 0x831:
    return "MSGBD1";
  case 0x832:
    return "MSGBD2";
  case 0x833:
    return "MSGBD3";
  case 0x834:
    return "MSGBD4";
  case 0x835:
    return "MSGBD5";
  case 0x836:
    return "MSGBD6";
  case 0x837:
    return "MSGBD7";
  case 0x838:
    return "MSGBD8";
  case 0x839:
    return "MSGBD9";
  case 0x83a:
    return "MSGBD10";

  // Privileged/ACR-scoped families (encoded low 12 bits).
  case 0xf00:
    return "ECSTATE_ACRn";
  case 0xf01:
    return "EVBASE_ACRn";
  case 0xf02:
    return "TRAPNO_ACRn";
  case 0xf03:
    return "TRAPARG0_ACRn";
  case 0xf05:
    return "ETEMP_ACRn";
  case 0xf06:
    return "ETEMP0_ACRn";
  case 0xf07:
    return "ECONFIG_ACRn";
  case 0xf08:
    return "IPENDING_ACRn";
  case 0xf09:
    return "TOPEI_ACRn";
  case 0xf0a:
    return "EOIEI_ACRn";
  case 0xf10:
    return "TTBR0_ACR1";
  case 0xf11:
    return "TTBR1_ACR1";
  case 0xf12:
    return "TCR_ACR1";
  case 0xf13:
    return "MAIR_ACR1";
  case 0xf14:
    return "IOTTBR_ACR1";
  case 0xf15:
    return "IOTCR_ACR1";
  case 0xf16:
    return "IOMAIR_ACR1";
  case 0xf20:
    return "TIMER_TIME_ACRn";
  case 0xf21:
    return "TIMER_TIMECMP_ACRn";
  case 0xf30:
    return "XBINFO_ACRn";
  case 0xf31:
    return "ACR_PARAM_ACRn";

  // EBARG group (v0.2): 0xF40+.
  case 0xf40:
    return "EBARG0_ACRn";
  case 0xf41:
    return "EBARG_BPC_CUR_ACRn";
  case 0xf42:
    return "EBARG_BPC_TGT_ACRn";
  case 0xf43:
    return "EBARG_TPC_ACRn";
  case 0xf44:
    return "EBARG_LRA_ACRn";
  case 0xf45:
    return "EBARG_TQ0_ACRn";
  case 0xf46:
    return "EBARG_TQ1_ACRn";
  case 0xf47:
    return "EBARG_TQ2_ACRn";
  case 0xf48:
    return "EBARG_TQ3_ACRn";
  case 0xf49:
    return "EBARG_UQ0_ACRn";
  case 0xf4a:
    return "EBARG_UQ1_ACRn";
  case 0xf4b:
    return "EBARG_UQ2_ACRn";
  case 0xf4c:
    return "EBARG_UQ3_ACRn";
  case 0xf4d:
    return "EBARG_LB_ACRn";
  case 0xf4e:
    return "EBARG_LC_ACRn";
  case 0xf4f:
    return "EBARG_EXTCTX_PTR_ACRn";
  case 0xf50:
    return "EBARG_EXTCTX_META_ACRn";
  default:
    return nullptr;
  }
}

static int64_t shlSigned64(int64_t V, unsigned Shift) {
  APInt A(64, static_cast<uint64_t>(V), /*isSigned=*/true);
  A <<= Shift;
  return A.getSExtValue();
}

static uint64_t shlUnsigned64(uint64_t V, unsigned Shift) {
  APInt A(64, V, /*isSigned=*/false);
  A <<= Shift;
  return A.getZExtValue();
}

std::pair<const char *, uint64_t>
LinxISAInstPrinter::getMnemonic(const MCInst &MI) const {
  static constexpr const char *BadOpcode = "<bad-opcode>";
  static constexpr const char *Invalid = "<invalid>";

  const unsigned Opcode = MI.getOpcode();
  if (Opcode >= linxisa_inst_forms_count)
    return {BadOpcode, 0};

  const linxisa_inst_form &Form = linxisa_inst_forms[Opcode];
  if (Form.mnemonic && Form.mnemonic[0])
    return {Form.mnemonic, 0};
  if (Form.id && Form.id[0])
    return {Form.id, 0};
  return {Invalid, 0};
}

void LinxISAInstPrinter::printRegName(raw_ostream &OS, MCRegister Reg) {
  // Reg numbering is target-defined; for early bring-up keep this stable by
  // printing numeric registers.
  OS << "r" << Reg.id();
}

static bool asmImpliesArrowDest(StringRef Asm, StringRef Dest) {
  SmallString<64> Compact;
  Compact.reserve(Asm.size());
  for (char C : Asm) {
    if (C == ' ' || C == '\t')
      continue;
    Compact.push_back(llvm::toLower(C));
  }
  return StringRef(Compact).contains(Dest);
}

void LinxISAInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                   StringRef Annot,
                                   const MCSubtargetInfo & /*STI*/,
                                   raw_ostream &OS) {
  const unsigned Opcode = MI->getOpcode();
  if (Opcode >= linxisa_inst_forms_count) {
    OS << "<bad-opcode>";
    return;
  }

  const linxisa_inst_form &Form = linxisa_inst_forms[Opcode];
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  const StringRef RawTok = AsmFmt.empty() ? StringRef()
                                          : AsmFmt.split(' ').first.rtrim(",");

  auto stripAngleSuffix = [&](StringRef Tok) -> StringRef {
    if (size_t Pos = Tok.find('<'); Pos != StringRef::npos)
      Tok = Tok.take_front(Pos);
    return Tok;
  };

  const bool IsVector =
      Form.length_bits == 64 &&
      stripAngleSuffix(RawTok).starts_with_insensitive("v.");

  // Map field name -> operand (immediate or expression) from MCInst operands in
  // spec field order.
  SmallVector<std::pair<StringRef, MCOperand>, 16> Fields;
  const unsigned FieldCount = Form.field_count;
  for (unsigned i = 0; i < FieldCount; ++i) {
    if (i >= MI->getNumOperands())
      break;
    const linxisa_field &F = linxisa_fields[Form.field_start + i];
    if (!F.name)
      continue;
    const MCOperand &Op = MI->getOperand(i);
    if (!Op.isImm() && !Op.isExpr())
      continue;
    Fields.push_back({StringRef(F.name), Op});
  }

  auto findField = [&](StringRef Name) -> std::optional<MCOperand> {
    for (auto &KV : Fields)
      if (KV.first == Name)
        return KV.second;
    return std::nullopt;
  };

  auto findFieldImm = [&](StringRef Name) -> std::optional<int64_t> {
    if (auto Op = findField(Name)) {
      if (Op->isImm())
        return Op->getImm();
    }
    return std::nullopt;
  };

  SmallString<32> PrintedMnemonicTok;
  auto mnemonicTok = [&](StringRef Default) -> StringRef {
    PrintedMnemonicTok.clear();
    if (RawTok.empty())
      return Default;

    StringRef Tok = stripAngleSuffix(RawTok);
    if (Tok.equals_insensitive("c.break") && Form.mnemonic &&
        StringRef(Form.mnemonic).equals_insensitive("C.EBREAK"))
      return "c.ebreak";

    if (Tok.empty())
      return Default;

    auto emitFpT = [&](unsigned SrcType) {
      switch (SrcType & 0x3u) {
      case 0:
        PrintedMnemonicTok += "fd";
        break;
      case 1:
        PrintedMnemonicTok += "fs";
        break;
      case 2:
        PrintedMnemonicTok += "fh";
        break;
      case 3:
        PrintedMnemonicTok += "fb";
        break;
      }
    };

    auto emitCvtFpDst = [&](unsigned DstType) {
      switch (DstType & 0x1fu) {
      case 0:
        PrintedMnemonicTok += "fd";
        break;
      case 1:
        PrintedMnemonicTok += "fs";
        break;
      default:
        PrintedMnemonicTok += "dt";
        PrintedMnemonicTok += utostr(DstType & 0x1fu);
        break;
      }
    };

    auto emitCvtIntDst = [&](unsigned DstType) {
      switch (DstType & 0x1fu) {
      case 0:
        PrintedMnemonicTok += "ud";
        break;
      case 1:
        PrintedMnemonicTok += "uw";
        break;
      case 2:
        PrintedMnemonicTok += "uh";
        break;
      case 3:
        PrintedMnemonicTok += "ub";
        break;
      case 8:
        PrintedMnemonicTok += "sd";
        break;
      case 9:
        PrintedMnemonicTok += "sw";
        break;
      case 10:
        PrintedMnemonicTok += "sh";
        break;
      case 11:
        PrintedMnemonicTok += "sb";
        break;
      default:
        PrintedMnemonicTok += "dt";
        PrintedMnemonicTok += utostr(DstType & 0x1fu);
        break;
      }
    };

    auto emitCvtIntSrc = [&](unsigned SrcType, bool Signed) {
      switch (SrcType & 0x3u) {
      case 0:
        PrintedMnemonicTok += Signed ? "sd" : "ud";
        break;
      case 1:
        PrintedMnemonicTok += Signed ? "sw" : "uw";
        break;
      case 2:
        PrintedMnemonicTok += Signed ? "sh" : "uh";
        break;
      case 3:
        PrintedMnemonicTok += Signed ? "sb" : "ub";
        break;
      }
    };

    if (Tok.contains("{T}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);
      unsigned SrcType = 0;
      if (auto V = findFieldImm("SrcType"))
        SrcType = static_cast<unsigned>(*V);
      emitFpT(SrcType);
      return PrintedMnemonicTok;
    }

    if (Tok.contains("{srcT2dstT}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);

      unsigned SrcType = 0;
      if (auto V = findFieldImm("SrcType"))
        SrcType = static_cast<unsigned>(*V);

      unsigned DstType = 0;
      if (auto V = findFieldImm("DstType"))
        DstType = static_cast<unsigned>(*V);

      StringRef Mnem = Form.mnemonic ? StringRef(Form.mnemonic) : StringRef();

      if (Mnem.equals_insensitive("SCVTF")) {
        emitCvtIntSrc(SrcType, /*Signed=*/true);
        PrintedMnemonicTok += "2";
        emitCvtFpDst(DstType);
      } else if (Mnem.equals_insensitive("UCVTF")) {
        emitCvtIntSrc(SrcType, /*Signed=*/false);
        PrintedMnemonicTok += "2";
        emitCvtFpDst(DstType);
      } else if (Mnem.equals_insensitive("FCVTZ")) {
        emitFpT(SrcType);
        PrintedMnemonicTok += "2";
        emitCvtIntDst(DstType);
      } else {
        // FCVT/FCVTA/FCVTM/FCVTN/FCVTP: default to FP->FP naming.
        emitFpT(SrcType);
        PrintedMnemonicTok += "2";
        emitCvtFpDst(DstType);
      }

      return PrintedMnemonicTok;
    }

    // v0.3 vector bridge: `<.local>` is a mnemonic qualifier driven by the L bit.
    if (AsmFmt.contains("<.local>")) {
      if (auto L = findFieldImm("L")) {
        if (((*L) & 1) != 0) {
          PrintedMnemonicTok += Tok;
          PrintedMnemonicTok += ".local";
          return PrintedMnemonicTok;
        }
      }
    }

    return Tok;
  };

  auto emitReg = [&](StringRef FieldName) {
    auto V = findField(FieldName);
    if (!V)
      return;
    if (!V->isImm())
      return;
    unsigned Code = static_cast<unsigned>(V->getImm());
    if (IsVector)
      printReg10Name(OS, Code);
    else
      OS << reg5Name(Code & 0x1F);
  };

  auto emitPcRelTargetHexScaled = [&](StringRef FieldName, bool Signed,
                                      unsigned Shift) -> bool {
    auto Op = findField(FieldName);
    if (!Op)
      return false;
    if (Op->isExpr()) {
      MAI.printExpr(OS, *Op->getExpr());
      return true;
    }
    if (!Op->isImm())
      return false;

    uint64_t Target = 0;
    if (Signed) {
      int64_t Delta = shlSigned64(Op->getImm(), Shift);
      int64_t SignedTarget = static_cast<int64_t>(Address) + Delta;
      Target = static_cast<uint64_t>(SignedTarget);
    } else {
      uint64_t Delta = shlUnsigned64(static_cast<uint64_t>(Op->getImm()), Shift);
      Target = Address + Delta;
    }

    OS << "0x" << utohexstr(Target, /*LowerCase=*/true);
    return true;
  };

  auto emitPcRelTargetHex = [&](StringRef FieldName, bool Signed) -> bool {
    return emitPcRelTargetHexScaled(FieldName, Signed, /*Shift=*/1);
  };

  auto emitSetRetTarget = [&]() -> bool {
    // setret/c.setret immediate is an instruction-halfword offset.
    if (findField("uimm5")) {
      return emitPcRelTargetHex("uimm5", /*Signed=*/false);
    }
    if (findField("imm20")) {
      return emitPcRelTargetHex("imm20", /*Signed=*/false);
    }
    if (findField("imm32")) {
      return emitPcRelTargetHex("imm32", /*Signed=*/true);
    }
    return false;
  };

  auto emitBlockTarget = [&]() -> bool {
    // BSTART label fields are instruction-halfword offsets.
    if (findField("simm25"))
      return emitPcRelTargetHex("simm25", /*Signed=*/true);
    if (findField("simm17"))
      return emitPcRelTargetHex("simm17", /*Signed=*/true);
    if (findField("simm12"))
      return emitPcRelTargetHex("simm12", /*Signed=*/true);
    // HL.BSTART uses an instruction-aligned byte offset (simm[0] is implicit 0).
    if (findField("simm"))
      return emitPcRelTargetHexScaled("simm", /*Signed=*/true, /*Shift=*/0);
    return false;
  };

  // Special-case: setret/c.setret want a printed target address instead of the
  // raw immediate encoding.
  if (AsmFmt.starts_with_insensitive("setret") ||
      AsmFmt.starts_with_insensitive("c.setret") ||
      AsmFmt.starts_with_insensitive("hl.setret")) {
    OS << mnemonicTok("setret");
    OS << "\t";
    if (!emitSetRetTarget())
      OS << "0x0";
    OS << ",\t->ra";
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: accelerator/tile block-start instructions.
  //
  // v0.3-facing disassembly is typed (`BSTART.TMA` / `BSTART.CUBE`).
  static constexpr StringLiteral LegacyPackedStart = "BSTART." "PAR";
  const bool IsLegacyPacked = AsmFmt.starts_with(LegacyPackedStart);
  const bool IsTypedTMA = AsmFmt.starts_with("BSTART.TMA");
  const bool IsTypedCUBE = AsmFmt.starts_with("BSTART.CUBE");
  if (IsLegacyPacked || IsTypedTMA || IsTypedCUBE) {
    const unsigned DT =
        static_cast<unsigned>(findFieldImm("DataType").value_or(0)) & 0x1fu;

    unsigned TileOp10 = 0;
    if (IsLegacyPacked) {
      TileOp10 = static_cast<unsigned>(findFieldImm("TileOp10").value_or(0)) &
                 0x3ffu;
    } else {
      const unsigned Func =
          static_cast<unsigned>(findFieldImm("Function").value_or(0)) & 0x1fu;
      if (IsTypedTMA) {
        TileOp10 = (Func == 0) ? 33u : (Func == 1) ? 65u : (32u + Func);
      } else {
        TileOp10 = Func;
        if (Func == 0) {
          TileOp10 = 2u;
        } else if (Func == 2) {
          TileOp10 = 66u;
        } else if (Func == 8) {
          TileOp10 = 258u;
        }
      }
    }

    const char *TypedPrefix = isCubeTileOp(TileOp10) ? "BSTART.CUBE" : "BSTART.TMA";
    OS << TypedPrefix << "\t";
    if (StringRef N = parTileOpName(TileOp10); !N.empty()) {
      OS << N;
    } else {
      OS << utostr(TileOp10);
    }
    OS << ", ";
    if (StringRef DTName = dtypeName(DT); !DTName.empty()) {
      OS << DTName;
    } else {
      OS << "DT" << utostr(DT);
    }
    LastParTileOp = TileOp10;
    LastParTileOpValid = true;
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: block split instructions. LLVM uses these as block
  // terminators; Linx uses halfword PC-relative offsets.
  const bool IsCBSTART = AsmFmt.starts_with("C.BSTART");
  const bool IsBSTART = AsmFmt.starts_with("HL.BSTART") ||
                        AsmFmt.starts_with("BSTART.STD") ||
                        AsmFmt.starts_with("BSTART.FP") ||
                        AsmFmt.starts_with("BSTART.SYS") ||
                        AsmFmt.starts_with("BSTART.") ||
                        AsmFmt.starts_with("BSTART ");
  if (IsCBSTART || IsBSTART) {
    StringRef FirstTok = RawTok;
    StringRef FirstTokBase = stripAngleSuffix(FirstTok);
    const bool HasBlockTypePlaceholder = AsmFmt.contains("<.BlockType>");

    // Render C.BSTART<.BlockType> as C.BSTART.<suffix>.
    SmallString<32> PrintedMnemonic;
    if (HasBlockTypePlaceholder) {
      unsigned BT = 0;
      if (auto V = findFieldImm("BlockType"))
        BT = static_cast<unsigned>(*V);
      PrintedMnemonic = FirstTokBase;
      // STD is the default and is omitted for readability.
      if ((BT & 0x1f) != 0) {
        PrintedMnemonic += ".";
        if (StringRef S = blockTypeSuffix(BT); !S.empty())
          PrintedMnemonic += S;
        else {
          PrintedMnemonic += "BT";
          PrintedMnemonic += utostr(BT & 0x1f);
        }
      }
      OS << PrintedMnemonic;
    } else if (FirstTokBase == "C.BSTART" &&
               (AsmFmt.contains(" DIRECT") || AsmFmt.contains(" COND"))) {
      // These encodings are scalar-block forms; the default BlockType is STD.
      OS << "C.BSTART";
    } else if (FirstTokBase == "BSTART" &&
               (AsmFmt.contains("{DIRECT, CALL}") || AsmFmt.contains(" COND"))) {
      // These encodings are scalar-block forms; the default BlockType is STD.
      OS << "BSTART";
    } else {
      OS << FirstTokBase;
    }

    enum class BrKind {
      Unknown,
      Fall,
      Direct,
      Cond,
      Call,
      Ind,
      ICall,
      Ret,
    };

    BrKind K = BrKind::Unknown;
    if (auto V = findFieldImm("BrType")) {
      switch (static_cast<unsigned>(*V) & 0x7) {
      case 1:
        K = BrKind::Fall;
        break;
      case 2:
        K = BrKind::Direct;
        break;
      case 3:
        K = BrKind::Cond;
        break;
      case 4:
        K = BrKind::Call;
        break;
      case 5:
        K = BrKind::Ind;
        break;
      case 6:
        K = BrKind::ICall;
        break;
      case 7:
        K = BrKind::Ret;
        break;
      default:
        K = BrKind::Unknown;
        break;
      }
    } else {
      // Order matters: ICALL contains "CALL" as a substring.
      if (AsmFmt.contains("{DIRECT, CALL}"))
        K = BrKind::Direct;
      else if (AsmFmt.contains(" ICALL"))
        K = BrKind::ICall;
      else if (AsmFmt.contains(" IND"))
        K = BrKind::Ind;
      else if (AsmFmt.contains(" RET"))
        K = BrKind::Ret;
      else if (AsmFmt.contains(" COND"))
        K = BrKind::Cond;
      else if (AsmFmt.contains(" DIRECT"))
        K = BrKind::Direct;
      else if (AsmFmt.contains(" CALL"))
        K = BrKind::Call;
      else if (AsmFmt.contains(" FALL"))
        K = BrKind::Fall;
    }

    auto emitKind = [&](StringRef S) {
      if (!S.empty())
        OS << "\t" << S;
    };

    auto emitKindAndLabel = [&](StringRef S) {
      OS << "\t" << S << ", ";
      emitBlockTarget();
    };

    switch (K) {
    case BrKind::Unknown:
      // Best-effort: if this is the BrType-form, show symbolic BrType.
      if (auto V = findFieldImm("BrType")) {
        StringRef N = brTypeName(static_cast<unsigned>(*V));
        if (N != "FALL")
          emitKind(N);
      }
      // Otherwise, fall back to generic printing below.
      break;
    case BrKind::Fall: {
      // For .STD/.SYS/.FP, FALL is the default and can be omitted. If a fixup
      // label is encoded (non-zero offset), show it.
      int64_t Off = 0;
      if (auto V = findFieldImm("simm17"))
        Off = *V;
      else if (auto V = findFieldImm("simm25"))
        Off = *V;
      else if (auto V = findFieldImm("simm12"))
        Off = *V;
      if (Off != 0)
        emitKindAndLabel("FALL");
      break;
    }
    case BrKind::Direct:
      emitKindAndLabel("DIRECT");
      break;
    case BrKind::Cond:
      emitKindAndLabel("COND");
      break;
    case BrKind::Call:
      emitKindAndLabel("CALL");
      break;
    case BrKind::Ind:
      emitKind("IND");
      break;
    case BrKind::ICall:
      emitKind("ICALL");
      break;
    case BrKind::Ret:
      emitKind("RET");
      break;
    }

    // Disassembler sugar: if a BSTART CALL MCInst carries an extra operand,
    // render it as a fused return-target annotation (`ra=...`).
    if (K == BrKind::Call && MI->getNumOperands() > FieldCount) {
      const MCOperand &RetOp = MI->getOperand(FieldCount);
      OS << ", ra=";
      if (RetOp.isExpr()) {
        MAI.printExpr(OS, *RetOp.getExpr());
      } else if (RetOp.isImm()) {
        OS << "0x"
           << utohexstr(static_cast<uint64_t>(RetOp.getImm()),
                        /*LowerCase=*/true);
      }
    }

    printAnnotation(OS, Annot);
    return;
  }

  auto srcRTypeSuffix = [&](unsigned V) -> StringRef {
    switch (V & 0x3u) {
    case 0:
      return ".sw";
    case 1:
      return ".uw";
    case 2:
      return ".neg";
    default:
      return StringRef();
    }
  };

  auto emitSrcRWithTypeAndShift = [&](std::optional<int64_t> ForcedShift) {
    emitReg("SrcR");
    if (auto V = findFieldImm("SrcRType")) {
      if (StringRef S = srcRTypeSuffix(static_cast<unsigned>(*V)); !S.empty())
        OS << S;
    }

    std::optional<int64_t> Sh;
    if (ForcedShift)
      Sh = *ForcedShift;
    else if (auto V = findFieldImm("shamt"))
      Sh = *V;

    if (Sh && *Sh != 0)
      OS << "<<" << (*Sh & 0x1f);
  };

  auto emitArrowDest = [&]() {
    if (auto Op = findField("RegDst")) {
      if (Op->isImm()) {
        unsigned Code = static_cast<unsigned>(Op->getImm());
        if (IsVector) {
          OS << ",\t->";
          printReg10Name(OS, Code);
          return;
        }
        Code &= 0x1F;
        if (Code == 31) {
          OS << ",\t->t";
        } else if (Code == 30) {
          // `->u` is the explicit U-hand queue push selector.
          OS << ",\t->u";
        } else {
          OS << ",\t->" << reg5Name(Code);
        }
        return;
      }
    }

    if (asmImpliesArrowDest(AsmFmt, "->t")) {
      OS << ",\t->t";
      return;
    }
    if (asmImpliesArrowDest(AsmFmt, "->u")) {
      OS << ",\t->u";
      return;
    }
    if (asmImpliesArrowDest(AsmFmt, "->ra")) {
      OS << ",\t->ra";
      return;
    }
  };

  // Special-case: FENTRY/FEXIT/FRET.RA/FRET.STK with register range syntax.
  // Format: MNEM [RegBegin ~ RegEnd], sp!, stacksize
  // Must check BEFORE memory operand check since these also contain '['.
  if (AsmFmt.contains("[RegSrc0 ~ RegSrcn]") ||
      AsmFmt.contains("[RegDst0 ~ RegDstn]")) {
    StringRef Tok = Form.mnemonic ? StringRef(Form.mnemonic) : StringRef("FENTRY");
    OS << Tok;
    OS << "\t[";
    
    // Get register range from SrcBegin/SrcEnd or DstBegin/DstEnd fields
    unsigned RegBegin = 10, RegEnd = 14;  // defaults: ra ~ s2
    if (auto V = findFieldImm("SrcBegin"))
      RegBegin = static_cast<unsigned>(*V);
    else if (auto V = findFieldImm("DstBegin"))
      RegBegin = static_cast<unsigned>(*V);
    if (auto V = findFieldImm("SrcEnd"))
      RegEnd = static_cast<unsigned>(*V);
    else if (auto V = findFieldImm("DstEnd"))
      RegEnd = static_cast<unsigned>(*V);
    
    OS << reg5Name(RegBegin & 0x1F) << " ~ " << reg5Name(RegEnd & 0x1F);
    OS << "], sp!, ";
    
    // Get stack size from uimm field
    if (auto V = findFieldImm("uimm")) {
      // uimm is already in bytes (reconstructed from split encoding)
      OS << *V;
    } else {
      OS << "0";
    }
    
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: GPR descriptor binding (B.IOR).
  if (AsmFmt.starts_with("B.IOR")) {
    const unsigned Src0 =
        static_cast<unsigned>(findFieldImm("RegSrc0").value_or(0)) & 0x1fu;
    const unsigned Src1 =
        static_cast<unsigned>(findFieldImm("RegSrc1").value_or(0)) & 0x1fu;
    const unsigned Src2 =
        static_cast<unsigned>(findFieldImm("RegSrc2").value_or(0)) & 0x1fu;

    auto printRegList = [&](ArrayRef<unsigned> Regs) {
      bool First = true;
      for (unsigned R : Regs) {
        if (R == 0)
          continue;
        if (!First)
          OS << ",";
        OS << reg5Name(R);
        First = false;
      }
    };

    OS << "B.IOR\t[";
    // Disassembly contract: treat zero as absent and omit it.
    //
    // The common bring-up streams place base/stride in RegSrc1/RegSrc0 order
    // (example: `[s0,a6]`).
    printRegList({Src1, Src0});
    OS << "],[";
    printRegList({Src2});
    OS << "]";
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: tile block IO descriptors (B.IOT / B.IOTI).
  //
  // These use bracket syntax in the ISA, but they are not memory operands and
  // should not be routed through the generic load/store pretty printer.
  if (AsmFmt.starts_with("B.IOT")) {
    const bool IsIOTI = AsmFmt.starts_with("B.IOTI");
    const unsigned S0V =
        static_cast<unsigned>(findFieldImm("S0V").value_or(0)) & 0x1u;
    const unsigned S1V =
        static_cast<unsigned>(findFieldImm("S1V").value_or(0)) & 0x1u;
    const unsigned S0R =
        static_cast<unsigned>(findFieldImm("S0R").value_or(0)) & 0x1u;
    const unsigned S1R =
        static_cast<unsigned>(findFieldImm("S1R").value_or(0)) & 0x1u;
    const unsigned DstTile =
        static_cast<unsigned>(findFieldImm("DstTile").value_or(0)) & 0x7u;
    const unsigned Src0 =
        static_cast<unsigned>(findFieldImm("SrcTile0").value_or(0)) & 0x1fu;
    const unsigned Src1 =
        static_cast<unsigned>(findFieldImm("SrcTile1").value_or(0)) & 0x1fu;
    const unsigned Reg =
        static_cast<unsigned>(findFieldImm("RegSrc").value_or(0)) & 0x1fu;
    std::optional<int64_t> SizeOpt = findFieldImm("Size");
    if (!SizeOpt)
      SizeOpt = findFieldImm("imm5");
    if (!SizeOpt)
      SizeOpt = findFieldImm("uimm5");
    const unsigned Size = static_cast<unsigned>(SizeOpt.value_or(0)) & 0x1fu;

    OS << (IsIOTI ? "B.IOTI" : "B.IOT");
    OS << "\t[";

    const bool Src0Present = (S0V == 0u);
    const bool Src1Present = (S1V == 0u);
    bool First = true;
    if (Src0Present) {
      printTileRef(OS, Src0);
      if (S0R)
        OS << ".reuse";
      First = false;
    }
    if (Src1Present) {
      if (!First)
        OS << ", ";
      printTileRef(OS, Src1);
      if (S1R)
        OS << ".reuse";
    }

    OS << "]";
    const bool Group1 = AsmFmt.contains("group=1");
    if (Group1)
      OS << ", last";

    const unsigned ActiveParOp = LastParTileOpValid ? LastParTileOp : 0u;

    // v0.3 bring-up: MAMULB-class blocks write an implicit accumulator.
    const bool IsAccDst = (ActiveParOp == 2u || ActiveParOp == 66u);

    const char *DstKind = "t";
    if (IsAccDst) {
      DstKind = "acc";
    } else {
      // If a tile destination is encoded, it lives in the first *absent* source
      // slot (preferring SrcTile1). This matches the disassembly snippet
      // contract where the arrow kind tracks the destination tile hand.
      std::optional<unsigned> DstTileReg;
      if (!Src1Present)
        DstTileReg = Src1;
      else if (!Src0Present)
        DstTileReg = Src0;

      if (DstTileReg) {
        const unsigned Tile = *DstTileReg & 0x1fu;
        if (Tile < 8u)
          DstKind = "t";
        else if (Tile < 16u)
          DstKind = "u";
        else if (Tile < 24u)
          DstKind = "m";
        else
          DstKind = "n";
      } else {
        // Fallback: treat DstTile as an enum in bring-up streams.
        switch (DstTile) {
        case 0u:
          DstKind = "t";
          break;
        case 1u:
          DstKind = "u";
          break;
        case 2u:
          DstKind = "m";
          break;
        case 3u:
          DstKind = "n";
          break;
        case 4u:
          DstKind = "acc";
          break;
        default:
          DstKind = "t";
          break;
        }
      }
    }

    OS << "\t->" << DstKind << "<";
    if (IsIOTI) {
      const uint64_t Bytes = (Size < 60u) ? (1ull << (Size + 4u)) : 0ull;
      if (Bytes >= 1024u && (Bytes % 1024u) == 0u)
        OS << utostr(static_cast<unsigned>(Bytes / 1024u)) << "KB";
      else
        OS << utostr(Size);
    } else {
      OS << reg5Name(Reg);
    }
    OS << ">";

    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: block argument format selector.
  if (AsmFmt.starts_with("B.ARG")) {
    if (AsmFmt.contains("NORM.normal")) {
      OS << "B.ARG\tNORM.normal";
      printAnnotation(OS, Annot);
      return;
    }
    if (AsmFmt.contains("ND2ZN.normal")) {
      OS << "B.ARG\tND2ZN.normal";
      printAnnotation(OS, Annot);
      return;
    }
    if (AsmFmt.contains("DN2NZ.normal")) {
      OS << "B.ARG\tDN2NZ.normal";
      printAnnotation(OS, Annot);
      return;
    }
    if (AsmFmt.contains("DN2ZN.normal")) {
      OS << "B.ARG\tDN2ZN.normal";
      printAnnotation(OS, Annot);
      return;
    }
    if (AsmFmt.contains("NZ2DN.canon")) {
      OS << "B.ARG\tformat=28";
      printAnnotation(OS, Annot);
      return;
    }

    const unsigned Format =
        static_cast<unsigned>(findFieldImm("format").value_or(0)) & 0x1fu;
    StringRef LayoutName = "format";
    switch (Format & 0x7u) {
    case 0:
      LayoutName = "NORM.normal";
      break;
    case 1:
      LayoutName = "ND2NZ.normal";
      break;
    case 2:
      LayoutName = "ND2ZN.normal";
      break;
    case 3:
      LayoutName = "DN2NZ.normal";
      break;
    case 4:
      LayoutName = "DN2ZN.normal";
      break;
    default:
      LayoutName = "format";
      break;
    }
    OS << "B.ARG\t" << LayoutName;
    if (LayoutName == "format")
      OS << "=" << utostr(Format);
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: block argument registers (B.DIM).
  //
  // The LB destinations are pseudo-registers in the ISA syntax and are not
  // modeled as normal `->RegDst` operands.
  if (AsmFmt.starts_with("B.DIM")) {
    const unsigned Reg =
        static_cast<unsigned>(findFieldImm("RegSrc").value_or(0)) & 0x1fu;
    std::optional<int64_t> UimmOpt = findFieldImm("uimm");
    if (!UimmOpt)
      UimmOpt = findFieldImm("uimm17");
    if (!UimmOpt)
      UimmOpt = findFieldImm("imm17");
    const unsigned Uimm = static_cast<unsigned>(UimmOpt.value_or(0)) & 0x1ffffu;
    unsigned Lb = 0;
    if (AsmFmt.contains("->LB1"))
      Lb = 1;
    else if (AsmFmt.contains("->LB2"))
      Lb = 2;

    OS << "B.DIM\t" << reg5Name(Reg) << ", " << Uimm << ", ->lb" << utostr(Lb);
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: compressed block dimensions.
  if (AsmFmt.starts_with("C.B.DIMI")) {
    const unsigned LoopNest =
        static_cast<unsigned>(findFieldImm("LoopNest").value_or(0)) & 0x3u;
    const unsigned Imm =
        static_cast<unsigned>(findFieldImm("imm8").value_or(0)) & 0xffu;
    OS << "C.B.DIMI\t" << Imm << ", \t->lb" << utostr(LoopNest);
    printAnnotation(OS, Annot);
    return;
  }

  // Pretty printer for memory operands.
  if (AsmFmt.contains('[')) {
    const StringRef Mnem = Form.mnemonic ? StringRef(Form.mnemonic) : StringRef();
    const bool IsPcr = Mnem.ends_with(".PCR");
    if (IsPcr) {
      // Prefer a symbol-first syntax for PC-relative accesses:
      //   lw.pcr <sym+addend>, ->rd
      //   sw.pcr rs, <sym+addend>
      OS << mnemonicTok("<pcr>");
      OS << "\t";

      // Loads: (RegDst, simm17) or (RegDst, simm) for HL.*.PCR.
      // Stores: (SrcL, simm).
      auto emitPcrExpr = [&]() {
        OS << "[";
        if (auto Op = findField("simm17")) {
          if (Op->isExpr())
            MAI.printExpr(OS, *Op->getExpr());
          else
            OS << "0x" << utohexstr(static_cast<uint64_t>(Op->getImm()),
                                    /*LowerCase=*/true);
          OS << "]";
          return true;
        }
        if (auto Op = findField("simm")) {
          if (Op->isExpr())
            MAI.printExpr(OS, *Op->getExpr());
          else
            OS << "0x" << utohexstr(static_cast<uint64_t>(Op->getImm()),
                                    /*LowerCase=*/true);
          OS << "]";
          return true;
        }
        OS << "]";
        return false;
      };

      const bool HasDest = AsmFmt.contains("->");
      if (HasDest) {
        if (!emitPcrExpr())
          OS << "0x0";
        emitArrowDest();
        printAnnotation(OS, Annot);
        return;
      }

      // Store: value first, then the symbol.
      emitReg("SrcL");
      OS << ", ";
      if (!emitPcrExpr())
        OS << "0x0";
      printAnnotation(OS, Annot);
      return;
    }

    OS << mnemonicTok("<mem>");
    OS << "\t";

    const bool HasDest = AsmFmt.contains("->");

    if (IsVector) {
      // Vector/SIMT memory ops: `[base, lc0<<k, idx|imm]`.
      StringRef BaseField = "SrcL";
      if (size_t L = AsmFmt.find('['); L != StringRef::npos) {
        StringRef Inside = AsmFmt.substr(L + 1).split(']').first;
        StringRef BasePart = Inside.split(',').first.trim();
        if (BasePart.starts_with_insensitive("srcr"))
          BaseField = "SrcR";
        else if (BasePart.starts_with_insensitive("srcl"))
          BaseField = "SrcL";
      }

      std::optional<StringRef> ValueField;
      if (!HasDest) {
        StringRef Prefix = AsmFmt;
        if (size_t L = Prefix.find('['); L != StringRef::npos)
          Prefix = Prefix.take_front(L);
        if (Prefix.contains_insensitive("SrcD"))
          ValueField = "SrcD";
        else if (Prefix.contains_insensitive("SrcL"))
          ValueField = "SrcL";
      }

      unsigned LaneScale = 0;
      if (AsmFmt.contains_insensitive("lc0<<3"))
        LaneScale = 3;
      else if (AsmFmt.contains_insensitive("lc0<<2"))
        LaneScale = 2;
      else if (AsmFmt.contains_insensitive("lc0<<1"))
        LaneScale = 1;

      auto emitLane = [&]() {
        OS << "lc0";
        if (LaneScale)
          OS << "<<" << utostr(LaneScale);
      };

      auto emitVecImm = [&]() -> bool {
        for (StringRef N : {"simm24", "uimm24", "simm12", "uimm12", "simm"}) {
          if (auto Op = findField(N)) {
            if (Op->isExpr())
              MAI.printExpr(OS, *Op->getExpr());
            else if (Op->isImm())
              OS << Op->getImm();
            return true;
          }
        }
        return false;
      };

      auto emitVecIndex = [&]() -> bool {
        if (!findField("SrcR"))
          return false;
        emitReg("SrcR");
        if (auto Sh = findFieldImm("shamt")) {
          unsigned Shift = static_cast<unsigned>(*Sh) & 0x1fu;
          if (AsmFmt.contains("+shamt"))
            Shift += LaneScale;
          if (Shift)
            OS << "<<" << utostr(Shift);
        }
        return true;
      };

      if (!HasDest) {
        if (ValueField.has_value())
          emitReg(*ValueField);
        OS << ", ";
      }

      OS << "[";
      emitReg(BaseField);
      OS << ", ";
      emitLane();
      OS << ", ";
      if (!emitVecIndex()) {
        if (!emitVecImm())
          OS << "0";
      }
      OS << "]";

      if (HasDest)
        emitArrowDest();

      printAnnotation(OS, Annot);
      return;
    }

    auto scaleFromMnemonic = [&]() -> int64_t {
      if (Mnem == "LBI" || Mnem == "LBUI" || Mnem == "SBI")
        return 1;
      if (Mnem == "LHI" || Mnem == "LHUI" || Mnem == "SHI")
        return 2;
      if (Mnem == "LWI" || Mnem == "LWUI" || Mnem == "SWI" || Mnem == "C.LWI" ||
          Mnem == "C.SWI")
        return 4;
      if (Mnem == "LDI" || Mnem == "SDI" || Mnem == "C.LDI" || Mnem == "C.SDI")
        return 8;
      return 1;
    };

    auto emitScaledImmOff = [&]() -> bool {
      std::optional<int64_t> Off;
      if (auto V = findFieldImm("simm12"))
        Off = *V;
      else if (auto V = findFieldImm("uimm12"))
        Off = *V;
      else if (auto V = findFieldImm("simm5"))
        Off = *V;
      else if (auto V = findFieldImm("uimm5"))
        Off = *V;

      if (!Off)
        return false;

      const int64_t Scale = scaleFromMnemonic();
      OS << (*Off * Scale);
      return true;
    };

    const bool IsRegOffset = findField("SrcRType") && findField("SrcR");

    if (Mnem == "C.SWI" || Mnem == "C.SDI") {
      // Compressed stores: implicit value `t#1`.
      OS << "t#1, [";
      emitReg("SrcL"); // base
      OS << ", ";
      if (!emitScaledImmOff())
        OS << "0";
      OS << "]";
      printAnnotation(OS, Annot);
      return;
    }

    if (HasDest) {
      // Loads: `[base, off]` / `[base, idx<type><<shamt>] , ->dst`.
      OS << "[";
      emitReg("SrcL"); // base
      OS << ", ";
      if (IsRegOffset) {
        emitSrcRWithTypeAndShift(/*ForcedShift=*/std::nullopt);
      } else {
        if (!emitScaledImmOff())
          OS << "0";
      }
      OS << "]";
      emitArrowDest();
      printAnnotation(OS, Annot);
      return;
    }

    // Stores.
    if (findField("SrcD")) {
      // Reg-offset stores: `SrcD, [SrcL, SrcR<type><<k>]`.
      emitReg("SrcD");
      OS << ", [";
      emitReg("SrcL"); // base
      OS << ", ";
      std::optional<int64_t> ForcedShift;
      if (Mnem == "SH")
        ForcedShift = 1;
      else if (Mnem == "SW")
        ForcedShift = 2;
      else if (Mnem == "SD")
        ForcedShift = 3;
      emitSrcRWithTypeAndShift(ForcedShift);
      OS << "]";
      printAnnotation(OS, Annot);
      return;
    }

    // Imm-offset stores: `SrcL, [SrcR, off]`.
    emitReg("SrcL"); // value
    OS << ", [";
    emitReg("SrcR"); // base
    OS << ", ";
    if (!emitScaledImmOff())
      OS << "0";
    OS << "]";
    printAnnotation(OS, Annot);
    return;
  }

  // Pretty printer for the common "SrcL, SrcR<type><<shamt>" operand form.
  if (Form.mnemonic && StringRef(Form.mnemonic).equals_insensitive("CSEL") &&
      findField("SrcP") && findField("SrcL") && findField("SrcR") &&
      findField("SrcRType")) {
    OS << mnemonicTok("<op>");
    OS << "\t";
    emitReg("SrcP");
    OS << ", ";
    emitReg("SrcL");
    OS << ", ";
    emitSrcRWithTypeAndShift(/*ForcedShift=*/std::nullopt);
    if (AsmFmt.contains("->"))
      emitArrowDest();
    printAnnotation(OS, Annot);
    return;
  }

  // Pretty printer for the common "SrcL, SrcR<type><<shamt>" operand form.
  if (findField("SrcL") && findField("SrcR") && findField("SrcRType") &&
      !findField("SrcP") && !findField("SrcD") && !findField("SrcA") &&
      !AsmFmt.contains('[')) {
    OS << mnemonicTok("<op>");
    OS << "\t";
    emitReg("SrcL");
    OS << ", ";
    emitSrcRWithTypeAndShift(/*ForcedShift=*/std::nullopt);
    if (AsmFmt.contains("->"))
      emitArrowDest();
    printAnnotation(OS, Annot);
    return;
  }

  // Generic printer: best-effort by listing fields in common ISA order.
  if (!AsmFmt.empty())
    OS << mnemonicTok("<invalid>");
  else if (Form.mnemonic && Form.mnemonic[0])
    OS << StringRef(Form.mnemonic).lower();
  else
    OS << "<invalid>";

  const bool IsSetcImm =
      AsmFmt.starts_with_insensitive("setc.") && findField("shamt") &&
      (findField("simm12") || findField("uimm12"));

  bool FirstOp = true;
  auto sep = [&]() {
    OS << (FirstOp ? "\t" : ", ");
    FirstOp = false;
  };

  // Sources (common ones).
  for (StringRef R : {"SrcL", "SrcR", "SrcD", "SrcP", "SrcA"}) {
    if (findField(R)) {
      sep();
      emitReg(R);
    }
  }

  // Common immediates / shift amounts.
  for (auto &KV : Fields) {
    StringRef Name = KV.first;
    if (Name == "RegDst" || Name == "SrcL" || Name == "SrcR" || Name == "SrcD" ||
        Name == "SrcP" || Name == "SrcA")
      continue;
    if (IsSetcImm &&
        (Name.equals_insensitive("shamt") || Name.equals_insensitive("simm12") ||
         Name.equals_insensitive("uimm12")))
      continue;
    if (Name.starts_with_insensitive("imm") ||
        Name.starts_with_insensitive("simm") ||
        Name.starts_with_insensitive("uimm") ||
        Name.starts_with_insensitive("shamt") || Name.equals_insensitive("BrType") ||
        Name.equals_insensitive("BlockType") || Name.equals_insensitive("SSR_ID") ||
        Name.equals_insensitive("SSRID")) {
      sep();
      const MCOperand &Op = KV.second;
      if (Op.isImm()) {
        if (Name.equals_insensitive("SSR_ID") || Name.equals_insensitive("SSRID")) {
          if (const char *Sym = ssrIdSymbol(static_cast<uint64_t>(Op.getImm()))) {
            OS << Sym;
          } else {
            OS << "0x" << utohexstr(static_cast<uint64_t>(Op.getImm()) & 0xfffu);
          }
        } else {
          OS << Op.getImm();
        }
      }
      else if (Op.isExpr())
        MAI.printExpr(OS, *Op.getExpr());
    }
  }

  // SETC.*I encodes an immediate as (simm12/uimm12) << shamt. Print the
  // computed value rather than the raw fields.
  if (IsSetcImm) {
    unsigned Shamt = 0;
    if (auto V = findFieldImm("shamt"))
      Shamt = static_cast<unsigned>(*V) & 0x1f;
    if (auto Op = findField("simm12")) {
      sep();
      if (Op->isImm())
        OS << shlSigned64(Op->getImm(), Shamt);
      else if (Op->isExpr())
        MAI.printExpr(OS, *Op->getExpr());
    } else if (auto Op = findField("uimm12")) {
      sep();
      if (Op->isImm())
        OS << shlUnsigned64(static_cast<uint64_t>(Op->getImm()), Shamt);
      else if (Op->isExpr())
        MAI.printExpr(OS, *Op->getExpr());
    }
  }

  // Destination (arrow syntax).
  auto dstSep = [&]() {
    OS << (FirstOp ? "\t" : ",\t");
    FirstOp = false;
  };

  if (auto Op = findField("RegDst")) {
    dstSep();
    if (Op->isImm()) {
      unsigned Code = static_cast<unsigned>(Op->getImm());
      if (IsVector) {
        OS << "->";
        printReg10Name(OS, Code);
      } else {
        Code &= 0x1F;
        if (Code == 31)
          OS << "->t";
        else if (Code == 30)
          OS << "->u";
        else
          OS << "->" << reg5Name(Code);
      }
    }
  } else if (!AsmFmt.empty()) {
    if (asmImpliesArrowDest(AsmFmt, "->t")) {
      dstSep();
      OS << "->t";
    } else if (asmImpliesArrowDest(AsmFmt, "->u")) {
      dstSep();
      OS << "->u";
    } else if (asmImpliesArrowDest(AsmFmt, "->ra")) {
      dstSep();
      OS << "->ra";
    }
  }

  printAnnotation(OS, Annot);
}
