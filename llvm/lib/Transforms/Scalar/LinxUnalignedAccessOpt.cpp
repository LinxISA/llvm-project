#include "llvm/Transforms/Scalar/LinxUnalignedAccessOpt.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"

// TODO:
// This is a quick implementation for performance evaluation.
// Align/Unalign access is an architecture-dependent optimization. It's more
// appropriate to implement in instruction selection as sub-target feature.

using namespace llvm;

#define DEBUG_TYPE "unaligned-access"
#define LINX_UNALIGN_ACCESS_OPT_NAME  "Linx Unaligned Access Opt"

static cl::opt<bool> EnableUnalignedAccess("enable-unaligned-access",
                                           cl::init(false), cl::Hidden);

namespace {

class LinxUnalignedAccessOpt : public FunctionPass {
public:
  static char ID;

  LinxUnalignedAccessOpt() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!EnableUnalignedAccess) {
      return false;
    }

    bool Changed = false;
    for (BasicBlock &BB : F) {
      Changed |= RelaxAlignInfo(BB);
    }

    return Changed;
  }

  StringRef getPassName() const override {
    return LINX_UNALIGN_ACCESS_OPT_NAME;
  }

private:
  bool RelaxAlignInfo(BasicBlock &BB) const;
};
} // namespace

char LinxUnalignedAccessOpt::ID = 0;

INITIALIZE_PASS_BEGIN(LinxUnalignedAccessOpt, DEBUG_TYPE,
                      LINX_UNALIGN_ACCESS_OPT_NAME, false, false)
INITIALIZE_PASS_END(LinxUnalignedAccessOpt, DEBUG_TYPE,
                    LINX_UNALIGN_ACCESS_OPT_NAME, false, false)

FunctionPass *llvm::createLinxUnalignedAccessOptPass() {
  return new LinxUnalignedAccessOpt();
}

bool LinxUnalignedAccessOpt::RelaxAlignInfo(BasicBlock &BB) const {
  bool Changed = false;
  for (Instruction &I : BB) {
    if (isa<LoadInst>(I)) {
      LoadInst *LI = dyn_cast<LoadInst>(&I);
      Align Alignment = LI->getAlign();
      unsigned LoadBytes = LI->getType()->getPrimitiveSizeInBits() / 8;
      if (LoadBytes > Alignment.value()) {
        unsigned MaxAlign = 8;
        LI->setAlignment(Align(MaxAlign));
        Changed = true;
      }
    }
  }
  return Changed;
}
