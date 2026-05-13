//===- LinxV5TargetMachine.h - Define TargetMachine for LinxV5 -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the LinxV5 specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV4_LINXV4TARGETMACHINE_H
#define LLVM_LIB_TARGET_LINXV4_LINXV4TARGETMACHINE_H

#include "LinxV5MachineScheduler.h"
#include "LinxV5Subtarget.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class LinxV5TargetMachine : public LLVMTargetMachine {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  mutable StringMap<std::unique_ptr<LinxV5Subtarget>> SubtargetMap;

public:
  LinxV5TargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);

  ~LinxV5TargetMachine() override = default;

  const LinxV5Subtarget *getSubtargetImpl(const Function &F) const override;
  // DO NOT IMPLEMENT: There is no such thing as a valid default subtarget,
  // subtargets are per-function entities based on the target-specific
  // attributes of each function.
  const LinxV5Subtarget *getSubtargetImpl() const = delete;

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  bool isNoopAddrSpaceCast(unsigned SrcAS, unsigned DstAS) const override;
};
} // namespace llvm

#endif
