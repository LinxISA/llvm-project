//===-- LinxISAAsmBackend.cpp - LinxISA Assembler Backend -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxISAFixupKinds.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

uint32_t encodeB12Pcrel(uint64_t Value) {
  // Branch immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx branch target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<12>(Imm))
    report_fatal_error("Linx branch target out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x0FFFu;
  return ((UImm & 0x07Fu) << 25) | (((UImm >> 7) & 0x01Fu) << 7);
}

uint32_t encodeJ22Pcrel(uint64_t Value) {
  // Jump immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx jump target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<22>(Imm))
    report_fatal_error("Linx jump target out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x003FFFFFu;
  return ((UImm & 0x1FFFFu) << 15) | (((UImm >> 17) & 0x01Fu) << 7);
}

uint32_t encodeB17Pcrel(uint64_t Value) {
  // Block-start immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx block target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<17>(Imm))
    report_fatal_error("Linx block target out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x1FFFFu;
  return (UImm << 15);
}

uint32_t encodeB25Pcrel(uint64_t Value) {
  // B.TEXT immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx B.TEXT target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<25>(Imm))
    report_fatal_error("Linx B.TEXT target out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x01FFFFFFu; // 25 bits
  return (UImm << 7);
}

uint16_t encodeCBStart12Pcrel(uint64_t Value) {
  // C.BSTART immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx compressed block target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<12>(Imm))
    report_fatal_error("Linx compressed block target out of range");

  uint16_t UImm = static_cast<uint16_t>(Imm) & 0x0FFFu;
  return (UImm << 4);
}

uint64_t encodeHLBStart30Pcrel(uint64_t Value) {
  // HL.BSTART uses a signed, instruction-aligned byte offset (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx HL block target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value);
  if (!isInt<30>(Imm))
    report_fatal_error("Linx HL block target out of range");

  uint64_t UImm = static_cast<uint64_t>(Imm) & 0x3FFFFFFFull; // 30 bits
  uint64_t Patch = 0;
  // simm[17:1] -> insn[47:31]
  Patch |= ((UImm >> 1) & 0x1FFFFull) << 31;
  // simm[29:18] -> insn[15:4]
  Patch |= ((UImm >> 18) & 0x0FFFull) << 4;
  return Patch;
}

uint32_t encodeSetRet20Pcrel(uint64_t Value) {
  // setret immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx setret target is not 2-byte aligned");

  uint64_t Imm = Value >> 1;
  if (!isUInt<20>(Imm))
    report_fatal_error("Linx setret target out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x000FFFFFu;
  return (UImm << 12);
}

uint16_t encodeCSetRet5Pcrel(uint64_t Value) {
  // c.setret immediates are encoded in units of 2 bytes (bit 0 is implicit 0).
  if (Value & 0x1)
    report_fatal_error("Linx c.setret target is not 2-byte aligned");

  uint64_t Imm = Value >> 1;
  if (!isUInt<5>(Imm))
    report_fatal_error("Linx c.setret target out of range");

  uint16_t UImm = static_cast<uint16_t>(Imm) & 0x001Fu;
  return (UImm << 6);
}

uint64_t encodeHLSetRet32Pcrel(uint64_t Value) {
  // hl.setret uses imm32 (halfword offset).
  if (Value & 0x1)
    report_fatal_error("Linx hl.setret target is not 2-byte aligned");

  int64_t Imm = static_cast<int64_t>(Value) >> 1;
  if (!isInt<32>(Imm))
    report_fatal_error("Linx hl.setret target out of range");

  uint64_t UImm = static_cast<uint64_t>(Imm) & 0xFFFF'FFFFull;
  uint64_t Patch = 0;
  // imm32[19:0] -> insn[47:28]
  Patch |= (UImm & 0xFFFFFull) << 28;
  // imm32[31:20] -> insn[15:4]
  Patch |= ((UImm >> 20) & 0x0FFFull) << 4;
  return Patch;
}

uint32_t encodePcrelHi20(uint64_t Value) {
  // ADDTPC uses simm20 in bits [31:12] and is scaled by 4KiB (imm20 << 12).
  //
  // Tooling supplies the full PC-relative byte delta (S + A - P). Encode the
  // high 20 bits using the usual "HI20 with carry" rule so that a subsequent
  // low-12 add can form the full address:
  //   hi20 = (delta + 0x800) >> 12
  int64_t Delta = static_cast<int64_t>(Value);
  int64_t Imm = (Delta + 0x800) >> 12;
  if (!isInt<20>(Imm))
    report_fatal_error("Linx ADDTPC PC-relative page offset out of range");

  uint32_t UImm = static_cast<uint32_t>(Imm) & 0x000FFFFFu;
  return UImm << 12;
}

uint32_t encodeLo12(uint64_t Value) {
  // ADDI/ADDIW low 12-bit absolute offset in bits [31:20].
  uint32_t UImm = static_cast<uint32_t>(Value) & 0x00000FFFu;
  return UImm << 20;
}

uint32_t encodePcr17Load(uint64_t Value) {
  // *.PCR loads use a signed 17-bit byte offset in bits [31:15].
  if (!isInt<17>(static_cast<int64_t>(Value)))
    report_fatal_error("Linx *.PCR load target out of range");
  uint32_t UImm = static_cast<uint32_t>(static_cast<int64_t>(Value)) & 0x1FFFFu;
  return UImm << 15;
}

uint32_t encodePcr17Store(uint64_t Value) {
  // *.PCR stores use a signed 17-bit byte offset split across:
  //   simm[11:0]  -> insn[31:20]
  //   simm[16:12] -> insn[11:7]
  if (!isInt<17>(static_cast<int64_t>(Value)))
    report_fatal_error("Linx *.PCR store target out of range");
  uint32_t UImm = static_cast<uint32_t>(static_cast<int64_t>(Value)) & 0x1FFFFu;
  uint32_t Patch = 0;
  Patch |= (UImm & 0x0FFFu) << 20;
  Patch |= ((UImm >> 12) & 0x1Fu) << 7;
  return Patch;
}

uint64_t encodeHLPcr29Load(uint64_t Value) {
  // HL.*.PCR loads use a signed 29-bit byte offset split across:
  //   simm[16:0]  -> insn[47:31]
  //   simm[28:17] -> insn[15:4]
  if (!isInt<29>(static_cast<int64_t>(Value)))
    report_fatal_error("Linx HL.*.PCR load target out of range");
  uint64_t UImm = static_cast<uint64_t>(static_cast<int64_t>(Value)) & 0x1FFF'FFFFull;
  uint64_t Patch = 0;
  Patch |= (UImm & 0x1FFFFull) << 31;
  Patch |= ((UImm >> 17) & 0x0FFFull) << 4;
  return Patch;
}

uint64_t encodeHLPcr29Store(uint64_t Value) {
  // HL.*.PCR stores use a signed 29-bit byte offset split across:
  //   simm[11:0]  -> insn[47:36]
  //   simm[16:12] -> insn[27:23]
  //   simm[28:17] -> insn[15:4]
  if (!isInt<29>(static_cast<int64_t>(Value)))
    report_fatal_error("Linx HL.*.PCR store target out of range");
  uint64_t UImm = static_cast<uint64_t>(static_cast<int64_t>(Value)) & 0x1FFF'FFFFull;
  uint64_t Patch = 0;
  Patch |= (UImm & 0x0FFFull) << 36;
  Patch |= ((UImm >> 12) & 0x01Full) << 23;
  Patch |= ((UImm >> 17) & 0x0FFFull) << 4;
  return Patch;
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

class LinxISAAsmBackend : public MCAsmBackend {
  Triple::OSType OSType;
  bool Is64Bit;

public:
  LinxISAAsmBackend(Triple::OSType OSType, bool Is64Bit)
      : MCAsmBackend(llvm::endianness::little), OSType(OSType),
        Is64Bit(Is64Bit) {}

  bool mayNeedRelaxation(unsigned Opcode, ArrayRef<MCOperand> Operands,
                         const MCSubtargetInfo &STI) const override {
    (void)Operands;
    (void)STI;
    if (Opcode >= linxisa_inst_forms_count)
      return false;
    const linxisa_inst_form &F = linxisa_inst_forms[Opcode];
    StringRef Mnemonic = F.mnemonic ? StringRef(F.mnemonic) : StringRef();
    StringRef AsmFmt = F.asm_fmt ? StringRef(F.asm_fmt) : StringRef();

    if (Mnemonic == "C.SETRET" || Mnemonic == "SETRET")
      return true;

    if (Mnemonic == "C.BSTART" &&
        (AsmFmt.contains(" DIRECT") || AsmFmt.contains(" COND")))
      return true;

    if (Mnemonic.starts_with("BSTART.") &&
        (AsmFmt == "BSTART.STD" || AsmFmt.contains(" DIRECT") ||
         AsmFmt.contains(" COND") || AsmFmt.contains(" CALL") ||
         AsmFmt.contains("FALL<, fixup_label>")))
      return true;

    if (!Mnemonic.starts_with("HL.") && Mnemonic.ends_with(".PCR"))
      return true;

    return false;
  }

  bool fixupNeedsRelaxationAdvanced(const MCFragment &F, const MCFixup &Fixup,
                                    const MCValue &Target, uint64_t Value,
                                    bool Resolved) const override {
    (void)F;
    (void)Fixup;
    (void)Target;
    if (!Resolved) {
      // If the target symbol is not resolved at assembly time (e.g. external
      // symbols in relocatable objects), conservatively relax instructions that
      // have short PC-relative ranges. LLD does not currently relax LinxISA
      // relocations, so emitting the long HL.* forms here avoids link-time
      // "relocation out of range" failures for large images such as the Linux
      // kernel.
      switch (Fixup.getKind()) {
      case LinxISA::FIXUP_LINX_CBSTART12_PCREL:
      case LinxISA::FIXUP_LINX_B17_PCREL:
      case LinxISA::FIXUP_LINX_B17_PLT:
      case LinxISA::FIXUP_LINX_CSETRET5_PCREL:
      case LinxISA::FIXUP_LINX_SETRET20_PCREL:
      case LinxISA::FIXUP_LINX_PCR17_LOAD:
      case LinxISA::FIXUP_LINX_PCR17_STORE:
        return true;
      default:
        return false;
      }
    }
    return fixupNeedsRelaxation(Fixup, Value);
  }

  bool fixupNeedsRelaxation(const MCFixup &Fixup,
                            uint64_t Value) const override {
    switch (Fixup.getKind()) {
    default:
      return false;
    case LinxISA::FIXUP_LINX_CBSTART12_PCREL: {
      if (Value & 0x1)
        return true;
      int64_t Imm = static_cast<int64_t>(Value) >> 1;
      return !isInt<12>(Imm);
    }
    case LinxISA::FIXUP_LINX_B17_PCREL: {
      if (Value & 0x1)
        return true;
      int64_t Imm = static_cast<int64_t>(Value) >> 1;
      return !isInt<17>(Imm);
    }
    case LinxISA::FIXUP_LINX_CSETRET5_PCREL: {
      if (Value & 0x1)
        return true;
      uint64_t Imm = Value >> 1;
      return !isUInt<5>(Imm);
    }
    case LinxISA::FIXUP_LINX_SETRET20_PCREL: {
      if (Value & 0x1)
        return true;
      uint64_t Imm = Value >> 1;
      return !isUInt<20>(Imm);
    }
    case LinxISA::FIXUP_LINX_PCR17_LOAD:
    case LinxISA::FIXUP_LINX_PCR17_STORE:
      return !isInt<17>(static_cast<int64_t>(Value));
    }
  }

  void relaxInstruction(MCInst &Inst,
                        const MCSubtargetInfo &STI) const override {
    (void)STI;

    if (Inst.getOpcode() >= linxisa_inst_forms_count)
      report_fatal_error("Linx: relaxInstruction opcode out of range");

    const linxisa_inst_form &F = linxisa_inst_forms[Inst.getOpcode()];
    StringRef Mnemonic = F.mnemonic ? StringRef(F.mnemonic) : StringRef();
    StringRef AsmFmt = F.asm_fmt ? StringRef(F.asm_fmt) : StringRef();

    // C.SETRET -> SETRET -> HL.SETRET
    if (Mnemonic == "C.SETRET") {
      Inst.setOpcode(
          findSpecOpcodeByAsmFmt("setret uimm, ->Ra", /*LengthBits=*/32));
      return;
    }
    if (Mnemonic == "SETRET") {
      Inst.setOpcode(
          findSpecOpcodeByAsmFmt("hl.setret imm, ->Ra", /*LengthBits=*/48));
      return;
    }

    // C.BSTART.{DIRECT,COND} -> BSTART.STD.{DIRECT,COND} -> HL.BSTART.STD.{DIRECT,COND}
    if (Mnemonic == "C.BSTART") {
      if (AsmFmt.contains(" DIRECT")) {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("BSTART.STD DIRECT, <label>",
                                              /*LengthBits=*/32));
        return;
      }
      if (AsmFmt.contains(" COND")) {
        Inst.setOpcode(
            findSpecOpcodeByAsmFmt("BSTART.STD COND, <label>", /*LengthBits=*/32));
        return;
      }
    }
    if (Mnemonic == "BSTART.STD") {
      if (AsmFmt == "BSTART.STD") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt(
            "HL.BSTART.STD FALL<, fixup_label>", /*LengthBits=*/48));
        return;
      }
      if (AsmFmt.contains("FALL<, fixup_label>")) {
        Inst.setOpcode(findSpecOpcodeByAsmFmt(
            "HL.BSTART.STD FALL<, fixup_label>", /*LengthBits=*/48));
        return;
      }
      if (AsmFmt.contains(" DIRECT")) {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("HL.BSTART.STD DIRECT, <label>",
                                              /*LengthBits=*/48));
        return;
      }
      if (AsmFmt.contains(" COND")) {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("HL.BSTART.STD COND, <label>",
                                              /*LengthBits=*/48));
        return;
      }
      if (AsmFmt.contains(" CALL")) {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("HL.BSTART.STD CALL, <label>",
                                              /*LengthBits=*/48));
        return;
      }
    }
    if (Mnemonic == "BSTART.FP" && AsmFmt.contains("FALL<, fixup_label>")) {
      Inst.setOpcode(findSpecOpcodeByAsmFmt(
          "HL.BSTART.FP FALL<, fixup_label>", /*LengthBits=*/48));
      return;
    }
    if (Mnemonic == "BSTART.SYS" && AsmFmt.contains("FALL<, fixup_label>")) {
      Inst.setOpcode(findSpecOpcodeByAsmFmt(
          "HL.BSTART.SYS FALL<, fixup_label>", /*LengthBits=*/48));
      return;
    }

    // *.PCR -> HL.*.PCR
    if (!Mnemonic.starts_with("HL.") && Mnemonic.ends_with(".PCR")) {
      if (Mnemonic == "LB.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lb.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LBU.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lbu.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LH.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lh.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LHU.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lhu.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LW.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lw.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LWU.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.lwu.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "LD.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.ld.pcr [<symbol>], ->{t, u, Rd}",
                                              /*LengthBits=*/48));
        return;
      }

      if (Mnemonic == "SB.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.sb.pcr SrcL, [<symbol>]",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "SH.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.sh.pcr SrcL, [<symbol>]",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "SW.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.sw.pcr SrcL, [<symbol>]",
                                              /*LengthBits=*/48));
        return;
      }
      if (Mnemonic == "SD.PCR") {
        Inst.setOpcode(findSpecOpcodeByAsmFmt("hl.sd.pcr SrcL, [<symbol>]",
                                              /*LengthBits=*/48));
        return;
      }
    }

    report_fatal_error("Linx: unsupported instruction relaxation");
  }

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, uint8_t *Data, uint64_t Value,
                  bool IsResolved) override {
    // Like AArch64 ADRP, ADDTPC uses a page-based delta which can vary by one
    // depending on the final section alignment. Even if a local target appears
    // resolvable during assembly, the linker must make the final decision.
    const bool ForceReloc =
        (Fixup.getKind() == LinxISA::FIXUP_LINX_PCREL_HI20 ||
         Fixup.getKind() == LinxISA::FIXUP_LINX_GOT_HI20);
    if (ForceReloc)
      IsResolved = false;

    if (!IsResolved)
      Asm->getWriter().recordRelocation(F, Fixup, Target, Value);

    // Leave the immediate zeroed and let the linker apply the relocation.
    if (ForceReloc)
      return;

    if (!Value)
      return;

    const unsigned Kind = Fixup.getKind();
    uint64_t Patch = 0;
    unsigned PatchBytes = 4;
    switch (Kind) {
    case FK_Data_1:
    case FK_SecRel_1:
      PatchBytes = 1;
      Patch = Value;
      break;
    case FK_Data_2:
    case FK_SecRel_2:
      PatchBytes = 2;
      Patch = Value;
      break;
    case FK_Data_4:
    case FK_SecRel_4:
      PatchBytes = 4;
      Patch = Value;
      break;
    case FK_Data_8:
    case FK_SecRel_8:
      PatchBytes = 8;
      Patch = Value;
      break;
    case LinxISA::FIXUP_LINX_NONE:
      return;
    case LinxISA::FIXUP_LINX_B12_PCREL:
      Patch = encodeB12Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_J22_PCREL:
      Patch = encodeJ22Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_CBSTART12_PCREL:
      PatchBytes = 2;
      Patch = encodeCBStart12Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_B17_PCREL:
      Patch = encodeB17Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_B25_PCREL:
      Patch = encodeB25Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_B17_PLT:
      Patch = encodeB17Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_HL_BSTART30_PCREL:
      PatchBytes = 6;
      Patch = encodeHLBStart30Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_CSETRET5_PCREL:
      PatchBytes = 2;
      Patch = encodeCSetRet5Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_SETRET20_PCREL:
      Patch = encodeSetRet20Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_HL_SETRET32_PCREL:
      PatchBytes = 6;
      Patch = encodeHLSetRet32Pcrel(Value);
      break;
    case LinxISA::FIXUP_LINX_PCREL_HI20:
      Patch = encodePcrelHi20(Value);
      break;
    case LinxISA::FIXUP_LINX_GOT_HI20:
      Patch = encodePcrelHi20(Value);
      break;
    case LinxISA::FIXUP_LINX_GOT_LO12:
      Patch = encodeLo12(Value);
      break;
    case LinxISA::FIXUP_LINX_LO12:
      Patch = encodeLo12(Value);
      break;
    case LinxISA::FIXUP_LINX_PCR17_LOAD:
      Patch = encodePcr17Load(Value);
      break;
    case LinxISA::FIXUP_LINX_PCR17_STORE:
      Patch = encodePcr17Store(Value);
      break;
    case LinxISA::FIXUP_LINX_HL_PCR29_LOAD:
      PatchBytes = 6;
      Patch = encodeHLPcr29Load(Value);
      break;
    case LinxISA::FIXUP_LINX_HL_PCR29_STORE:
      PatchBytes = 6;
      Patch = encodeHLPcr29Store(Value);
      break;
    default:
      llvm_unreachable("Unknown Linx fixup kind");
    }

    if (PatchBytes == 1) {
      Data[0] |= static_cast<uint8_t>(Patch);
      return;
    }
    if (PatchBytes == 2) {
      uint16_t Cur = support::endian::read16le(Data);
      Cur |= static_cast<uint16_t>(Patch);
      support::endian::write16le(Data, Cur);
      return;
    }
    if (PatchBytes == 4) {
      uint32_t Cur = support::endian::read32le(Data);
      Cur |= static_cast<uint32_t>(Patch);
      support::endian::write32le(Data, Cur);
      return;
    }
    if (PatchBytes == 6) {
      uint64_t Cur = 0;
      for (unsigned i = 0; i < 6; ++i)
        Cur |= static_cast<uint64_t>(Data[i]) << (i * 8);
      Cur |= Patch;
      for (unsigned i = 0; i < 6; ++i)
        Data[i] = static_cast<uint8_t>((Cur >> (i * 8)) & 0xFF);
      return;
    }
    if (PatchBytes == 8) {
      uint64_t Cur = support::endian::read64le(Data);
      Cur |= Patch;
      support::endian::write64le(Data, Cur);
      return;
    }

    llvm_unreachable("Unexpected fixup patch size");
  }

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    return createLinxISAELFObjectWriter(MCELFObjectTargetWriter::getOSABI(OSType),
                                        Is64Bit);
  }

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override {
    static const MCFixupKindInfo Infos[LinxISA::NumTargetFixupKinds] = {
        {"FIXUP_LINX_NONE", 0, 32, 0},
        // Non-contiguous, but the size is only used for diagnostics.
        {"FIXUP_LINX_B12_PCREL", 0, 12, 0},
        {"FIXUP_LINX_J22_PCREL", 0, 22, 0},
        {"FIXUP_LINX_CBSTART12_PCREL", 0, 12, 0},
        {"FIXUP_LINX_B17_PCREL", 0, 17, 0},
        {"FIXUP_LINX_B25_PCREL", 7, 25, 0},
        {"FIXUP_LINX_B17_PLT", 0, 17, 0},
        // simm30 byte offset, instruction-aligned.
        {"FIXUP_LINX_HL_BSTART30_PCREL", 0, 30, 0},
        {"FIXUP_LINX_CSETRET5_PCREL", 0, 5, 0},
        {"FIXUP_LINX_SETRET20_PCREL", 0, 20, 0},
        {"FIXUP_LINX_HL_SETRET32_PCREL", 0, 32, 0},
        // PC-relative page offset for ADDTPC (imm20 << 12).
        {"FIXUP_LINX_PCREL_HI20", 12, 20, 0},
        // GOT page offset for ADDTPC (imm20 << 12).
        {"FIXUP_LINX_GOT_HI20", 12, 20, 0},
        // Absolute low 12 bits of the GOT entry address for ADDI/ADDIW.
        {"FIXUP_LINX_GOT_LO12", 20, 12, 0},
        // Absolute low 12 bits for ADDI/ADDIW (uimm12).
        {"FIXUP_LINX_LO12", 20, 12, 0},
        {"FIXUP_LINX_PCR17_LOAD", 0, 17, 0},
        {"FIXUP_LINX_PCR17_STORE", 0, 17, 0},
        {"FIXUP_LINX_HL_PCR29_LOAD", 0, 29, 0},
        {"FIXUP_LINX_HL_PCR29_STORE", 0, 29, 0},
    };

    if (Kind < FirstTargetFixupKind)
      return MCAsmBackend::getFixupKindInfo(Kind);

    const unsigned Idx = Kind - FirstTargetFixupKind;
    assert(Idx < LinxISA::NumTargetFixupKinds && "Invalid fixup kind");
    return Infos[Idx];
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override {
    if ((Count % 2) != 0)
      return false;
    // 16b NOP: `c.movr zero, ->zero` (encoding 0x0006).
    while (Count >= 4) {
      // 32b conservative NOP: `addi zero, 0, ->zero`.
      OS.write("\x15\0\0\0", 4);
      Count -= 4;
    }
    if (Count == 2)
      OS.write("\x06\0", 2);
    return true;
  }
};

} // namespace

MCAsmBackend *llvm::createLinxISAAsmBackend(const Target &T,
                                            const MCSubtargetInfo &STI,
                                            const MCRegisterInfo &MRI,
                                            const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  if (!TT.isOSBinFormatELF())
    report_fatal_error("Linx: only ELF is supported");
  return new LinxISAAsmBackend(TT.getOS(), TT.isArch64Bit());
}
