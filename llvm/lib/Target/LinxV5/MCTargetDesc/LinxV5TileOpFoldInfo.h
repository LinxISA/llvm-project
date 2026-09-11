//===-- LinxV5TileOpFoldInfo.h - TileOp bundle-fold side table --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// PTO 0.58.6 TileOp macro folding (issue #90): when the disassembler
// recognizes a complete physical bundle that exactly matches one schema
// plain form, it registers a fold description (macro spelling + the full
// bundle byte extent) keyed by the bundle's start address. The InstPrinter
// consults the table when printing the BSTART and renders the single macro
// line instead. The table lives behind a process-wide mutex because the
// disassembler and printer are const objects across concurrent uses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEOPFOLDINFO_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEOPFOLDINFO_H

#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace llvm {
namespace LinxV5TileOpFold {

struct FoldDesc {
  // Full macro spelling, e.g. "TADD" or "TMATMUL.SHARED_BOTH".
  std::string Spelling;
  // Rendered macro line (pre-formatted; the printer emits it verbatim).
  std::string Line;
  // Total folded bundle byte size (the disassembler's reported Size).
  uint64_t BundleBytes;
};

/// Register a fold at the given bundle start address.
void registerFold(uint64_t Address, FoldDesc Desc);

/// Look up (and consume) a fold registered for this address. Returns
/// nullptr when the address does not begin a folded bundle.
const FoldDesc *takeFold(uint64_t Address);

/// Drop all registered folds (called when a disassembly session starts).
void resetFolds();

} // namespace LinxV5TileOpFold
} // namespace llvm


namespace llvm {
namespace cl {
class opt<bool>;
}
namespace LinxV5TileOpFold {
/// The -linxv5-no-aliases physical-assembly escape hatch (issue #90).
/// Declared here so both the disassembler (folding decision) and the
/// printer (rendering decision) honor the same switch.
extern cl::opt<bool> NoAliases;
} // namespace LinxV5TileOpFold
} // namespace llvm

#endif
