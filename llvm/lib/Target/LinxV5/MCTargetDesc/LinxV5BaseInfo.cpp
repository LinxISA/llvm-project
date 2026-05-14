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
