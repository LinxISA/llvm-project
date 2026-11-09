#ifndef LLVM_SUPPORT_PTOISAIDENTITY_H
#define LLVM_SUPPORT_PTOISAIDENTITY_H

#include "llvm/ADT/StringRef.h"

namespace llvm {
namespace PTOISAIdentity {
constexpr StringLiteral Descriptor =
    "{\"encoding_abi\":\"pto-isa-0.59.0-mode-function-v1\","
    "\"encoding_projection_sha256\":"
    "\"4effea27acd51de4f637bddbee7d80e135c2d53775fa90e7d47c49041da8f693\","
    "\"release\":\"0.59.0\"}";
constexpr StringLiteral SpecCommit =
    "42067d0f2890b8c013a868bd1247ebc6437b3179";
}
}

#endif
