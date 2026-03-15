//===- LinxISASIMTAutoVectorize.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISASIMTAUTOVECTORIZER_H
#define LLVM_LIB_TARGET_LINXISA_LINXISASIMTAUTOVECTORIZER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class Function;
class FunctionPass;

bool linxSIMTAutoVectorizeEnabled();
StringRef linxSIMTAutoVectorizeMode();
StringRef linxSIMTAutoVectorizeRemarksPath();

FunctionPass *createLinxISASIMTAutoVectorizePass();

class LinxISASIMTAutoVectorizePass
    : public PassInfoMixin<LinxISASIMTAutoVectorizePass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISASIMTAUTOVECTORIZER_H
