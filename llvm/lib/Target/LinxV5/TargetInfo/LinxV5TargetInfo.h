//===-- LinxV5TargetInfo.h - LinxV5 Target Implementation ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_TARGETINFO_LINXTARGETINFO_H
#define LLVM_LIB_TARGET_LINXV5_TARGETINFO_LINXTARGETINFO_H

namespace llvm {

class Target;

Target &getTheLinx64V5Target();
Target &getTheLinx64V5beTarget();
} // namespace llvm

#endif // LLVM_LIB_TARGET_LINX_TARGETINFO_LINXTARGETINFO_H
