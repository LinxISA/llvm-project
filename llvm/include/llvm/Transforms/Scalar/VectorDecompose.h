//===- VectorDecompose.h --- Decompose vector types -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// This pass decompose vector types to scalar
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_VECTORDECOMPOSE_H
#define LLVM_TRANSFORMS_SCALAR_VECTORDECOMPOSE_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class VectorDecomposePass : public PassInfoMixin<VectorDecomposePass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_SCALAR_VECTORDECOMPOSE_H
