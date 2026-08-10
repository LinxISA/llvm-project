#include "MCTargetDesc/LinxISAInstPrinter.h"
#include "LinxISATileEnginesV058.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
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
  if (Code == 92u) {
    OS << "p";
    return;
  }
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
  const unsigned Hand = (TileId >> 4) & 0x3u;
  const unsigned Depth = TileId & 0xfu;
  const char Prefix = (Hand == 0)   ? 't'
                      : (Hand == 1) ? 'u'
                      : (Hand == 2) ? 'm'
                                    : 'n';
  OS << Prefix << "#" << utostr(Depth + 1u);
}

static StringRef dtypeName(unsigned DT) {
  switch (DT & 0x1f) {
  case 0:
    return "FP64";
  case 1:
    return "FP32";
  case 2:
    return "TF32";
  case 3:
    return "HF32";
  case 4:
    return "FP16";
  case 5:
    return "BF16";
  case 6:
    return "HiF8";
  case 7:
    return "E4M3";
  case 8:
    return "E5M2";
  case 9:
    return "E3M2";
  case 10:
    return "E2M3";
  case 11:
    return "E2M1X2";
  case 12:
    return "E1M2X2";
  case 13:
    return "E8M0";
  case 14:
    return "HiF4X2";
  case 16:
    return "S64";
  case 17:
    return "S32";
  case 18:
    return "S16";
  case 19:
    return "S8";
  case 20:
    return "S4X2";
  case 24:
    return "U64";
  case 25:
    return "U32";
  case 26:
    return "U16";
  case 27:
    return "U8";
  case 28:
    return "U4X2";
  default:
    return StringRef();
  }
}

static unsigned tlsuStateOpFromFunction(unsigned Func) {
  switch (Func & 0x1fu) {
  case 0:
    return 33u;
  case 1:
    return 65u;
  default:
    return 32u + (Func & 0x1fu);
  }
}

static unsigned cubeStateOpFromFunction(unsigned Func) {
  switch (Func & 0x1fu) {
  case 0:
    return 2u;
  case 2:
    return 66u;
  case 8:
    return 258u;
  default:
    return Func & 0x1fu;
  }
}

static StringRef layoutFormatName(unsigned Format) {
  switch (Format & 0x1fu) {
  case 0:
    return "NORM.normal";
  case 2:
    return "ND2NZ.normal";
  case 3:
    return "ND2ZN.normal";
  case 8:
    return "DN2ZN.normal";
  case 9:
    return "DN2NZ.normal";
  case 28:
    return "NZ2DN.canon";
  default:
    return StringRef();
  }
}

static StringRef vectorBlockModeName(unsigned Mode) {
  switch (Mode & 0x3u) {
  case 0:
    return "VS8";
  case 1:
    return "VS16";
  default:
    return StringRef();
  }
}

