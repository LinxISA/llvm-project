//===-- LinxISA.h - Top-level interface for LinxISA ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISA_H
#define LLVM_LIB_TARGET_LINXISA_LINXISA_H

#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Pass.h"
#include <cstdint>

namespace llvm {

class LinxISATargetMachine;
class PassRegistry;

enum class LinxCodeSizeBalanceMode : uint8_t {
  Off,
  Balanced,
  StaticFirst,
  DynamicFirst,
};

FunctionPass *createLinxISAISelDag(LinxISATargetMachine &TM);
FunctionPass *createLinxISATileSSABalancePass();
FunctionPass *createLinxISAMemOpsCombinePass();
FunctionPass *createLinxISABlockifyPass();
FunctionPass *createLinxISASIMTAutoVectorizePass();

void initializeLinxISAAsmPrinterPass(PassRegistry &);
void initializeLinxISADAGToDAGISelLegacyPass(PassRegistry &);
void initializeLinxISATileSSABalancePass(PassRegistry &);
void initializeLinxISAMemOpsCombinePass(PassRegistry &);
void initializeLinxISABlockifyPass(PassRegistry &);
void initializeLinxISASIMTAutoVectorizePass(PassRegistry &);

bool linxEnableNegImmCanon();
bool linxEnableMaskSetcFold();
bool linxEnableSetcSrcRTypeFlags();
bool linxEnableCShift16();
bool linxEnableT1Motion();
LinxCodeSizeBalanceMode linxCodeSizeBalanceMode();

namespace LinxISD {

enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  CALL,
  RET_GLUE,

  BR_CC,
  SETCC,
};

} // namespace LinxISD

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISA_H
