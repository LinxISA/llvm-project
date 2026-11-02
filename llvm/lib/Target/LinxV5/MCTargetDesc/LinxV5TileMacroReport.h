//===-- LinxV5TileMacroReport.h - TileOp macro coverage reports -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Diagnostics for TileOp bundles that the default macro disassembly view
// cannot print as a complete macro instruction.  Every occurrence is
// reported to stderr and disassembly continues, so one pass over an image
// surfaces every uncovered TileOp; the process-wide counter lets tools
// exit non-zero at the end.  The explicit no-tile-macros view bypasses
// these reports entirely and prints the physical form.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEMACROREPORT_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEMACROREPORT_H

#include "llvm/ADT/Twine.h"

namespace llvm {
namespace LinxV5 {

// Report a TileOp header that cannot fold into a complete macro, then let
// the caller continue in physical form.  HeaderName is the canonical
// assembly spelling of the header, Address locates it in the image.
void reportUnfoldableTileOp(const Twine &HeaderName, uint64_t Address);

// Report a TileOp bundle that folded but failed print-time validation.
void reportInvalidTileOpMacro(uint64_t Address);

// Number of coverage reports issued in this process so far.
unsigned tileOpCoverageReports();

} // namespace LinxV5
} // namespace llvm

// Weak-linked entry point for disassembly tools: the number of TileOp
// coverage reports issued in this process.  Tools built without the
// LinxV5 target resolve this symbol to null instead of failing to link.
extern "C" unsigned LinxV5TileOpCoverageReports();

#endif // LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEMACROREPORT_H
