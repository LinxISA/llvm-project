//===-- LinxV5MCAsmInfo.h - LinxV5 Asm Info -------------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the LinxV5MCAsmInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4MCASMINFO_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV4MCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {
class Triple;

class LinxV5MCAsmInfo : public MCAsmInfoELF {
public:
  explicit LinxV5MCAsmInfo(const Triple &TargetTriple);

  const MCExpr *getExprForFDESymbol(const MCSymbol *Sym, unsigned Encoding,
                                    MCStreamer &Streamer) const override;
};

} // namespace llvm

#endif
