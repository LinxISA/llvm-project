//===-- LinxV5AsmBackend.cpp - LinxV5 Assembler Backend ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5AsmBackend.h"
#include "LinxV5MCExpr.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

Optional<MCFixupKind> LinxV5AsmBackend::getFixupKind(StringRef Name) const {
  assert(0 && "TODO: Asm back-end!");
  return None;
}

const MCFixupKindInfo &
LinxV5AsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // LinxV5FixupKinds.h.
      //
      // name    offset    bits      flags
      {"fixup_linxv5_bnext", 7, 25, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_bnext_c", 4, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_btext", 7, 25, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_32_bnext", 15, 17, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_48_bnext", 15, 33, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_64_bnext", 15, 49, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_tpcrel_hi20", 12, 20, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_tpcrel_hi32", 12, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_tpcrel_lo12_i", 20, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_tpcrel_lo12_l", 20, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_tpcrel_lo12_s", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_c_addpc", 2, 5, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_addpc", 20, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_hlsetret", 12, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_simt_branch", 32, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_simt_branch_rc", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_simt_jump", 32, 32, MCFixupKindInfo::FKF_IsPCRel},

      {"fixup_linxv5_simt_tpcrel_lo12_i", 52, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_simt_tpcrel_lo12_l", 52, 12, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_simt_tpcrel_lo12_s", 32, 64, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_load_symbol", 17, 15, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_store_symbol", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_branch", 0, 32, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_branch_22", 7, 25, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_stack_size", 7, 5, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_load_symbol_target_42", 7, 57, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_store_symbol_target_42", 7, 57, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_load_symbol_target_29", 4, 44, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_store_symbol_target_29", 4, 44, MCFixupKindInfo::FKF_IsPCRel},
      {"fixup_linxv5_add_8", 0, 8, 0},
      {"fixup_linxv5_sub_8", 0, 8, 0},

      {"fixup_linxv5_add_16", 0, 16, 0},
      {"fixup_linxv5_sub_16", 0, 16, 0},

      {"fixup_linxv5_add_32", 0, 32, 0},
      {"fixup_linxv5_sub_32", 0, 32, 0},

      {"fixup_linxv5_add_64", 0, 64, 0},
      {"fixup_linxv5_sub_64", 0, 64, 0},

      {"fixup_linxv5_relax", 0, 0, 0},
      {"fixup_linxv5_align", 0, 0, 0},
      {"fixup_linxv5_tprel_hi20", 12, 20, 0},
      {"fixup_linxv5_tprel_lo12_i", 20, 12, 0},
      {"fixup_linxv5_tprel_lo12_l", 20, 12, 0},
      {"fixup_linxv5_tprel_lo12_s", 0, 32, 0},
  };
  static_assert((array_lengthof(Infos)) == LinxV5::NumTargetFixupKinds,
                "Not all fixup kinds added to Infos array");

  // Fixup kinds from .reloc directive are like R_LinxV5_NONE. They
  // do not require any extra processing.
  if (Kind >= FirstLiteralRelocationKind)
    return MCAsmBackend::getFixupKindInfo(FK_NONE);

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < getNumFixupKinds() &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

// If linker relaxation is enabled, or the relax option had previously been
// enabled, always emit relocations even if the fixup can be resolved. This is
// necessary for correctness as offsets may change during relaxation.
bool LinxV5AsmBackend::shouldForceRelocation(const MCAssembler &Asm,
                                             const MCFixup &Fixup,
                                             const MCValue &Target) {
  if (Fixup.getKind() >= FirstLiteralRelocationKind)
    return true;

  switch (Fixup.getTargetKind()) {
  default:
    break;
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    if (Target.isAbsolute())
      return false;
    break;
  }

  return true;
}

bool LinxV5AsmBackend::fixupNeedsRelaxationAdvanced(
    const MCFixup &Fixup, bool Resolved, uint64_t Value,
    const MCRelaxableFragment *DF, const MCAsmLayout &Layout,
    const bool WasForced) const {
  assert(0 && "Support relax!");
  return false;
}

void LinxV5AsmBackend::relaxInstruction(MCInst &Inst,
                                        const MCSubtargetInfo &STI) const {
  assert(0 && "Support relax!");
}

bool LinxV5AsmBackend::relaxDwarfLineAddr(MCDwarfLineAddrFragment &DF,
                                          MCAsmLayout &Layout,
                                          bool &WasRelaxed) const {
  // TODO: riscv has rewritten this interface, but it seems that Linx does not need it.
  // If there is a corresponding scenario, you can refer to the riscv modification.
  return false;
}

unsigned LinxV5AsmBackend::getRelaxedOpcode(unsigned Op) const { return Op; }

bool LinxV5AsmBackend::mayNeedRelaxation(const MCInst &Inst,
                                         const MCSubtargetInfo &STI) const {
  return getRelaxedOpcode(Inst.getOpcode()) != Inst.getOpcode();
}

