//===-- LinxISATargetMachine.h - Define TargetMachine for LinxISA -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXISA_LINXISATARGETMACHINE_H
#define LLVM_LIB_TARGET_LINXISA_LINXISATARGETMACHINE_H

#include "LinxISASubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Support/Allocator.h"
#include <memory>

namespace llvm {

class Function;
struct MachineFunctionInfo;
class PassBuilder;
class TargetSubtargetInfo;

class LinxISATargetMachine : public CodeGenTargetMachineImpl {
  std::unique_ptr<TargetLoweringObjectFile> TLOF;
  LinxISASubtarget Subtarget;

public:
  LinxISATargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                       StringRef FS, const TargetOptions &Options,
                       std::optional<Reloc::Model> RM,
                       std::optional<CodeModel::Model> CM,
                       CodeGenOptLevel OL, bool JIT);

  const LinxISASubtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }

  MachineFunctionInfo *
  createMachineFunctionInfo(BumpPtrAllocator &Allocator, const Function &F,
                            const TargetSubtargetInfo *STI) const override;

  TargetTransformInfo getTargetTransformInfo(const Function &F) const override;
  void registerPassBuilderCallbacks(PassBuilder &PB) override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_LINXISA_LINXISATARGETMACHINE_H
