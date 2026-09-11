//===- LinxV5TileMacroCatalog.h - PTO TileOp macro metadata ----*- C++ -*-===//

#ifndef LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEMACROCATALOG_H
#define LLVM_LIB_TARGET_LINXV5_MCTARGETDESC_LINXV5TILEMACROCATALOG_H

#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace llvm {

#include "LinxV5TileMacroCatalog.inc"

inline const TileMacroOperationDesc *
findTileMacroOperation(TileMacroHeaderKind Kind, unsigned Selector) {
  for (const TileMacroOperationDesc &Operation : TileMacroOperations)
    if (Operation.HeaderKind == Kind && Operation.Selector == Selector)
      return &Operation;
  return nullptr;
}

inline const TileMacroOperationDesc *
findTileMacroOperation(StringRef Mnemonic) {
  for (const TileMacroOperationDesc &Operation : TileMacroOperations)
    if (Mnemonic.equals_insensitive(Operation.Mnemonic))
      return &Operation;
  return nullptr;
}

inline const TileMacroOperationDesc *
findTileMacroOperationForSpelling(StringRef Spelling) {
  for (const TileMacroOperationDesc &Operation : TileMacroOperations) {
    for (unsigned I = 0; I != Operation.NumForms; ++I) {
      const TileMacroFormDesc &Form = TileMacroForms[Operation.FirstForm + I];
      if (Spelling.equals_insensitive(Form.Spelling))
        return &Operation;
    }
  }
  return nullptr;
}

} // namespace llvm

#endif
