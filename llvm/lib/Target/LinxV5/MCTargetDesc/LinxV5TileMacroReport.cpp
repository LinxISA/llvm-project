//===-- LinxV5TileMacroReport.cpp - TileOp macro coverage reports -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/LinxV5TileMacroReport.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <atomic>

namespace llvm {
namespace LinxV5 {

static std::atomic<unsigned> ReportCount{0};

static void issue(const Twine &Detail) {
  ++ReportCount;
  errs() << "LinxV5 error: " << Detail << "\n";
}

void reportUnfoldableTileOp(const Twine &HeaderName, uint64_t Address) {
  issue(Twine("TileOp header '") + HeaderName + "' at 0x" +
        utohexstr(Address) +
        " has no complete macro instruction form; disassembly continues in "
        "physical form (use --disassembler-options=no-tile-macros to inspect "
        "it)");
}

void reportInvalidTileOpMacro(uint64_t Address) {
  issue(Twine("TileOp macro at 0x") + utohexstr(Address) +
        " failed macro-form validation; printing '<incomplete TileOp "
        "macro>'");
}

unsigned tileOpCoverageReports() { return ReportCount.load(); }

} // namespace LinxV5
} // namespace llvm

extern "C" unsigned LinxV5TileOpCoverageReports() {
  return llvm::LinxV5::tileOpCoverageReports();
}
