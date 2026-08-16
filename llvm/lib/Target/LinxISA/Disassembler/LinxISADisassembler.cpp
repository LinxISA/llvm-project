#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/MathExtras.h"
#include <cstdint>
#include <optional>

using namespace llvm;

namespace {

class LinxISADisassembler : public MCDisassembler {
public:
  LinxISADisassembler(const MCSubtargetInfo &STI, MCContext &Ctx)
      : MCDisassembler(STI, Ctx) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;
};

} // namespace

static MCDisassembler *createLinxISADisassembler(const Target & /*T*/,
                                                 const MCSubtargetInfo &STI,
                                                 MCContext &Ctx) {
  return new LinxISADisassembler(STI, Ctx);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLinxISADisassembler() {
  TargetRegistry::RegisterMCDisassembler(getTheLinx32Target(),
                                         createLinxISADisassembler);
  TargetRegistry::RegisterMCDisassembler(getTheLinx64Target(),
                                         createLinxISADisassembler);
}

static uint64_t readLE(ArrayRef<uint8_t> Bytes, unsigned SizeBytes) {
  uint64_t V = 0;
  for (unsigned i = 0; i < SizeBytes; ++i)
    V |= uint64_t(Bytes[i]) << (8 * i);
  return V;
}

static int64_t signExtend(uint64_t V, unsigned Bits) {
  if (Bits == 0 || Bits >= 64)
    return static_cast<int64_t>(V);
  uint64_t SignBit = 1ULL << (Bits - 1);
  if (V & SignBit)
    V |= (~0ULL) << Bits;
  return static_cast<int64_t>(V);
}

static bool isSupportedLength(unsigned Bits) {
  return Bits == 16 || Bits == 32 || Bits == 48 || Bits == 64;
}

static bool isLegalCompressedStdBrType(const linxisa_inst_form &Form,
                                       ArrayRef<int64_t> FieldVals) {
  StringRef Mnem(Form.mnemonic ? Form.mnemonic : "");
  if (Mnem != "C.BSTART.STD")
    return true;

  for (unsigned I = 0; I < Form.field_count; ++I) {
    StringRef FieldName(linxisa_fields[Form.field_start + I].name);
    if (FieldName != "BrType")
      continue;
    const int64_t BrType = FieldVals[I];
    return BrType == 1 || BrType == 5 || BrType == 7;
  }
  return false;
}

static const linxisa_inst_form *findMatch(uint64_t Insn, unsigned Bits,
                                          unsigned &OutOpcode) {
  // Best-effort: pick the matching form with the most fixed bits.
  unsigned Best = ~0U;
  unsigned BestFixed = 0;

  for (unsigned i = 0; i < linxisa_inst_forms_count; ++i) {
    const linxisa_inst_form &F = linxisa_inst_forms[i];
    if (unsigned(F.length_bits) != Bits)
      continue;
    if ((Insn & F.mask) != F.match)
      continue;

    // Disambiguate packed tile/par headers from MSEQ/MPAR in canonical PTO
    // 0.58: MSEQ/MPAR require bit[25]=0. Some generated masks are currently
    // under-constrained and can otherwise steal TEPL/TLSU/CUBE headers during
    // disassembly.
    StringRef Mnem(F.mnemonic ? F.mnemonic : "");
    if ((Mnem == "BSTART.MSEQ" || Mnem == "BSTART.MPAR") &&
        (((Insn >> 25) & 0x1ULL) != 0ULL))
      continue;

    unsigned Fixed = llvm::popcount(static_cast<uint64_t>(F.mask));
    if (Best == ~0U || Fixed > BestFixed) {
      Best = i;
      BestFixed = Fixed;
    }
  }

  if (Best == ~0U)
    return nullptr;
  OutOpcode = Best;
  return &linxisa_inst_forms[Best];
}

static void extractFields(const linxisa_inst_form &Form, uint64_t Insn,
                          SmallVectorImpl<int64_t> &Out) {
  Out.clear();
  for (unsigned i = 0; i < Form.field_count; ++i) {
    const linxisa_field &F = linxisa_fields[Form.field_start + i];
    uint64_t Val = 0;
    for (unsigned j = 0; j < F.piece_count; ++j) {
      const linxisa_field_piece &P = linxisa_field_pieces[F.piece_start + j];
      uint64_t Bits = (Insn >> P.insn_lsb) & ((1ULL << P.width) - 1);
      Val |= Bits << P.value_lsb;
    }

    int64_t SignedVal = static_cast<int64_t>(Val);
    if (F.signed_hint == 1)
      SignedVal = signExtend(Val, F.bit_width);
    Out.push_back(SignedVal);
  }
}

static bool isBStartCall(const linxisa_inst_form &Form) {
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  return (AsmFmt.starts_with_insensitive("bstart") ||
          AsmFmt.starts_with_insensitive("hl.bstart")) &&
         AsmFmt.contains_insensitive(" call,");
}

static bool isSetRet(const linxisa_inst_form &Form) {
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  return AsmFmt.starts_with_insensitive("setret") ||
         AsmFmt.starts_with_insensitive("c.setret") ||
         AsmFmt.starts_with_insensitive("hl.setret");
}

static bool isSignedSetRet(const linxisa_inst_form &Form) {
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  return AsmFmt.starts_with_insensitive("hl.setret");
}

static bool isFrameTemplateForm(const linxisa_inst_form &Form) {
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  return AsmFmt.contains("[RegSrc0 ~ RegSrcn]") ||
         AsmFmt.contains("[RegDst0 ~ RegDstn]");
}

static bool isFrameTemplateEndpoint(uint64_t Reg) {
  return Reg >= 2u && Reg <= 23u;
}

static unsigned frameTemplateRegisterCount(uint64_t Begin, uint64_t End) {
  return static_cast<unsigned>(((End + 22u - Begin) % 22u) + 1u);
}

static bool isLegalFrameTemplate(const linxisa_inst_form &Form,
                                 ArrayRef<int64_t> FieldVals) {
  if (!isFrameTemplateForm(Form))
    return true;

  std::optional<uint64_t> Begin;
  std::optional<uint64_t> End;
  std::optional<uint64_t> FrameBytes;

  for (unsigned i = 0; i < FieldVals.size(); ++i) {
    const linxisa_field &F = linxisa_fields[Form.field_start + i];
    StringRef FieldName(F.name ? F.name : "");
    const uint64_t V = static_cast<uint64_t>(FieldVals[i]);
    if (FieldName == "SrcBegin" || FieldName == "DstBegin")
      Begin = V;
    else if (FieldName == "SrcEnd" || FieldName == "DstEnd")
      End = V;
    else if (FieldName == "uimm")
      FrameBytes = V;
  }

  if (!Begin || !End || !FrameBytes)
    return false;
  if (!isFrameTemplateEndpoint(*Begin) || !isFrameTemplateEndpoint(*End))
    return false;
  if ((*FrameBytes % 8u) != 0)
    return false;
  const unsigned SavedRegs = frameTemplateRegisterCount(*Begin, *End);
  if (*FrameBytes < uint64_t{8} * SavedRegs)
    return false;

  StringRef Mnem(Form.mnemonic ? Form.mnemonic : "");
  if (Mnem == "FRET.STK" && *Begin != 10u)
    return false;

  return true;
}

static bool isLegalBIOTDestination(const linxisa_inst_form &Form,
                                   ArrayRef<int64_t> FieldVals) {
  StringRef Mnemonic(Form.mnemonic ? Form.mnemonic : "");
  StringRef AsmFmt(Form.asm_fmt ? Form.asm_fmt : "");
  if (Mnemonic != "B.IOT" || !AsmFmt.contains("->DstTile"))
    return true;

  for (unsigned I = 0; I != FieldVals.size(); ++I) {
    const linxisa_field &Field = linxisa_fields[Form.field_start + I];
    if (StringRef(Field.name ? Field.name : "") == "DstTile")
      return static_cast<uint64_t>(FieldVals[I]) <= 3u;
  }
  return false;
}

MCDisassembler::DecodeStatus
LinxISADisassembler::getInstruction(MCInst &Instr, uint64_t &Size,
                                    ArrayRef<uint8_t> Bytes, uint64_t Address,
                                    raw_ostream &CStream) const {
  // Allow symbolic operand printing when llvm-objdump installs a symbolizer.
  CommentStream = &CStream;

  // LinxISA has overlapping encodings across lengths (e.g. templates/prefixes).
  // Prefer the longest matching encoding to keep the disassembler in sync.
  static constexpr unsigned CandidateBits[] = {64, 48, 32, 16};

  unsigned MatchedOpcode = 0;
  const linxisa_inst_form *Matched = nullptr;
  unsigned MatchedBits = 0;

  for (unsigned Bits : CandidateBits) {
    if (!isSupportedLength(Bits))
      continue;
    unsigned SizeBytes = Bits / 8;
    if (Bytes.size() < SizeBytes)
      continue;
    uint64_t Insn = readLE(Bytes, SizeBytes);
    unsigned Opcode = 0;
    const linxisa_inst_form *Form = findMatch(Insn, Bits, Opcode);
    if (!Form)
      continue;
    Matched = Form;
    MatchedOpcode = Opcode;
    MatchedBits = Bits;
    break;
  }

  if (!Matched) {
    // Fallback size: advance by 2 bytes if possible, otherwise fail.
    Size = Bytes.size() >= 2 ? 2 : 0;
    return Fail;
  }

  Size = MatchedBits / 8;
  uint64_t Insn = readLE(Bytes, Size);

  Instr.clear();
  Instr.setOpcode(MatchedOpcode);

  StringRef Mnem(Matched->mnemonic ? Matched->mnemonic : "");

  auto trySymbolize = [&](int64_t Value, bool IsBranchLike) -> bool {
    // For bring-up, use Offset=0/OpSize=0. This matches common target patterns
    // and allows llvm-objdump to turn relocations into symbolic operands.
    return tryAddingSymbolicOperand(Instr, Value, Address, IsBranchLike,
                                    /*Offset=*/0, /*OpSize=*/0,
                                    /*InstSize=*/Size);
  };

  const bool IsFusedCall32 = Mnem == "BSTART.CALL";
  const bool IsFusedCall48 = Mnem == "HL.BSTART CALL";
  const bool IsFusedICall32 = Mnem == "BSTART.ICALL";

  auto isHalfwordPcRel = [&](StringRef FieldName) -> bool {
    // Control-flow immediates are halfword-scaled.
    return FieldName == "simm12" || FieldName == "simm17" ||
           FieldName == "simm22" || FieldName == "simm25" ||
           FieldName == "uimm5" || FieldName == "imm20" || FieldName == "imm32";
  };

  SmallVector<int64_t, 16> FieldVals;
  extractFields(*Matched, Insn, FieldVals);
  if (!isLegalCompressedStdBrType(*Matched, FieldVals) ||
      !isLegalFrameTemplate(*Matched, FieldVals) ||
      !isLegalBIOTDestination(*Matched, FieldVals)) {
    Instr.clear();
    return Fail;
  }

  for (unsigned i = 0; i < FieldVals.size(); ++i) {
    const linxisa_field &F = linxisa_fields[Matched->field_start + i];
    StringRef FieldName(F.name ? F.name : "");
    const int64_t V = FieldVals[i];

    bool WantsSym = false;
    bool IsBranchLike = false;
    int64_t SymValue = V;

    if (Mnem.ends_with(".PCR") &&
        (FieldName == "simm17" || FieldName == "simm")) {
      // *.PCR/HL.*.PCR: PC-relative data access.
      WantsSym = true;
      IsBranchLike = false;
      SymValue = static_cast<int64_t>(Address) + V;
    } else if (FieldName == "simm12" && Mnem.starts_with("B.")) {
      // Conditional branches (halfword-scaled).
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "simm22" && Mnem == "J") {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "simm12" && IsFusedCall32) {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "simm25" && IsFusedCall48) {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "uimm5" &&
               (IsFusedCall32 || IsFusedCall48 || IsFusedICall32)) {
      WantsSym = true;
      IsBranchLike = true;
      const uint64_t ReturnFieldOffset = IsFusedCall48 ? 4 : 2;
      SymValue = static_cast<int64_t>(Address + ReturnFieldOffset) + (V << 1);
      if (tryAddingSymbolicOperand(Instr, SymValue, Address, IsBranchLike,
                                   /*Offset=*/ReturnFieldOffset,
                                   /*OpSize=*/2, /*InstSize=*/Size))
        continue;
      Instr.addOperand(MCOperand::createImm(V));
      continue;
    } else if (FieldName == "simm12" && Mnem.starts_with("C.BSTART")) {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if ((FieldName == "simm17" || FieldName == "simm25") &&
               Mnem.starts_with("BSTART.")) {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "simm25" && Mnem == "B.TEXT") {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    } else if (FieldName == "simm" && Mnem.starts_with("HL.BSTART")) {
      WantsSym = true;
      IsBranchLike = true;
      SymValue = static_cast<int64_t>(Address) + V;
    } else if (isHalfwordPcRel(FieldName) &&
               (Mnem == "C.SETRET" || Mnem == "SETRET" ||
                Mnem == "HL.SETRET")) {
      WantsSym = true;
      IsBranchLike = true;
      // SETRET immediates are halfword offsets from the SETRET instruction PC.
      SymValue = static_cast<int64_t>(Address) + (V << 1);
    }

    if (WantsSym && trySymbolize(SymValue, IsBranchLike))
      continue;

    Instr.addOperand(MCOperand::createImm(V));
  }

  // Disassembler sugar: fuse `BSTART ... CALL` + `SETRET` into a single
  // printed instruction, while still consuming both encodings.
  if (isBStartCall(*Matched)) {
    const uint64_t BStartSize = Size;
    ArrayRef<uint8_t> Tail = Bytes.drop_front(BStartSize);

    const linxisa_inst_form *NextForm = nullptr;
    unsigned NextBits = 0;

    for (unsigned Bits : CandidateBits) {
      if (!isSupportedLength(Bits))
        continue;
      unsigned SizeBytes = Bits / 8;
      if (Tail.size() < SizeBytes)
        continue;
      uint64_t NextInsn = readLE(Tail, SizeBytes);
      unsigned TmpOpcode = 0;
      const linxisa_inst_form *Form = findMatch(NextInsn, Bits, TmpOpcode);
      if (!Form)
        continue;
      NextForm = Form;
      NextBits = Bits;
      break;
    }

    if (NextForm && isSetRet(*NextForm)) {
      const uint64_t NextSize = NextBits / 8;
      uint64_t NextInsn = readLE(Tail, NextSize);
      SmallVector<int64_t, 16> NextFieldVals;
      extractFields(*NextForm, NextInsn, NextFieldVals);
      if (!NextFieldVals.empty()) {
        const int64_t Enc = NextFieldVals[0];
        const uint64_t SetRetAddr = Address + BStartSize;

        uint64_t Target = 0;
        if (isSignedSetRet(*NextForm)) {
          int64_t Delta = Enc;
          Delta <<= 1;
          Target =
              static_cast<uint64_t>(static_cast<int64_t>(SetRetAddr) + Delta);
        } else {
          uint64_t Delta = static_cast<uint64_t>(Enc) << 1;
          Target = SetRetAddr + Delta;
        }

        // Prefer symbolic printing for the fused return-target annotation.
        // Use the BSTART instruction address with an Offset pointing at the
        // SETRET encoding, so relocations at the SETRET address can be used
        // by the symbolizer even though the disassembler prints a fused form.
        const uint64_t TotalSize = BStartSize + NextSize;
        if (!tryAddingSymbolicOperand(Instr, static_cast<int64_t>(Target),
                                      /*Address=*/Address,
                                      /*IsBranch=*/true,
                                      /*Offset=*/BStartSize,
                                      /*OpSize=*/NextSize,
                                      /*InstSize=*/TotalSize)) {
          Instr.addOperand(MCOperand::createImm(static_cast<int64_t>(Target)));
        }
        Size = BStartSize + NextSize;
      }
    }
  }

  return Success;
}
