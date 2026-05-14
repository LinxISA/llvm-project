//===-- LinxV5MCExpr.h - LinxV5 specific MC expression classes -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes LinxV5-specific MCExprs, used for modifiers like
// "%hi" or "%lo" etc.,
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4MCEXPR_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4MCEXPR_H

#include "llvm/MC/MCExpr.h"

namespace llvm {

class StringRef;

class LinxV5MCExpr : public MCTargetExpr {
public:
  enum VariantKind {
    VK_LinxV5_None,
    VK_LinxV5_TPCREL,
    VK_LinxV5_TPCREL_LO,
    VK_LinxV5_TPCREL_HI,
    VK_LinxV5_TPCREL_HI32,
    VK_LinxV5_TPREL,
    VK_LinxV5_TPREL_LO,
    VK_LinxV5_TPREL_HI,
    VK_LinxV5_CALL,
    VK_LinxV5_32_TPCREL,
    VK_LinxV5_Invalid
  };

  static const LinxV5MCExpr *create(const MCExpr *Expr, VariantKind Kind,
                                    MCContext &Ctx);

  VariantKind getKind() const { return Kind; }

  const MCExpr *getSubExpr() const { return Expr; }

  void printImpl(raw_ostream &OS, const MCAsmInfo *MAI) const override;
  bool evaluateAsRelocatableImpl(MCValue &Res, const MCAsmLayout *Layout,
                                 const MCFixup *Fixup) const override;
  void visitUsedExpr(MCStreamer &Streamer) const override;
  MCFragment *findAssociatedFragment() const override;

  void fixELFSymbolsInTLSFixups(MCAssembler &Asm) const override;

  static bool classof(const MCExpr *E) {
    return E->getKind() == MCExpr::Target;
  }

  static bool classof(const LinxV5MCExpr *) { return true; }

  static VariantKind getVariantKindForName(StringRef name);
  static StringRef getVariantKindName(VariantKind Kind);

private:
  const MCExpr *Expr;
  const VariantKind Kind;

  explicit LinxV5MCExpr(const MCExpr *Expr, VariantKind Kind)
      : Expr(Expr), Kind(Kind) {}
};

} // end namespace llvm.

#endif
