#ifndef LLVM_IR_LINX_UNALIGN_ACCESS_OPT
#define LLVM_IR_LINX_UNALIGN_ACCESS_OPT

#include "llvm/Pass.h"

namespace llvm {
FunctionPass *createLinxUnalignedAccessOptPass();
}

#endif