bool LinxV5AsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                    const MCSubtargetInfo *STI) const {
  unsigned MinNopLen = 2;

  if ((Count % MinNopLen) != 0)
    return false;

  // The canonical align nop on LinxV5 is bstop.
  for (; Count >= 4; Count -= 4)
    OS.write("\x01\0\0\0", 4);

  // The canonical align c.nop on LinxV5 is c.bstop.
  if (Count)
    OS.write("\0\0", 2);

  return true;
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  switch (Fixup.getTargetKind()) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return Value;
  case LinxV5::fixup_linxv5_bnext:
    return (Value & 0x3fffffe) >> 1; // [25:1]
  }
}

void LinxV5AsmBackend::applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                                  const MCValue &Target,
                                  MutableArrayRef<char> Data, uint64_t Value,
                                  bool IsResolved,
                                  const MCSubtargetInfo *STI) const {
  MCFixupKind Kind = Fixup.getKind();
  if (Kind >= FirstLiteralRelocationKind)
    return;
  MCContext &Ctx = Asm.getContext();
  MCFixupKindInfo Info = getFixupKindInfo(Kind);
  if (!Value)
    return; // Doesn't change encoding.
  // Apply any target-specific value adjustments.
  Value = adjustFixupValue(Fixup, Value, Ctx);

  // Shift the value into position.
  Value <<= Info.TargetOffset;

  unsigned Offset = Fixup.getOffset();
  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;

  assert(Offset + NumBytes <= Data.size() && "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = Endian == support::little ? i : (NumBytes - 1 - i);
    Data[Offset + i] |= uint8_t((Value >> (Idx * 8)) & 0xff);
  }
}

// Linker relaxation may change code size. We have to insert Nops
// for .align directive when linker relaxation enabled. So then Linker
// could satisfy alignment by removing Nops.
// The function return the total Nops Size we need to insert.
bool LinxV5AsmBackend::shouldInsertExtraNopBytesForCodeAlign(
    const MCAlignFragment &AF, unsigned &Size) {
  // Calculate Nops Size only when linker relaxation enabled.
  const MCSubtargetInfo *STI = AF.getSubtargetInfo();
  if (!STI->getFeatureBits()[LinxV5::FeatureRelax])
    return false;

  unsigned MinNopLen = 2;

  if (AF.getAlignment() <= MinNopLen) {
    return false;
  } else {
    Size = AF.getAlignment().value() - MinNopLen;
    return true;
  }
}

// We need to insert R_LinxV5_ALIGN relocation type to indicate the
// position of Nops and the total bytes of the Nops have been inserted
// when linker relaxation enabled.
// The function insert fixup_linxv5_align fixup which eventually will
// transfer to R_LinxV5_ALIGN relocation type.
bool LinxV5AsmBackend::shouldInsertFixupForCodeAlign(MCAssembler &Asm,
                                                     const MCAsmLayout &Layout,
                                                     MCAlignFragment &AF) {
  // Insert the fixup only when linker relaxation enabled.
  const MCSubtargetInfo *STI = AF.getSubtargetInfo();
  if (!STI->getFeatureBits()[LinxV5::FeatureRelax])
    return false;

  // Calculate total Nops we need to insert. If there are none to insert
  // then simply return.
  unsigned Count;
  if (!shouldInsertExtraNopBytesForCodeAlign(AF, Count) || (Count == 0))
    return false;

  MCContext &Ctx = Asm.getContext();
  const MCExpr *Dummy = MCConstantExpr::create(0, Ctx);
  // Create fixup_linxv5_align fixup.
  MCFixup Fixup = MCFixup::create(
      0, Dummy, MCFixupKind(LinxV5::fixup_linxv5_align), SMLoc());

  uint64_t FixedValue = 0;
  MCValue NopBytes = MCValue::get(Count);

  Asm.getWriter().recordRelocation(Asm, Layout, &AF, Fixup, NopBytes,
                                   FixedValue);

  return true;
}

std::unique_ptr<MCObjectTargetWriter>
LinxV5AsmBackend::createObjectTargetWriter() const {
  return createLinxV5ELFObjectWriter(OSABI, Is64Bit);
}

MCAsmBackend *llvm::createLinxV5AsmBackend(const Target &T,
                                           const MCSubtargetInfo &STI,
                                           const MCRegisterInfo &MRI,
                                           const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new LinxV5AsmBackend(STI, OSABI, TT.isArch64Bit(), Options,
                              support::little);
}

MCAsmBackend *llvm::createLinxV5beAsmBackend(const Target &T,
                                             const MCSubtargetInfo &STI,
                                             const MCRegisterInfo &MRI,
                                             const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new LinxV5AsmBackend(STI, OSABI, TT.isArch64Bit(), Options,
                              support::big);
}
