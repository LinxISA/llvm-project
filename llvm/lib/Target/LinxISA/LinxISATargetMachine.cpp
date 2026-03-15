//===-- LinxISATargetMachine.cpp - Define TargetMachine for LinxISA -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISATargetMachine.h"
#include "LinxISA.h"
#include "LinxISASIMTAutoVectorize.h"
#include "LinxISAMachineFunctionInfo.h"
#include "LinxISATargetTransformInfo.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLinxISATarget() {
  RegisterTargetMachine<LinxISATargetMachine> X32(getTheLinx32Target());
  RegisterTargetMachine<LinxISATargetMachine> X64(getTheLinx64Target());

  PassRegistry &PR = *PassRegistry::getPassRegistry();
  initializeLinxISAAsmPrinterPass(PR);
  initializeLinxISATileSSABalancePass(PR);
  initializeLinxISAMemOpsCombinePass(PR);
  initializeLinxISABlockifyPass(PR);
  initializeLinxISASIMTAutoVectorizePass(PR);
  initializeLinxISADAGToDAGISelLegacyPass(PR);
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

static TargetOptions getEffectiveTargetOptions(const TargetOptions &Options) {
  TargetOptions Opts = Options;

  // LinxISA bring-up defaults to soft-float: the backend currently expands FP
  // operations to libcalls (e.g. __adddf3). If FloatABI stays as Default, the
  // runtime-libcalls set may not mark these implementations as available,
  // causing "unsupported library call operation" fatal errors during
  // legalization.
  if (Opts.FloatABIType == FloatABI::Default) {
    Opts.FloatABIType = FloatABI::Soft;
  }

  // Linx block templates rely on temporary (local) labels being available
  // during object emission for internal fixups/relaxation. Keep them by
  // default.
  Opts.MCOptions.MCSaveTempLabels = true;

  return Opts;
}

LinxISATargetMachine::LinxISATargetMachine(const Target &T, const Triple &TT,
                                           StringRef CPU, StringRef FS,
                                           const TargetOptions &Options,
                                           std::optional<Reloc::Model> RM,
                                           std::optional<CodeModel::Model> CM,
                                           CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(T, TT.computeDataLayout(), TT, CPU, FS,
                               getEffectiveTargetOptions(Options),
                               getEffectiveRelocModel(RM),
                               getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<TargetLoweringObjectFileELF>()),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo();
}

namespace {

class LinxISAPassConfig : public TargetPassConfig {
public:
  LinxISAPassConfig(LinxISATargetMachine &TM, PassManagerBase *PM)
      : TargetPassConfig(TM, *PM) {}

  LinxISATargetMachine &getLinxISATargetMachine() const {
    return getTM<LinxISATargetMachine>();
  }

  void addIRPasses() override {
    addPass(createAtomicExpandLegacyPass());
    TargetPassConfig::addIRPasses();
    if (getOptLevel() != CodeGenOptLevel::None &&
        getOptLevel() != CodeGenOptLevel::Less) {
      addPass(createLinxISASIMTAutoVectorizePass());
    }
  }

  bool addInstSelector() override {
    addPass(createLinxISAISelDag(getLinxISATargetMachine()));
    return false;
  }

  // Run once before ExpandPostRA so tile PHI/COPY traffic is materialized as
  // target pseudos and never reaches generic reg-to-reg COPY expansion.
  void addPostRegAlloc() override { addPass(createLinxISATileSSABalancePass()); }

  void addPreEmitPass() override {
    addPass(createLinxISATileSSABalancePass());
    if (getOptLevel() != CodeGenOptLevel::None)
      addPass(createLinxISAMemOpsCombinePass());
    addPass(createLinxISABlockifyPass());
  }
};

} // namespace

TargetPassConfig *LinxISATargetMachine::createPassConfig(PassManagerBase &PM) {
  return new LinxISAPassConfig(*this, &PM);
}

MachineFunctionInfo *LinxISATargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return new (Allocator.Allocate<LinxISAMachineFunctionInfo>())
      LinxISAMachineFunctionInfo(F, static_cast<const LinxISASubtarget *>(STI));
}

TargetTransformInfo
LinxISATargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<LinxISATTIImpl>(this, F));
}

void LinxISATargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerOptimizerLastEPCallback(
      [this](ModulePassManager &MPM, OptimizationLevel Level,
             ThinOrFullLTOPhase Phase) {
        (void)this;
        (void)Phase;
        if (Level == OptimizationLevel::O0 || Level == OptimizationLevel::O1)
          return;

        FunctionPassManager FPM;
        FPM.addPass(LinxISASIMTAutoVectorizePass());
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
      });
}
