//===-- LinxV5.h - Top-level interface for LinxV5 --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// LinxV5 back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV4_LINXV5_H
#define LLVM_LIB_TARGET_LINXV4_LINXV5_H

#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class LinxV5TargetMachine;
class AsmPrinter;
class FunctionPass;
class MCInst;
class MCOperand;
class MachineInstr;
class MachineOperand;
class PassRegistry;

void LowerLinxV5MachineInstrToMCInst(const MachineInstr *MI, MCInst &OutMI,
                                     const AsmPrinter &AP);
bool LowerLinxV5MachineOperandToMCOperand(const MachineOperand &MO,
                                          MCOperand &MCOp,
                                          const AsmPrinter &AP);

FunctionPass *createLinxV5ISelDag(LinxV5TargetMachine &TM);

FunctionPass *createLinxV5AnnotateControlFlowPass();
void initializeLinxV5AnnotateControlFlowPass(PassRegistry &);

FunctionPass *createLinxV5CanonicalizeBlockPass(bool dupConstOnly = false);
void initializeLinxV5CanonicalizeBlockPass(PassRegistry &);

FunctionPass *createLinxV5IsolateInlineASMBlockPass();
void initializeLinxV5IsolateInlineASMBlockPass(PassRegistry &);

FunctionPass *createLinxV5FixSGPRCopiesPass();
void initializeLinxV5FixSGPRCopiesPass(PassRegistry &);

FunctionPass *createLinxV5ScrubRegsPass(bool dceonly = false);
void initializeLinxV5ScrubRegsPass(PassRegistry &);

FunctionPass *createLinxV5ExpandPseudoPass();
void initializeLinxV5ExpandPseudoPass(PassRegistry &);

FunctionPass *createLinxV5ExpandAtomicPseudoPass();
void initializeLinxV5ExpandAtomicPseudoPass(PassRegistry &);

FunctionPass *createLinxV5LoadStoreOptimizationPass();
void initializeLinxV5LoadStoreOptPass(PassRegistry &);

FunctionPass *createLinxV5TRegToOffsetOptPass();
void initializeLinxV5TRegToOffsetOptPass(PassRegistry &);

FunctionPass *createLinxV5RegisterCanonicalizationPass();
void initializeLinxV5RegisterCanonicalizationPass(PassRegistry &);

FunctionPass *createLinxV5ClockhandsPreAllocPass();
void initializeLinxV5ClockhandsPreAllocPass(PassRegistry &);

FunctionPass *createLinxV5ClockhandsPostAllocPass();
void initializeLinxV5ClockhandsPostAllocPass(PassRegistry &);

FunctionPass *createLinxV5TileFixupPass();
void initializeLinxV5TileFixupPass(PassRegistry &);

FunctionPass *createLinxV5SIMTSpillFixupPass();
void initializeLinxV5SIMTSpillFixupPass(PassRegistry &);

FunctionPass *createLinxV5StackSizeFixupPass();
void initializeLinxV5StackSizeFixupPass(PassRegistry &);

FunctionPass *createLinxV5BGPRFixupPass();
void initializeLinxV5BGPRFixupPass(PassRegistry &);

FunctionPass *createLinxV5PreEmitBlockOptPass();
void initializeLinxV5PreEmitBlockOptPass(PassRegistry &);

FunctionPass *createLinxV5LoopIndVarPromotePass();
void initializeLinxV5LoopIndVarPromotePass(PassRegistry &);

FunctionPass *createLinxV5RevertCsePass();
void initializeLinxV5RevertCSEPass(PassRegistry &);

FunctionPass *createLinxV5LoadStoreBridgeOptPass();
void initializeLinxV5LoadStoreBridgeOptPass(PassRegistry &);

FunctionPass *createLinxV5ClockhandsColoringPass();
void initializeLinxV5ClockhandsColoringPass(PassRegistry &);

FunctionPass *createLinxV5SharedRegAllocPass();
void initializeLinxV5SharedRegAllocPass(PassRegistry &);

FunctionPass *createLinxV5EmitHeaderPass();
void initializeLinxV5EmitHeaderPass(PassRegistry &);

FunctionPass *createLinxV5AnnotateControlFlowPass();
void initializeLinxV5AnnotateControlFlowPass(PassRegistry&);
extern char &LinxV5AnnotateControlFlowPassID;

FunctionPass *createLinxV5RebindGetTilePTRPass();
void initializeLinxV5RebindGetTilePTRPass(PassRegistry &);

FunctionPass *createLinxV5ConstantRegOptPass();
void initializeLinxV5ConstantRegOptPass(PassRegistry &);
}
#endif
