//===-- LinxV5MCExpr.cpp - LinxV5 specific MC expression classes ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the assembly expression modifiers
// accepted by the LinxV5 architecture (e.g. ":lo12:", ":gottprel_g1:", ...).
//
//===----------------------------------------------------------------------===//

#include "LinxV5MCExpr.h"
#include "LinxV5FixupKinds.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "linxmcexpr"

const LinxV5MCExpr *LinxV5MCExpr::create(const MCExpr *Expr, VariantKind Kind,
                                         MCContext &Ctx) {
  return new (Ctx) LinxV5MCExpr(Expr, Kind);
}

void LinxV5MCExpr::printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const {
  VariantKind Kind = getKind();
  bool HasVariant = ((Kind != VK_LinxV5_None) && (Kind != VK_LinxV5_CALL));

  if (HasVariant)
    OS << '%' << getVariantKindName(getKind()) << '(';
  Expr->print(OS, MAI);
  if (HasVariant)
    OS << ')';
}

bool LinxV5MCExpr::evaluateAsRelocatableImpl(MCValue &Res,
                                             const MCAsmLayout *Layout,
                                             const MCFixup *Fixup) const {
  // Explicitly drop the layout and assembler to prevent any symbolic folding in
  // the expression handling.  This is required to preserve symbolic difference
  // expressions to emit the paired relocations.
  if (!getSubExpr()->evaluateAsRelocatable(Res, nullptr, nullptr))
    return false;

  Res =
      MCValue::get(Res.getSymA(), Res.getSymB(), Res.getConstant(), getKind());
  // Custom fixup types are not valid with symbol difference expressions.
  return Res.getSymB() ? getKind() == VK_LinxV5_None : true;
}

void LinxV5MCExpr::visitUsedExpr(MCStreamer &Streamer) const {
  Streamer.visitUsedExpr(*getSubExpr());
}

MCFragment *LinxV5MCExpr::findAssociatedFragment() const {
  llvm_unreachable("unsupported LinxV5MCExpr::findAssociatedFragment");
}

LinxV5MCExpr::VariantKind LinxV5MCExpr::getVariantKindForName(StringRef name) {
  return StringSwitch<LinxV5MCExpr::VariantKind>(name)
      .Case("tpcrel_lo", VK_LinxV5_TPCREL_LO)
      .Case("tpcrel_hi", VK_LinxV5_TPCREL_HI)
      .Case("tpcrel_hi32", VK_LinxV5_TPCREL_HI32)
      .Case("tprel_lo", VK_LinxV5_TPREL_LO)
      .Case("tprel_hi", VK_LinxV5_TPREL_HI)
      .Default(VK_LinxV5_Invalid);
}

StringRef LinxV5MCExpr::getVariantKindName(VariantKind Kind) {
  switch (Kind) {
  case VK_LinxV5_TPCREL:
    return "tpcrel";
  case VK_LinxV5_TPREL_LO:
    return "tprel_lo";
  case VK_LinxV5_TPREL_HI:
    return "tprel_hi";
  case VK_LinxV5_TPCREL_LO:
    return "tpcrel_lo";
  case VK_LinxV5_TPCREL_HI:
    return "tpcrel_hi";
  case VK_LinxV5_TPCREL_HI32:
    return "tpcrel_hi32";
  case VK_LinxV5_32_TPCREL:
    return "32_tpcrel";
  default:
    llvm_unreachable("Invalid ELF symbol kind");
  }
}

static void fixELFSymbolsInTLSFixupsImpl(const MCExpr *Expr, MCAssembler &Asm) {
  switch (Expr->getKind()) {
  case MCExpr::Target:
    llvm_unreachable("Can't handle nested target expression");
    break;
  case MCExpr::Constant:
    break;

  case MCExpr::Binary: {
    const MCBinaryExpr *BE = cast<MCBinaryExpr>(Expr);
    fixELFSymbolsInTLSFixupsImpl(BE->getLHS(), Asm);
    fixELFSymbolsInTLSFixupsImpl(BE->getRHS(), Asm);
    break;
  }

  case MCExpr::SymbolRef: {
    // We're known to be under a TLS fixup, so any symbol should be
    // modified. There should be only one.
    const MCSymbolRefExpr &SymRef = *cast<MCSymbolRefExpr>(Expr);
    cast<MCSymbolELF>(SymRef.getSymbol()).setType(ELF::STT_TLS);
    break;
  }

  case MCExpr::Unary:
    fixELFSymbolsInTLSFixupsImpl(cast<MCUnaryExpr>(Expr)->getSubExpr(), Asm);
    break;
  }
}

void LinxV5MCExpr::fixELFSymbolsInTLSFixups(MCAssembler &Asm) const {
  switch (getKind()) {
  default:
    return;
  // TODO: LinxV5 will define VK_LinxV5_TPRelXXX for TLS fix-up.
  case VK_LinxV5_TPREL:
  case VK_LinxV5_TPREL_LO:
  case VK_LinxV5_TPREL_HI:
    break;
  }

  fixELFSymbolsInTLSFixupsImpl(getSubExpr(), Asm);
}
