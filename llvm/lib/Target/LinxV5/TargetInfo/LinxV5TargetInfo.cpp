//===-- LinxV5TargetInfo.cpp - LinxV5 Target Implementation --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheLinx64V5Target() {
  static Target TheLinx64V5Target;
  return TheLinx64V5Target;
}

Target &llvm::getTheLinx64V5beTarget() {
  static Target TheLinx64V5beTarget;
  return TheLinx64V5beTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5TargetInfo() {
  RegisterTarget<Triple::linx64v5> V4(getTheLinx64V5Target(), "linx64",
                                      "64-bit LinxISA", "LinxV5");
  RegisterTarget<Triple::linx64v5be> BE(getTheLinx64V5beTarget(), "linx64be",
                                        "64-bit LinxISA (big endian)",
                                        "LinxV5");
}
