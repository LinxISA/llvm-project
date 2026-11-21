#ifndef LLVM_LIB_TARGET_LINXISA_MCTARGETDESC_LINXISASCOPEDSCALAR_H
#define LLVM_LIB_TARGET_LINXISA_MCTARGETDESC_LINXISASCOPEDSCALAR_H

#include "llvm/ADT/StringRef.h"
#include <algorithm>
#include <cstdint>
#include <iterator>

namespace llvm::LinxISA {

enum ScopedScalarFlags : unsigned {
  ScopeUnqualified = 0,
  ScopePrivate = 1,
  ScopeIntra = 2,
  ScopeInter = 3,
};

inline bool isScopedScalarForm(StringRef ID) {
  static constexpr StringRef Forms[] = {
#include "LinxISAScopedScalarForms.inc"
  };
  return std::binary_search(std::begin(Forms), std::end(Forms), ID);
}

inline StringRef getScopedScalarSuffix(unsigned Flags) {
  switch (Flags) {
  case ScopePrivate:
    return ".private";
  case ScopeIntra:
    return ".intra";
  case ScopeInter:
    return ".inter";
  default:
    return {};
  }
}

inline unsigned takeScopedScalarSuffix(StringRef &Mnemonic) {
  for (unsigned Scope : {ScopePrivate, ScopeIntra, ScopeInter}) {
    if (Mnemonic.consume_back_insensitive(getScopedScalarSuffix(Scope)))
      return Scope;
  }
  return ScopeUnqualified;
}

inline uint64_t getScopedPCRTarget(uint64_t Address, int64_t Displacement) {
  return (Address & ~uint64_t(3)) + static_cast<uint64_t>(Displacement) * 4;
}

} // namespace llvm::LinxISA

#endif
