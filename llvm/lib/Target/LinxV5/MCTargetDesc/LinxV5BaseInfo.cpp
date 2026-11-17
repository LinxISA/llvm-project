//===-- LinxV5BaseInfo.cpp - Top level definitions for LinxV5 MC ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the LinxV5 target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//

#include "LinxV5BaseInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/LinxV5ISAInfo.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

namespace LinxV5Shareability {
unsigned scopedOpcode(unsigned Base, unsigned Scope) {
  if (Scope > 2)
    return 0;
  switch (Base) {
#define SCOPED_SCALAR(B, P, I, E)                                              \
  case LinxV5::B: {                                                            \
    const unsigned Ops[] = {LinxV5::P, LinxV5::I, LinxV5::E};                  \
    return Ops[Scope];                                                         \
  }
#define CG_SCOPED_SCALAR(B, P, I, E)
#include "LinxV5ScopedScalar.inc"
#undef CG_SCOPED_SCALAR
#undef SCOPED_SCALAR
  default:
    return 0;
  }
}
unsigned codegenOpcode(unsigned Base, unsigned Scope) {
  if (Scope > 2)
    return 0;
  switch (Base) {
#define SCOPED_SCALAR(B, P, I, E)
#define CG_SCOPED_SCALAR(B, P, I, E)                                           \
  case LinxV5::B: {                                                            \
    const unsigned Ops[] = {LinxV5::P, LinxV5::I, LinxV5::E};                  \
    return Ops[Scope];                                                         \
  }
#include "LinxV5ScopedScalar.inc"
#undef CG_SCOPED_SCALAR
#undef SCOPED_SCALAR
  default:
    return 0;
  }
}
unsigned baseOpcode(unsigned Scoped) {
  switch (Scoped) {
#define SCOPED_SCALAR(B, P, I, E)                                              \
  case LinxV5::P:                                                              \
  case LinxV5::I:                                                              \
  case LinxV5::E:                                                              \
    return LinxV5::B;
#define CG_SCOPED_SCALAR SCOPED_SCALAR
#include "LinxV5ScopedScalar.inc"
#undef CG_SCOPED_SCALAR
#undef SCOPED_SCALAR
  default:
    return 0;
  }
}
} // namespace LinxV5Shareability

namespace LinxV5ABI {
ABI computeTargetABI(const Triple &TT, FeatureBitset FeatureBits,
                     StringRef ABIName) {
  auto TargetABI = getTargetABI(ABIName);
  if (TargetABI != ABI_Unknown)
    return TargetABI;

  // If no explicit ABI is given, try to compute the default ABI.
  return ABI_LP64;
}

ABI getTargetABI(StringRef ABIName) {
  auto TargetABI =
      StringSwitch<ABI>(ABIName).Case("lp64", ABI_LP64).Default(ABI_Unknown);
  return TargetABI;
}

// To avoid the BP value clobbered by a function call, we need to choose a
// callee saved register to save the value. We choose R6 as bp.
MCRegister getBPReg() { return LinxV5::R6; } // s1

} // namespace LinxV5ABI

namespace LinxV5TileCall {
#define GET_TileCallTable_IMPL
using namespace LinxV5;
#include "LinxV5GenSearchableTables.inc"
} // namespace LinxV5TileCall

namespace LinxV5Features {

void validate(const Triple &TT, const FeatureBitset &FeatureBits) {
  // Do we need this check?
}

} // namespace LinxV5Features

} // namespace llvm
