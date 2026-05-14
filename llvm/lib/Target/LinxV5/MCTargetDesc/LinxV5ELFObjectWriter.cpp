//===-- LinxV5ELFObjectWriter.cpp - LinxV5 ELF Writer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxV5FixupKinds.h"
#include "MCTargetDesc/LinxV5MCExpr.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

namespace {
class LinxV5ELFObjectWriter : public MCELFObjectTargetWriter {
public:
  LinxV5ELFObjectWriter(uint8_t OSABI, bool Is64Bit);

  ~LinxV5ELFObjectWriter() override;

  // Return true if the given relocation must be with a symbol rather than
  // section plus offset.
  bool needsRelocateWithSymbol(const MCSymbol &Sym,
                               unsigned Type) const override {
    return true;
  }

protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override;
};
} // namespace

LinxV5ELFObjectWriter::LinxV5ELFObjectWriter(uint8_t OSABI, bool Is64Bit)
    : MCELFObjectTargetWriter(Is64Bit, OSABI, ELF::EM_LINXISA,
                              /*HasRelocationAddend*/ true) {}

LinxV5ELFObjectWriter::~LinxV5ELFObjectWriter() = default;

unsigned LinxV5ELFObjectWriter::getRelocType(MCContext &Ctx,
                                             const MCValue &Target,
                                             const MCFixup &Fixup,
                                             bool IsPCRel) const {
  const MCExpr *Expr = Fixup.getValue();
  // Determine the type of the relocation
  unsigned Kind = Fixup.getTargetKind();
  if (Kind >= FirstLiteralRelocationKind)
    return Kind - FirstLiteralRelocationKind;
  if (IsPCRel) {
    switch (Kind) {
    default:
      Ctx.reportError(Fixup.getLoc(), "Unsupported relocation type");
      return ELF::R_LinxV5_NONE;
    case LinxV5::fixup_linxv5_bnext:
      return ELF::R_LinxV5_BNEXT;
    case LinxV5::fixup_linxv5_32_bnext:
      return ELF::R_LinxV5_32_BNEXT;
    case LinxV5::fixup_linxv5_48_bnext:
      return ELF::R_LinxV5_48_BNEXT;
    case LinxV5::fixup_linxv5_64_bnext:
      return ELF::R_LinxV5_64_BNEXT;
    case LinxV5::fixup_linxv5_bnext_c:
      return ELF::R_LinxV5_BNEXT_C;
    case LinxV5::fixup_linxv5_btext:
      return ELF::R_LinxV5_BTEXT;
    case LinxV5::fixup_linxv5_c_addpc:
      return ELF::R_LinxV5_C_ADDPC;
    case LinxV5::fixup_linxv5_addpc:
      return ELF::R_LinxV5_ADDPC;
    case LinxV5::fixup_linxv5_hlsetret:
      return ELF::R_LinxV5_HLSETRET;
    case LinxV5::fixup_linxv5_simt_branch:
      return ELF::R_LinxV5_SIMT_BRANCH;
    case LinxV5::fixup_linxv5_simt_branch_rc:
      return ELF::R_LinxV5_SIMT_BRANCH_RC;
    case LinxV5::fixup_linxv5_simt_jump:
      return ELF::R_LinxV5_SIMT_JUMP;
    case LinxV5::fixup_linxv5_simt_tpcrel_lo12_i:
      return ELF::R_LinxV5_SIMT_TPCREL_LO12_I;
    case LinxV5::fixup_linxv5_simt_tpcrel_lo12_l:
      return ELF::R_LinxV5_SIMT_TPCREL_LO12_L;
    case LinxV5::fixup_linxv5_simt_tpcrel_lo12_s:
      return ELF::R_LinxV5_SIMT_TPCREL_LO12_S;
    case LinxV5::fixup_linxv5_tpcrel_hi20:
      return ELF::R_LinxV5_TPCREL_HI20;
    case LinxV5::fixup_linxv5_tpcrel_hi32:
      return ELF::R_LinxV5_TPCREL_HI32;
    case LinxV5::fixup_linxv5_tpcrel_lo12_i:
      return ELF::R_LinxV5_TPCREL_LO12_I;
    case LinxV5::fixup_linxv5_tpcrel_lo12_l:
      return ELF::R_LinxV5_TPCREL_LO12_L;
    case LinxV5::fixup_linxv5_tpcrel_lo12_s:
      return ELF::R_LinxV5_TPCREL_LO12_S;
    case LinxV5::fixup_linxv5_add_8:
      return ELF::R_LinxV5_ADD8;
    case LinxV5::fixup_linxv5_sub_8:
      return ELF::R_LinxV5_SUB8;
    case LinxV5::fixup_linxv5_add_16:
      return ELF::R_LinxV5_ADD16;
    case LinxV5::fixup_linxv5_sub_16:
      return ELF::R_LinxV5_SUB16;
    case LinxV5::fixup_linxv5_add_32:
      return ELF::R_LinxV5_ADD32;
    case LinxV5::fixup_linxv5_sub_32:
      return ELF::R_LinxV5_SUB32;
    case LinxV5::fixup_linxv5_add_64:
      return ELF::R_LinxV5_ADD64;
    case LinxV5::fixup_linxv5_sub_64:
      return ELF::R_LinxV5_SUB64;
    case LinxV5::fixup_linxv5_load_symbol:
      return ELF::R_LinxV5_Load_Symbol;
    case LinxV5::fixup_linxv5_store_symbol:
      return ELF::R_LinxV5_Store_Symbol;
    case LinxV5::fixup_linxv5_branch:
      return ELF::R_LinxV5_BRANCH;
    case LinxV5::fixup_linxv5_load_symbol_target_42:
      return ELF::R_LinxV5_Load_Symbol_Target_42;
    case LinxV5::fixup_linxv5_store_symbol_target_42:
      return ELF::R_LinxV5_Store_Symbol_Target_42;
    case LinxV5::fixup_linxv5_branch_22:
      return ELF::R_LinxV5_BRANCH_22;
    case LinxV5::fixup_linxv5_load_symbol_target_29:
      return ELF::R_LinxV5_Load_Symbol_Target_29;
    case LinxV5::fixup_linxv5_store_symbol_target_29:
      return ELF::R_LinxV5_Store_Symbol_Target_29;
    case LinxV5::fixup_linxv5_stack_size:
      return ELF::R_LinxV5_STACK_SIZE;
    }
  }

  switch (Kind) {
  default:
    Ctx.reportError(Fixup.getLoc(), "Unsupported relocation type");
    return ELF::R_LinxV5_NONE;
  case FK_Data_4:
    if (Expr->getKind() == MCExpr::Target &&
        cast<LinxV5MCExpr>(Expr)->getKind() ==
            LinxV5MCExpr::VK_LinxV5_32_TPCREL)
      return ELF::R_LinxV5_32_PCREL;
    return ELF::R_LinxV5_32;
  case FK_Data_8:
    return ELF::R_LinxV5_64;
  case LinxV5::fixup_linxv5_add_8:
    return ELF::R_LinxV5_ADD8;
  case LinxV5::fixup_linxv5_sub_8:
    return ELF::R_LinxV5_SUB8;
  case LinxV5::fixup_linxv5_add_16:
    return ELF::R_LinxV5_ADD16;
  case LinxV5::fixup_linxv5_sub_16:
    return ELF::R_LinxV5_SUB16;
  case LinxV5::fixup_linxv5_add_32:
    return ELF::R_LinxV5_ADD32;
  case LinxV5::fixup_linxv5_sub_32:
    return ELF::R_LinxV5_SUB32;
  case LinxV5::fixup_linxv5_add_64:
    return ELF::R_LinxV5_ADD64;
  case LinxV5::fixup_linxv5_sub_64:
    return ELF::R_LinxV5_SUB64;
  case LinxV5::fixup_linxv5_align:
    return ELF::R_LinxV5_ALIGN;
  case LinxV5::fixup_linxv5_relax:
    return ELF::R_LinxV5_RELAX;
  case LinxV5::fixup_linxv5_tprel_hi20:
    return ELF::R_LinxV5_TPREL_HI20;
  case LinxV5::fixup_linxv5_tprel_lo12_i:
    return ELF::R_LinxV5_TPREL_LO12_I;
  case LinxV5::fixup_linxv5_tprel_lo12_l:
    return ELF::R_LinxV5_TPREL_LO12_L;
  case LinxV5::fixup_linxv5_tprel_lo12_s:
    return ELF::R_LinxV5_TPREL_LO12_S;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createLinxV5ELFObjectWriter(uint8_t OSABI, bool Is64Bit) {
  return std::make_unique<LinxV5ELFObjectWriter>(OSABI, Is64Bit);
}
