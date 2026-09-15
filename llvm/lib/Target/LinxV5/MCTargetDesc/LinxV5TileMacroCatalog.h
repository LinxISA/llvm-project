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

inline StringRef getTileMacroGPRLabel(const TileMacroBindingDesc &Binding) {
  StringRef Field(Binding.Field);
  if (Field == "RowStrideGPR")
    return "stride";
  return {};
}

inline StringRef getTileMacroGPRListLabel(const TileMacroBindingDesc &Binding,
                                          unsigned Member) {
  StringRef Syntax(Binding.Syntax);
  if (Member == 0 &&
      (Syntax.contains("BaseGPR") || Syntax.contains("GMBaseGPR")))
    return "base";
  if (Member == 1 && Syntax.contains("RowStrideGPR"))
    return "stride";
  if (Member == 1 && Syntax.contains("ShapeGPR"))
    return "shape";
  if (Member == 2 && Syntax.contains("StartGPR"))
    return "start";
  return {};
}

} // namespace llvm

#endif
