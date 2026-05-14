//===- LinxV5MatInt.h - Immediate materialisation ------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MATINT_H
#define LLVM_LIB_TARGET_LINXV5_MATINT_H

#include "llvm/ADT/SmallVector.h"
#include <cstdint>

namespace llvm {
class APInt;

namespace LinxV5MatInt {
struct Inst {
  unsigned Opc;
  int64_t Imm;

  Inst(unsigned Opc, int64_t Imm) : Opc(Opc), Imm(Imm) {}
};
using InstSeq = SmallVector<Inst, 8>;

struct SIMTInst {
  unsigned Opc;
  unsigned DstType;
  unsigned SrcType;
  int64_t Imm;
  SIMTInst(unsigned Opc, unsigned DstType, unsigned SrcType, int64_t Imm)
      : Opc(Opc), DstType(DstType), SrcType(SrcType), Imm(Imm) {}
};
using SIMTInstSeq = SmallVector<SIMTInst, 8>;

} // namespace LinxV5MatInt
} // namespace llvm
#endif
