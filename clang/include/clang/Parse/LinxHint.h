#ifndef LLVM_CLANG_SEMA_LINXHINT_H
#define LLVM_CLANG_SEMA_LINXHINT_H

#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Ownership.h"
#include "clang/Sema/ParsedAttr.h"

namespace clang {
struct LinxHint {
  SourceRange Range;
  // Identifier corresponding to the name of the pragma, which is "linx"
  IdentifierLoc *PragmaNameLoc;
  // Name of the linx option hint.  Examples: block
  IdentifierLoc *OptionLoc;
  // Identifier for the hint state argument. If null, then the state is default null
  IdentifierLoc *StateLoc;

  LinxHint()
      : PragmaNameLoc(nullptr), OptionLoc(nullptr), StateLoc(nullptr) {}
};

}
#endif // LLVM_CLANG_SEMA_LINXHINT_H