//===-- LinxV5TileOpMacro.h - single-line TileOp macro support ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// PTO 0.58.6 single-line TileOp macro-assembly (issue #90). The parsing
// and expansion live as a member of LinxV5AsmParser (see
// LinxV5AsmParser.cpp) so they can reuse its private lexing and emission
// helpers; this header exposes the mnemonic check used by the
// ParseInstruction interception.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_ASMPARSER_LINXV5TILEOPMACRO_H
#define LLVM_LIB_TARGET_LINXV5_ASMPARSER_LINXV5TILEOPMACRO_H

#include "llvm/ADT/StringRef.h"

namespace llvm {
namespace LinxV5TileOpMacro {

/// True when Name is one of the 141 TileOp macro spellings (including
/// dotted forms such as TMATMUL.SHARED_BOTH).
bool isMacroMnemonic(StringRef Name);

} // namespace LinxV5TileOpMacro
} // namespace llvm

#endif
