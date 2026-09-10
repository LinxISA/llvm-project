//===-- LinxV5TileOpMacro.cpp - TileOp macro mnemonic check ---*- C++ -*-===//

#include "LinxV5TileOpMacro.h"
#include "MCTargetDesc/LinxV5TileOpSchema.h"

using namespace llvm;

namespace llvm {
namespace LinxV5TileOpMacro {

bool isMacroMnemonic(StringRef Name) {
  return LinxV5TileOpSchema::lookupFormBySpelling(Name.str().c_str()) >= 0;
}

} // namespace LinxV5TileOpMacro
} // namespace llvm