static StringRef padValueName(unsigned Pad) {
  switch (Pad & 0x1fu) {
  case 0:
    return "Null";
  case 1:
    return "Zero";
  case 2:
    return "Max";
  case 3:
    return "Min";
  default:
    return StringRef();
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
  // SSR_ID is encoded as a 12-bit field in the base forms
  // (SSRGET/SSRSET/SSRSWAP). Keep this mapping aligned with isa.txt (and the
  // ISA manual SSR table).
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
  const StringRef RawTok =
      AsmFmt.empty() ? StringRef() : AsmFmt.split(' ').first.rtrim(",");

  auto stripAngleSuffix = [&](StringRef Tok) -> StringRef {
    if (size_t Pos = Tok.find('<'); Pos != StringRef::npos)
      Tok = Tok.take_front(Pos);
    return Tok;
  };

  const bool IsVector = Form.length_bits == 64 &&
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

    auto emitPrefetchModel = [&](unsigned Model) {
      switch (Model & 0x1fu) {
      case 0:
        PrintedMnemonicTok += ".l1";
        break;
      case 1:
        PrintedMnemonicTok += ".l2";
        break;
      case 2:
        PrintedMnemonicTok += ".l3";
        break;
      default:
        PrintedMnemonicTok += ".m";
        PrintedMnemonicTok += utostr(Model & 0x1fu);
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

    if (Tok.contains("{st2dt}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);

      unsigned SrcType = 0;
      if (auto V = findFieldImm("SrcType"))
        SrcType = static_cast<unsigned>(*V);

      unsigned DstType = 0;
      if (auto V = findFieldImm("DstType"))
        DstType = static_cast<unsigned>(*V);

      StringRef Mnem = Form.mnemonic ? StringRef(Form.mnemonic) : StringRef();
      if (Mnem.equals_insensitive("V.FCVTI")) {
        emitFpT(SrcType);
        PrintedMnemonicTok += "2";
        emitCvtIntDst(DstType);
      } else if (Mnem.equals_insensitive("V.ICVT")) {
        emitCvtIntSrc(SrcType, /*Signed=*/true);
        PrintedMnemonicTok += "2";
        emitCvtIntDst(DstType);
      } else if (Mnem.equals_insensitive("V.ICVTF")) {
        emitCvtIntSrc(SrcType, /*Signed=*/true);
        PrintedMnemonicTok += "2";
        emitCvtFpDst(DstType);
      } else {
        emitFpT(SrcType);
        PrintedMnemonicTok += "2";
        emitCvtFpDst(DstType);
      }
      return PrintedMnemonicTok;
    }

    if (Tok.contains("{.l1,.l2,.l3}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);
      const unsigned Model =
          static_cast<unsigned>(findFieldImm("model").value_or(0)) & 0x1fu;
      emitPrefetchModel(Model);
      return PrintedMnemonicTok;
    }

    if (Tok.contains("{i,e,s,r,ie,is,ir,es,er,ies,ier}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);
      if ((findFieldImm("i").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "i";
      if ((findFieldImm("e").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "e";
      if ((findFieldImm("s").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "s";
      if ((findFieldImm("r").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "r";
      return PrintedMnemonicTok;
    }

    if (Tok.contains("{e,r,er}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);
      if ((findFieldImm("e").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "e";
      if ((findFieldImm("r").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "r";
      return PrintedMnemonicTok;
    }

    if (Tok.contains("{h,e,r,he,hr,er,her}")) {
      const size_t Pos = Tok.find('{');
      PrintedMnemonicTok = Tok.take_front(Pos);
      if ((findFieldImm("h").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "h";
      if ((findFieldImm("e").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "e";
      if ((findFieldImm("r").value_or(0) & 0x1) != 0)
        PrintedMnemonicTok += "r";
      return PrintedMnemonicTok;
    }

    // Canonical PTO 0.58 vector bridge: `<.local>` is a mnemonic qualifier
    // driven by the L bit, while bridged/global forms stay on the base
    // mnemonic.
    if (AsmFmt.contains("<.local>")) {
      if (auto L = findFieldImm("L")) {
        if (((*L) & 1) != 0) {
          PrintedMnemonicTok += Tok;
          PrintedMnemonicTok += ".local";
          return PrintedMnemonicTok;
        }
      }
    }

    if (AsmFmt.contains("<.{")) {
      const unsigned Aq =
          static_cast<unsigned>(findFieldImm("aq").value_or(0)) & 0x1u;
      const unsigned Rl =
          static_cast<unsigned>(findFieldImm("rl").value_or(0)) & 0x1u;
      const unsigned Far =
          static_cast<unsigned>(findFieldImm("far").value_or(0)) & 0x1u;
      const unsigned Rd =
          static_cast<unsigned>(findFieldImm("rd").value_or(0)) & 0x1u;

      SmallString<8> Qual;
      if (findField("aq")) {
        if (Aq)
          Qual += "aq";
        if (Rl)
          Qual += "rl";
        if (Far)
          Qual += "f";
      } else {
        if (Rl)
          Qual += "rl";
        if (Rd)
          Qual += "rd";
        if (Far)
          Qual += "f";
      }

      if (!Qual.empty()) {
        PrintedMnemonicTok += Tok;
        PrintedMnemonicTok += ".";
        PrintedMnemonicTok += Qual;
        return PrintedMnemonicTok;
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
      uint64_t Delta =
          shlUnsigned64(static_cast<uint64_t>(Op->getImm()), Shift);
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
    // HL.BSTART uses an instruction-aligned byte offset (simm[0] is implicit
    // 0).
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
  // PTO 0.58 keeps TEPL as the binary carrier and prints its named VEC/SFU
  // execution-engine alias. TLSU and CUBE retain their direct start names.
  const bool IsTypedTEPL = AsmFmt.starts_with("BSTART.TEPL");
  const bool IsTypedVPAR = AsmFmt.starts_with("BSTART.VPAR");
  const bool IsTypedVSEQ = AsmFmt.starts_with("BSTART.VSEQ");
  const bool IsTypedMPAR = AsmFmt.starts_with("BSTART.MPAR");
  const bool IsTypedMSEQ = AsmFmt.starts_with("BSTART.MSEQ");
  const StringRef TypedTok = stripAngleSuffix(RawTok);
  const std::optional<unsigned> DirectTLSUFunc =
      StringSwitch<std::optional<unsigned>>(TypedTok)
          .Case("BSTART.TLOAD", 0u)
          .Case("BSTART.TSTORE", 1u)
          .Case("BSTART.TMOV", 2u)
          .Case("BSTART.TPREFETCH", 3u)
          .Case("BSTART.MGATHER", 4u)
          .Case("BSTART.MSCATTER", 5u)
          .Case("BSTART.MGATHER.MASK", 6u)
          .Case("BSTART.MSCATTER.MASK", 7u)
          .Case("BSTART.MGATHER.CAS", 8u)
          .Default(std::nullopt);
  const std::optional<unsigned> DirectCUBEFunc =
      StringSwitch<std::optional<unsigned>>(TypedTok)
          .Case("BSTART.TMATMUL", 0u)
          .Case("BSTART.TMATMUL.BIAS", 1u)
          .Case("BSTART.TMATMUL.ACC", 2u)
          .Case("BSTART.TMATMULMX", 4u)
          .Case("BSTART.TMATMULMX.BIAS", 5u)
          .Case("BSTART.TMATMULMX.ACC", 6u)
          .Case("BSTART.TGEMV", 16u)
          .Case("BSTART.TGEMV.BIAS", 17u)
          .Case("BSTART.TGEMV.ACC", 18u)
          .Case("BSTART.TGEMVMX", 20u)
          .Case("BSTART.TGEMVMX.BIAS", 21u)
          .Case("BSTART.TGEMVMX.ACC", 22u)
          .Default(std::nullopt);
  const bool IsDirectTLSUAlias = DirectTLSUFunc.has_value();
  const bool IsDirectCUBEAlias = DirectCUBEFunc.has_value();
  if (IsTypedTEPL || IsTypedVPAR || IsTypedVSEQ || IsTypedMPAR || IsTypedMSEQ ||
      IsDirectTLSUAlias || IsDirectCUBEAlias) {
    const unsigned DT = static_cast<unsigned>(
                            findFieldImm("DataType")
                                .value_or(findFieldImm("dtype").value_or(0))) &
                        0x1fu;

    auto printDataType = [&]() {
      if (StringRef DTName = dtypeName(DT); !DTName.empty())
        OS << DTName;
      else
        OS << "DT" << utostr(DT);
    };

    unsigned ParStateOp = 0u;

    if (IsDirectTLSUAlias || IsDirectCUBEAlias) {
      OS << TypedTok << "\t";
      printDataType();

      if (IsDirectTLSUAlias) {
        LastTileHeader = LastTileHeaderKind::TLSU;
        ParStateOp = tlsuStateOpFromFunction(*DirectTLSUFunc);
      } else if (IsDirectCUBEAlias) {
        LastTileHeader = LastTileHeaderKind::CUBE;
        ParStateOp = cubeStateOpFromFunction(*DirectCUBEFunc);
      }

      LastParTileOp = ParStateOp;
      LastParTileOpValid = true;
      printAnnotation(OS, Annot);
      return;
    }

    if (IsTypedVPAR || IsTypedVSEQ || IsTypedMPAR || IsTypedMSEQ) {
      const unsigned Mode =
          static_cast<unsigned>(findFieldImm("Mode").value_or(0)) & 0x3u;
      OS << TypedTok << "\t";
      if (StringRef Name = vectorBlockModeName(Mode); !Name.empty())
        OS << Name;
      else
        OS << utostr(Mode);
      LastParTileOp = 0u;
      LastParTileOpValid = false;
      LastTileHeader = LastTileHeaderKind::None;
      printAnnotation(OS, Annot);
      return;
    }

    const unsigned Mode =
        static_cast<unsigned>(findFieldImm("Mode").value_or(0)) & 0x3u;
    const unsigned Function =
        static_cast<unsigned>(findFieldImm("Function").value_or(0)) & 0x1fu;
    const unsigned Selector = (Mode << 5) | Function;
    ParStateOp = Selector;

    StringRef OperationName = LinxISA::canonicalTileOperationNameV058(Selector);
    auto Engine = LinxISA::tileEngineV058(Selector);
    LastTileHeader = LastTileHeaderKind::TILEOP;

    if (!OperationName.empty() && Engine) {
      OS << LinxISA::tileOperationAssemblyAliasV058(*Engine) << "\t"
         << OperationName << ", ";
      printDataType();
    } else {
      OS << "BSTART.TEPL\t" << utostr(Mode) << ", " << utostr(Function) << ", ";
      printDataType();
    }

    LastParTileOp = ParStateOp;
    LastParTileOpValid = true;
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: block split instructions. LLVM uses these as block
  // terminators; Linx uses halfword PC-relative offsets.
  auto emitFusedReturnTarget = [&](uint64_t PcBaseOffset) -> bool {
    auto Op = findField("uimm5");
    if (!Op)
      return false;
    if (Op->isExpr()) {
      MAI.printExpr(OS, *Op->getExpr());
      return true;
    }
    if (!Op->isImm())
      return false;
    OS << "0x"
       << utohexstr(Address + PcBaseOffset +
                        (static_cast<uint64_t>(Op->getImm()) << 1),
                    /*LowerCase=*/true);
    return true;
  };

  if (AsmFmt.starts_with("BSTART.CALL")) {
    OS << "BSTART.CALL\t";
    if (!emitPcRelTargetHex("simm12", /*Signed=*/true))
      OS << "0x0";
    OS << ", ";
    if (!emitFusedReturnTarget(/*PcBaseOffset=*/2))
      OS << "0x0";
    OS << ",\t->ra";
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("HL.BSTART.CALL")) {
    OS << "HL.BSTART.CALL\t";
    if (!emitPcRelTargetHex("simm25", /*Signed=*/true))
      OS << "0x0";
    OS << ", ";
    if (!emitFusedReturnTarget(/*PcBaseOffset=*/4))
      OS << "0x0";
    OS << ",\t->ra";
    printAnnotation(OS, Annot);
    return;
  }

  const bool IsCBSTART = AsmFmt.starts_with("C.BSTART");
  const bool IsLBSTART = AsmFmt.starts_with("L.BSTART");
  const bool IsBSTART =
      AsmFmt.starts_with("HL.BSTART") || IsLBSTART ||
      AsmFmt.starts_with("BSTART.STD") || AsmFmt.starts_with("BSTART.FP") ||
      AsmFmt.starts_with("BSTART.SYS") || AsmFmt.starts_with("BSTART.") ||
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
    } else if (FirstTokBase == "BSTART" && (AsmFmt.contains("{DIRECT, CALL}") ||
                                            AsmFmt.contains(" COND"))) {
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
      if (IsLBSTART) {
        if (auto Op = findField("simm")) {
          if (Op->isExpr())
            MAI.printExpr(OS, *Op->getExpr());
          else if (Op->isImm())
            OS << shlSigned64(Op->getImm(), /*Shift=*/1);
        }
      } else {
        emitBlockTarget();
      }
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
      else if (auto V = findFieldImm("simm"))
        Off = *V;
      const bool HasExprTarget =
          IsLBSTART && findField("simm") && findField("simm")->isExpr();
      if (Off != 0 || HasExprTarget)
        emitKindAndLabel("FALL");
      else if (IsLBSTART)
        emitKind("FALL");
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

    // Non-tile block starts terminate any tile-header descriptor context used
    // by B.IOT pretty-printing.
    LastParTileOp = 0;
    LastParTileOpValid = false;
    LastTileHeader = LastTileHeaderKind::None;

    printAnnotation(OS, Annot);
    return;
  }

  // Decoupled-body PC-relative target.
  if (AsmFmt.starts_with("B.TEXT")) {
    OS << "B.TEXT\t";
    if (!emitPcRelTargetHex("simm25", /*Signed=*/true))
      OS << "0x0";
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
    // Multi-dest forms: `->dst0, dst1`.
    if (auto Op0 = findField("RegDst0")) {
      if (Op0->isImm()) {
        if (auto Op1 = findField("RegDst1"); Op1 && Op1->isImm()) {
          // Today all multi-dest forms are scalar (5-bit GPRs).
          unsigned Code0 = static_cast<unsigned>(Op0->getImm()) & 0x1fu;
          unsigned Code1 = static_cast<unsigned>(Op1->getImm()) & 0x1fu;
          OS << ",\t->" << reg5Name(Code0) << ", " << reg5Name(Code1);
          return;
        }
      }
    }

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
    StringRef Tok =
        Form.mnemonic ? StringRef(Form.mnemonic) : StringRef("FENTRY");
    OS << Tok;
    OS << "\t[";

    // Get register range from SrcBegin/SrcEnd or DstBegin/DstEnd fields
    unsigned RegBegin = 10, RegEnd = 14; // defaults: ra ~ s2
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
    const unsigned Dst =
        static_cast<unsigned>(findFieldImm("RegDst").value_or(0)) & 0x1fu;
    const unsigned Src0 =
        static_cast<unsigned>(findFieldImm("RegSrc0").value_or(0)) & 0x1fu;
    const unsigned Src1 =
        static_cast<unsigned>(findFieldImm("RegSrc1").value_or(0)) & 0x1fu;
    const unsigned Src2 =
        static_cast<unsigned>(findFieldImm("RegSrc2").value_or(0)) & 0x1fu;

    OS << "B.IOR\t[" << reg5Name(Src0) << ", " << reg5Name(Src1) << ", "
       << reg5Name(Src2) << "], ->" << reg5Name(Dst);
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("B.IOS")) {
    const unsigned PEMask =
        static_cast<unsigned>(findFieldImm("PE_MASK").value_or(0)) & 0xfu;
    const unsigned SharedTID =
        static_cast<unsigned>(findFieldImm("SharedTID").value_or(0)) & 0xffu;
    const unsigned TSize =
        static_cast<unsigned>(findFieldImm("TSize").value_or(0)) & 0x7u;

    auto printPEMask = [&]() {
      for (int Bit = 3; Bit >= 0; --Bit)
        OS << ((PEMask >> Bit) & 1u);
    };
    OS << "B.IOS\t";
    if (TSize == 0) {
      OS << "S" << SharedTID << ", mask=";
      printPEMask();
    } else {
      OS << "mask=";
      printPEMask();
      OS << ", ->S" << SharedTID << "<";
      const uint64_t Bytes = 1ull << (TSize + 6u);
      if (Bytes >= 1024u)
        OS << (Bytes / 1024u) << "KB";
      else
        OS << Bytes << "B";
      OS << ">";
    }
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("ERCOV") || AsmFmt.starts_with("ESAVE") ||
      AsmFmt.starts_with("MCOPY") || AsmFmt.starts_with("MSET")) {
    const unsigned Src0 =
        static_cast<unsigned>(findFieldImm("RegSrc0").value_or(
            findFieldImm("RegSrc0=BasePtr")
                .value_or(findFieldImm("RegSrc0=DstAddr").value_or(0)))) &
        0x1fu;
    const unsigned Src1 =
        static_cast<unsigned>(findFieldImm("RegSrc1").value_or(
            findFieldImm("RegSrc1=LenBytes")
                .value_or(
                    findFieldImm("RegSrc1=SrcAddr")
                        .value_or(
                            findFieldImm("RegSrc1=Value").value_or(0))))) &
        0x1fu;
    const unsigned Src2 =
        static_cast<unsigned>(findFieldImm("RegSrc2").value_or(
            findFieldImm("RegSrc2=Kind")
                .value_or(findFieldImm("RegSrc2=Size").value_or(0)))) &
        0x1fu;
    OS << mnemonicTok("<op>") << "\t[";
    OS << reg5Name(Src0) << ", " << reg5Name(Src1) << ", " << reg5Name(Src2)
       << "]";
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("XB ")) {
    const unsigned AcrId =
        static_cast<unsigned>(findFieldImm("ACR-ID").value_or(0)) & 0x3ffu;
    const unsigned CrossBid =
        static_cast<unsigned>(findFieldImm("CROSS-BID").value_or(0)) & 0x3ffu;
    OS << "XB\t" << AcrId << ", " << CrossBid;
    printAnnotation(OS, Annot);
    return;
  }

  // Special-case: canonical PTO 0.58 B.IOT descriptors.
  if (AsmFmt.starts_with("B.IOT")) {
    const unsigned DstTile =
        static_cast<unsigned>(findFieldImm("DstTile").value_or(0)) & 0x3u;
    const unsigned PEMask =
        static_cast<unsigned>(findFieldImm("PE_MASK").value_or(0)) & 0xfu;
    const unsigned Src0 =
        static_cast<unsigned>(findFieldImm("SrcTile0").value_or(0)) & 0x3fu;
    const unsigned Src1 =
        static_cast<unsigned>(findFieldImm("SrcTile1").value_or(0)) & 0x3fu;
    const unsigned TSize =
        static_cast<unsigned>(findFieldImm("TSize").value_or(0)) & 0x7u;
    const bool Src0Present = AsmFmt.contains("SrcTile0");
    const bool Src1Present = AsmFmt.contains("SrcTile1");
    const bool HasDestination = AsmFmt.contains("->DstTile");

    OS << "B.IOT\t";
    bool First = true;
    if (Src0Present) {
      printTileRef(OS, Src0);
      First = false;
    }
    if (Src1Present) {
      if (!First)
        OS << ", ";
      printTileRef(OS, Src1);
      First = false;
    }

    if (!First)
      OS << ", ";
    OS << "mask=";
    for (int Bit = 3; Bit >= 0; --Bit)
      OS << ((PEMask >> Bit) & 1u);
    First = false;

    if ((static_cast<unsigned>(findFieldImm("L").value_or(0)) & 1u) != 0u) {
      if (!First)
        OS << ", ";
      OS << "last";
      First = false;
    }

    if (!HasDestination) {
      printAnnotation(OS, Annot);
      return;
    }
    if (!First)
      OS << ", ";

    static constexpr const char *DstKinds[] = {"t", "u", "m", "n"};
    OS << "->" << DstKinds[DstTile] << "<";
    const uint64_t Bytes = 1ull << (TSize + 6u);
    if (Bytes >= 1024u)
      OS << utostr(static_cast<unsigned>(Bytes / 1024u)) << "KB";
    else
      OS << utostr(static_cast<unsigned>(Bytes)) << "B";
    OS << ">";

    printAnnotation(OS, Annot);
    return;
  }

  // Canonical PTO 0.58 control attributes.
  if (AsmFmt.starts_with("B.CATR")) {
    const unsigned Trap =
        static_cast<unsigned>(findFieldImm("trap").value_or(0)) & 0x1u;
    const unsigned DR = static_cast<unsigned>(findFieldImm("DR").value_or(
                            findFieldImm("dr").value_or(0))) &
                        0x1u;
    const unsigned Aq = static_cast<unsigned>(findFieldImm("aq").value_or(
                            findFieldImm("AQ").value_or(0))) &
                        0x1u;
    const unsigned Atom = static_cast<unsigned>(findFieldImm("atom").value_or(
                              findFieldImm("atomic").value_or(0))) &
                          0x1u;
    const unsigned Far = static_cast<unsigned>(findFieldImm("far").value_or(
                             findFieldImm("FAR").value_or(0))) &
                         0x1u;
    const unsigned Rl = static_cast<unsigned>(findFieldImm("rl").value_or(
                            findFieldImm("RL").value_or(0))) &
                        0x1u;

    StringRef OrderName;
    if (Aq && Rl)
      OrderName = "aqrl";
    else if (Aq)
      OrderName = "aq";
    else if (Rl)
      OrderName = "rl";

    OS << "B.CATR";
    bool First = true;
    auto emitToken = [&](StringRef Token) {
      OS << (First ? "\t" : ", ") << Token;
      First = false;
    };
    if (Trap)
      emitToken("trap");
    if (Atom)
      emitToken("atomic");
    if (!OrderName.empty())
      emitToken(OrderName);
    if (Far)
      emitToken("far");
    if (DR)
      emitToken("dr");

    printAnnotation(OS, Annot);
    return;
  }

  // Canonical PTO 0.58 data attributes.
  if (AsmFmt.starts_with("B.DATR")) {
    const unsigned CMode =
        static_cast<unsigned>(findFieldImm("CMode").value_or(0)) & 0x7u;
    const unsigned Layout =
        static_cast<unsigned>(findFieldImm("Layout").value_or(0)) & 0x1fu;
    const unsigned DType =
        static_cast<unsigned>(findFieldImm("DataType").value_or(0)) & 0x1fu;
    const unsigned Pad =
        static_cast<unsigned>(findFieldImm("PadValueOrByteId").value_or(0)) &
        0x3u;
    const unsigned RMode =
        static_cast<unsigned>(findFieldImm("RMode").value_or(0)) & 0x7u;
    const unsigned Sat =
        static_cast<unsigned>(findFieldImm("Sat").value_or(0)) & 0x1u;
    const unsigned Canonicalize =
        static_cast<unsigned>(findFieldImm("Canonicalize").value_or(0)) & 0x1u;
    const StringRef DTName = dtypeName(DType);
    const StringRef PadName = padValueName(Pad);

    OS << "B.DATR\t";
    if (StringRef LayoutName = layoutFormatName(Layout); !LayoutName.empty())
      OS << LayoutName;
    else
      OS << "Layout" << utostr(Layout);
    OS << ", ";
    if (!DTName.empty())
      OS << DTName;
    else
      OS << "DT" << utostr(DType);
    OS << ", ";
    if (!PadName.empty())
      OS << PadName;
    else
      OS << "Pad" << utostr(Pad);
    OS << ", cmode" << CMode << ", rmode" << RMode;
    if (Sat)
      OS << ", sat";
    if (Canonicalize)
      OS << ", canonicalize";
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
  if (AsmFmt.starts_with("C.B.DIM ") ||
      AsmFmt == "C.B.DIM RegSrc, ->{LB0, LB1, LB2}") {
    const unsigned LoopNest =
        static_cast<unsigned>(findFieldImm("LoopNest").value_or(0)) & 0x3u;
    const unsigned Reg =
        static_cast<unsigned>(findFieldImm("RegSrc").value_or(0)) & 0x1fu;
    OS << "C.B.DIM\t" << reg5Name(Reg) << ", \t->lb" << utostr(LoopNest);
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("C.B.DIMI")) {
    const unsigned LoopNest =
        static_cast<unsigned>(findFieldImm("LoopNest").value_or(0)) & 0x3u;
    const unsigned Imm =
        static_cast<unsigned>(findFieldImm("imm8").value_or(0)) & 0xffu;
    OS << "C.B.DIMI\t" << Imm << ", \t->lb" << utostr(LoopNest);
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("B.HINT TRACE")) {
    const unsigned Begin =
        static_cast<unsigned>(findFieldImm("B/E").value_or(0)) & 0x1u;
    OS << "B.HINT\tTRACE." << (Begin ? "begin" : "end");
    printAnnotation(OS, Annot);
    return;
  }

  if (AsmFmt.starts_with("B.HINT ")) {
    const unsigned Likely =
        static_cast<unsigned>(findFieldImm("L/UL").value_or(0)) & 0x1u;
    const unsigned Temp =
        static_cast<unsigned>(findFieldImm("temp").value_or(0)) & 0x3u;
    const unsigned Prefetch =
        static_cast<unsigned>(findFieldImm("prefetch_size").value_or(0)) &
        0xfffu;
    const char *TempName = "hot";
    switch (Temp) {
    case 1:
      TempName = "warm";
      break;
    case 2:
      TempName = "cool";
      break;
    case 3:
      TempName = "none";
      break;
    default:
      TempName = "hot";
      break;
    }
    OS << "B.HINT\t{BR." << (Likely ? "likely" : "unlikely") << ", TEMP."
       << TempName << ", " << Prefetch << "}";
    printAnnotation(OS, Annot);
    return;
  }

  // Pretty printer for memory operands.
  if (AsmFmt.contains('[')) {
    const StringRef Mnem =
        Form.mnemonic ? StringRef(Form.mnemonic) : StringRef();
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
            OS << "0x"
               << utohexstr(static_cast<uint64_t>(Op->getImm()),
                            /*LowerCase=*/true);
          OS << "]";
          return true;
        }
        if (auto Op = findField("simm")) {
          if (Op->isExpr())
            MAI.printExpr(OS, *Op->getExpr());
          else
            OS << "0x"
               << utohexstr(static_cast<uint64_t>(Op->getImm()),
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

    if (IsVector) {
      // Vector/SIMT memory ops: `[base, lc0<<k, idx|imm]`.
      StringRef BaseField = "SrcL";
      bool VecUsesIndex = false;
      if (size_t L = AsmFmt.find('['); L != StringRef::npos) {
        StringRef Inside = AsmFmt.substr(L + 1).split(']').first;
        StringRef BasePart = Inside.split(',').first.trim();
        if (BasePart.starts_with_insensitive("srcr"))
          BaseField = "SrcR";
        else if (BasePart.starts_with_insensitive("srcl"))
          BaseField = "SrcL";
        VecUsesIndex = findField("SrcR") &&
                       !Inside.contains_insensitive("simm") &&
                       !Inside.contains_insensitive("uimm");
      }

      std::optional<StringRef> ValueField;
      StringRef Prefix = AsmFmt;
      if (size_t L = Prefix.find('['); L != StringRef::npos)
        Prefix = Prefix.take_front(L);
      if (Prefix.contains_insensitive("SrcD"))
        ValueField = "SrcD";
      else if (Prefix.contains_insensitive("SrcL"))
        ValueField = "SrcL";
      const bool HasDest = AsmFmt.contains("->") ||
                           (!ValueField.has_value() && findField("RegDst"));

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
        if (!VecUsesIndex || !findField("SrcR"))
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
      StringRef Up = Mnem;
      if (Up.ends_with(".LOCAL"))
        Up = Up.drop_back(StringRef(".LOCAL").size());

      // Unscaled byte offsets.
      if (Up.ends_with(".U") || Up.ends_with(".UPO") || Up.ends_with(".UPR"))
        return 1;

      if (Up.starts_with("HL."))
        Up = Up.drop_front(3);
      if (Up.starts_with("C."))
        Up = Up.drop_front(2);
      StringRef Op = Up.split('.').first;

      if (Op == "LBI" || Op == "LBUI" || Op == "SBI" || Op == "LBIP" ||
          Op == "LBUIP" || Op == "SBIP")
        return 1;
      if (Op == "LHI" || Op == "LHUI" || Op == "SHI" || Op == "LHIP" ||
          Op == "LHUIP" || Op == "SHIP")
        return 2;
      if (Op == "LWI" || Op == "LWUI" || Op == "SWI" || Op == "LWIP" ||
          Op == "LWUIP" || Op == "SWIP")
        return 4;
      if (Op == "LDI" || Op == "SDI" || Op == "LDIP" || Op == "SDIP")
        return 8;

      return 1;
    };

    auto emitScaledImmOff = [&]() -> bool {
      std::optional<int64_t> Off;
      if (auto V = findFieldImm("simm17"))
        Off = *V;
      else if (auto V = findFieldImm("uimm17"))
        Off = *V;
      else if (auto V = findFieldImm("simm22"))
        Off = *V;
      else if (auto V = findFieldImm("uimm22"))
        Off = *V;
      else if (auto V = findFieldImm("simm12"))
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

    auto baseFieldFromAsmFmt = [&]() -> StringRef {
      StringRef BaseField = "SrcL";
      if (size_t L = AsmFmt.find('['); L != StringRef::npos) {
        StringRef Inside = AsmFmt.substr(L + 1).split(']').first;
        StringRef BasePart = Inside.split(',').first.trim();
        if (BasePart.starts_with_insensitive("srcr"))
          BaseField = "SrcR";
        else if (BasePart.starts_with_insensitive("srcl"))
          BaseField = "SrcL";
      }
      return BaseField;
    };

    std::optional<StringRef> ValueField;
    {
      StringRef Prefix = AsmFmt;
      if (size_t L = Prefix.find('['); L != StringRef::npos)
        Prefix = Prefix.take_front(L);
      if (Prefix.contains_insensitive("SrcD"))
        ValueField = "SrcD";
      else if (Prefix.contains_insensitive("SrcL"))
        ValueField = "SrcL";
    }
    const bool HasDest = AsmFmt.contains("->") ||
                         (!ValueField.has_value() && findField("RegDst"));
    StringRef PostMemFmt;
    if (size_t R = AsmFmt.find(']'); R != StringRef::npos)
      PostMemFmt = AsmFmt.drop_front(R + 1);
    const bool HasPostMemSrcR = !IsRegOffset && findField("SrcR") &&
                                PostMemFmt.contains_insensitive("SrcR");
    const bool HasPostMemSrcD =
        findField("SrcD") && PostMemFmt.contains_insensitive("SrcD");
    const bool HasPostMemSrcD1 =
        findField("SrcD1") && PostMemFmt.contains_insensitive("SrcD1");

    std::optional<int64_t> ForcedShift;
    if (AsmFmt.contains("<<1"))
      ForcedShift = 1;
    else if (AsmFmt.contains("<<2"))
      ForcedShift = 2;
    else if (AsmFmt.contains("<<3"))
      ForcedShift = 3;

    const StringRef BaseField = baseFieldFromAsmFmt();
    const bool IsStore = ValueField.has_value();

    if (IsStore) {
      // Stores: `val[, val1], [base, off|idx] [, ->dst]`.
      emitReg(*ValueField);
      if (findField("SrcD1")) {
        OS << ", ";
        emitReg("SrcD1");
      }
      OS << ", [";
      emitReg(BaseField);
      OS << ", ";
      if (IsRegOffset) {
        emitSrcRWithTypeAndShift(ForcedShift);
      } else {
        if (!emitScaledImmOff())
          OS << "0";
      }
      OS << "]";
      if (HasDest)
        emitArrowDest();
      printAnnotation(OS, Annot);
      return;
    }

    // Loads: `[base, off|idx] , ->dst`.
    OS << "[";
    emitReg(BaseField);
    OS << ", ";
    if (IsRegOffset) {
      emitSrcRWithTypeAndShift(/*ForcedShift=*/std::nullopt);
    } else {
      if (!emitScaledImmOff())
        OS << "0";
    }
    OS << "]";
    if (HasPostMemSrcR) {
      OS << ", ";
      emitReg("SrcR");
    }
    if (HasPostMemSrcD) {
      OS << ", ";
      emitReg("SrcD");
    }
    if (HasPostMemSrcD1) {
      OS << ", ";
      emitReg("SrcD1");
    }
    if (HasDest)
      emitArrowDest();
    printAnnotation(OS, Annot);
    return;
  }

  // Pretty printer for the common "SrcP, SrcL, SrcR<type><<shamt>" form.
  if (Form.mnemonic &&
      (StringRef(Form.mnemonic).equals_insensitive("CSEL") ||
       StringRef(Form.mnemonic).equals_insensitive("V.CSEL")) &&
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

  if (findField("RegDst0") && findField("RegDst1") && findField("SrcL") &&
      findField("SrcR") && !findField("SrcD") && !AsmFmt.contains('[')) {
    OS << mnemonicTok("<op>");
    OS << "\t";
    emitReg("SrcL");
    OS << ", ";
    emitReg("SrcR");
    if (auto V = findFieldImm("shamt")) {
      OS << ", " << *V;
    }
    emitArrowDest();
    printAnnotation(OS, Annot);
    return;
  }

  if (findField("RegDst0") && findField("RegDst1") && findField("SrcL") &&
      findField("SrcR") && findField("SrcD") && !AsmFmt.contains('[')) {
    OS << mnemonicTok("<op>");
    OS << "\t";
    emitReg("SrcL");
    OS << ", ";
    emitReg("SrcR");
    OS << ", ";
    emitReg("SrcD");
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

  const bool IsSetcImm = AsmFmt.starts_with_insensitive("setc.") &&
                         findField("shamt") &&
                         (findField("simm12") || findField("uimm12"));
  const bool HasSplitImm12 = findField("imml") && findField("imms");

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
    if (Name == "RegDst" || Name == "SrcL" || Name == "SrcR" ||
        Name == "SrcD" || Name == "SrcP" || Name == "SrcA")
      continue;
    if (IsSetcImm && (Name.equals_insensitive("shamt") ||
                      Name.equals_insensitive("simm12") ||
                      Name.equals_insensitive("uimm12")))
      continue;
    if (HasSplitImm12 &&
        (Name.equals_insensitive("imml") || Name.equals_insensitive("imms")))
      continue;
    if (Name.starts_with_insensitive("imm") ||
        Name.starts_with_insensitive("simm") ||
        Name.starts_with_insensitive("uimm") ||
        Name.starts_with_insensitive("shamt") ||
        Name.equals_insensitive("BrType") ||
        Name.equals_insensitive("BlockType") ||
        Name.equals_insensitive("SSR_ID") || Name.equals_insensitive("SSRID") ||
        Name.equals_insensitive("LSR_ID") ||
        Name.equals_insensitive("ACR-ID") || Name.equals_insensitive("C-ID")) {
      sep();
      const MCOperand &Op = KV.second;
      if (Op.isImm()) {
        if (Name.equals_insensitive("SSR_ID") ||
            Name.equals_insensitive("SSRID")) {
          if (const char *Sym =
                  ssrIdSymbol(static_cast<uint64_t>(Op.getImm()))) {
            OS << Sym;
          } else {
            OS << "0x"
               << utohexstr(static_cast<uint64_t>(Op.getImm()) & 0xfffu);
          }
        } else {
          OS << Op.getImm();
        }
      } else if (Op.isExpr())
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

  if (HasSplitImm12) {
    auto Low = findFieldImm("imml");
    auto High = findFieldImm("imms");
    if (Low && High) {
      sep();
      OS << (((*High & 0x3f) << 6) | (*Low & 0x3f));
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
