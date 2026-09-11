//===-- LinxV5TileOpFoldInfo.cpp - fold side table impl ---------*- C++ -*-===//

#include "LinxV5TileOpFoldInfo.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {
namespace LinxV5TileOpFold {
cl::opt<bool> &NoAliases = NoAliasesImpl;

namespace {
cl::opt<bool>
    NoAliasesImpl("linxv5-no-aliases",
                  cl::desc("Disable the emission of assembler pseudo "
                           "instructions"),
                  cl::init(false), cl::Hidden);
std::mutex FoldMutex;
std::unordered_map<uint64_t, FoldDesc> Folds;
} // namespace

void registerFold(uint64_t Address, FoldDesc Desc) {
  std::lock_guard<std::mutex> Lock(FoldMutex);
  Folds[Address] = std::move(Desc);
}

const FoldDesc *takeFold(uint64_t Address) {
  std::lock_guard<std::mutex> Lock(FoldMutex);
  auto It = Folds.find(Address);
  if (It == Folds.end())
    return nullptr;
  return &It->second;
}

void resetFolds() {
  std::lock_guard<std::mutex> Lock(FoldMutex);
  Folds.clear();
}

} // namespace LinxV5TileOpFold
} // namespace llvm
