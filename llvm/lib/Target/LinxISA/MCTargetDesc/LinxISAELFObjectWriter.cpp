//===-- LinxISAELFObjectWriter.cpp - LinxISA ELF Writer -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxISAFixupKinds.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

namespace {

class LinxISAELFObjectWriter : public MCELFObjectTargetWriter {
public:
  LinxISAELFObjectWriter(uint8_t OSABI, bool Is64Bit)
      : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_LINXISA,
                                /*HasRelocationAddend=*/true) {}

  ~LinxISAELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &,
                        bool IsPCRel) const override {
    // Map Linx fixup kinds to an ELF relocation type. This is a minimal set
    // sufficient for early bring-up (control-flow and basic data relocations).
    const unsigned Kind = Fixup.getKind();
    switch (Kind) {
    default:
      return ELF::R_LINX_NONE;
    case LinxISA::FIXUP_LINX_B12_PCREL:
      return ELF::R_LINX_B12_PCREL;
    case LinxISA::FIXUP_LINX_J22_PCREL:
      return ELF::R_LINX_J22_PCREL;
    case LinxISA::FIXUP_LINX_CBSTART12_PCREL:
      return ELF::R_LINX_CBSTART12_PCREL;
    case LinxISA::FIXUP_LINX_B17_PCREL:
      return ELF::R_LINX_B17_PCREL;
    case LinxISA::FIXUP_LINX_B17_PLT:
      return ELF::R_LINX_B17_PLT;
    case LinxISA::FIXUP_LINX_B25_PCREL:
      return ELF::R_LINX_B25_PCREL;
    case LinxISA::FIXUP_LINX_HL_BSTART30_PCREL:
      return ELF::R_LINX_HL_BSTART30_PCREL;
    case LinxISA::FIXUP_LINX_L_BSTART42_PCREL:
      return ELF::R_LINX_L_BSTART42_PCREL;
    case LinxISA::FIXUP_LINX_CSETRET5_PCREL:
      return ELF::R_LINX_CSETRET5_PCREL;
    case LinxISA::FIXUP_LINX_SETRET20_PCREL:
      return ELF::R_LINX_SETRET20_PCREL;
    case LinxISA::FIXUP_LINX_HL_SETRET32_PCREL:
      return ELF::R_LINX_HL_SETRET32_PCREL;
    case LinxISA::FIXUP_LINX_PCREL_HI20:
      return ELF::R_LINX_PCREL_HI20;
    case LinxISA::FIXUP_LINX_GOT_HI20:
      return ELF::R_LINX_GOT_HI20;
    case LinxISA::FIXUP_LINX_GOT_LO12:
      return ELF::R_LINX_GOT_LO12;
    case LinxISA::FIXUP_LINX_PCR17_LOAD:
      return ELF::R_LINX_PCR17_LOAD;
    case LinxISA::FIXUP_LINX_PCR17_STORE:
      return ELF::R_LINX_PCR17_STORE;
    case LinxISA::FIXUP_LINX_HL_PCR29_LOAD:
      return ELF::R_LINX_HL_PCR29_LOAD;
    case LinxISA::FIXUP_LINX_HL_PCR29_STORE:
      return ELF::R_LINX_HL_PCR29_STORE;
    case LinxISA::FIXUP_LINX_LO12:
      return ELF::R_LINX_LO12;
    case FK_Data_4:
      return IsPCRel ? ELF::R_LINX_32_PCREL : ELF::R_LINX_32;
    case FK_Data_8:
      return ELF::R_LINX_64;
    }
  }
};

} // namespace

std::unique_ptr<MCObjectTargetWriter>
llvm::createLinxISAELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<LinxISAELFObjectWriter>(OSABI, Is64Bit);
}
