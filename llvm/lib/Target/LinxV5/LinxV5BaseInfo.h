//===- LinxV5BaseInfo.h - Top level definitions for LinxV5 ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5BASEINFO_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5BASEINFO_H

#include "llvm/IR/CallingConv.h"
#include "llvm/Support/Alignment.h"

namespace llvm {

struct Align;
class Argument;
class Function;
class GlobalValue;
class MCRegisterClass;
class MCRegisterInfo;
class MCSubtargetInfo;
class StringRef;
class Triple;

namespace LinxV5 {

#include "LinxV5GenSearchableTables.inc"

/// \returns true if the intrinsic is divergent
bool isIntrinsicSourceOfDivergence(unsigned IntrID);

} // end namespace LinxV5

} // end namespace llvm

#endif // LLVM_LIB_TARGET_LINXV5_LINXV5BASEINFO_H
