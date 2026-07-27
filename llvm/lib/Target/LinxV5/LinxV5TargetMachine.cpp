//===-- LinxV5TargetMachine.cpp - Define TargetMachine for LinxV5 --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about LinxV5 target spec.
//
//===----------------------------------------------------------------------===//

#include "LinxV5TargetMachine.h"
#include "LinxV5.h"
#include "LinxV5MachineScheduler.h"
#include "LinxV5TargetObjectFile.h"
#include "LinxV5TargetTransformInfo.h"
#include "MCTargetDesc/LinxV5BaseInfo.h"
#include "TargetInfo/LinxV5TargetInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;

cl::opt<bool> EnableClockHandSched("linxv5-enable-clock-hand-sched",
                                   cl::init(false));

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeLinxV5Target() {
  RegisterTargetMachine<LinxV5TargetMachine> V4(getTheLinx64V5Target());
  RegisterTargetMachine<LinxV5TargetMachine> BE(getTheLinx64V5beTarget());
  auto *PR = PassRegistry::getPassRegistry();
  initializeLinxV5CanonicalizeBlockPass(*PR);
  initializeLinxV5IsolateInlineASMBlockPass(*PR);
  initializeLinxV5FixSGPRCopiesPass(*PR);
  initializeLinxV5RegisterCanonicalizationPass(*PR);
  initializeLinxV5BGPRFixupPass(*PR);
  initializeLinxV5ClockhandsPreAllocPass(*PR);
  initializeLinxV5LoadStoreBridgeOptPass(*PR);
  initializeLinxV5ClockhandsPostAllocPass(*PR);
  initializeLinxV5ClockhandsColoringPass(*PR);
  initializeLinxV5TRegToOffsetOptPass(*PR);
  initializeLinxV5EmitHeaderPass(*PR);
  initializeLinxV5ExpandPseudoPass(*PR);
  initializeLinxV5AnnotateControlFlowPass(*PR);
  initializeLinxV5RebindGetTilePTRPass(*PR);
  initializeLinxV5ConstantRegOptPass(*PR);
  initializeLinxV5SIMTSpillFixupPass(*PR);
}

static StringRef computeDataLayout(const Triple &TT) {
  assert(TT.isArch64Bit() && "only 64-bits are currently supported");
  if (TT.isLittleEndian()) {
    return "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128";
  } else {
    return "E-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128";
  }
}

static Reloc::Model getEffectiveRelocModel(const Triple &TT,
                                           Optional<Reloc::Model> RM) {
  if (!RM.hasValue())
    return Reloc::Static;
  return *RM;
}

LinxV5TargetMachine::LinxV5TargetMachine(const Target &T, const Triple &TT,
                                         StringRef CPU, StringRef FS,
                                         const TargetOptions &Options,
                                         Optional<Reloc::Model> RM,
                                         Optional<CodeModel::Model> CM,
                                         CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(TT), TT, CPU, FS, Options,
                        getEffectiveRelocModel(TT, RM),
                        getEffectiveCodeModel(CM, CodeModel::Medium), OL),
      TLOF(std::make_unique<LinxV5ELFTargetObjectFile>()) {
  initAsmInfo();

  setMachineOutliner(false);
}

const LinxV5Subtarget *
LinxV5TargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  if (F.getFnAttribute("__vec__").isValid() ||
      F.getFnAttribute("__mtc__").isValid())
    CPU = "simt";

  FS = FS + ",+64bit";

  std::string Key = CPU + TuneCPU + FS;
  auto &I = SubtargetMap[Key];
  if (!I) {
    resetTargetOptions(F);
    auto ABIName = Options.MCOptions.getABIName();
    if (const MDString *ModuleTargetABI = dyn_cast_or_null<MDString>(
            F.getParent()->getModuleFlag("target-abi"))) {
      auto TargetABI = LinxV5ABI::getTargetABI(ABIName);
      if (TargetABI != LinxV5ABI::ABI_Unknown &&
          ModuleTargetABI->getString() != ABIName) {
        report_fatal_error("-target-abi option != target-abi module flag");
      }
      ABIName = ModuleTargetABI->getString();
    }
    I = std::make_unique<LinxV5Subtarget>(TargetTriple, CPU, TuneCPU, FS,
                                          ABIName, *this);
  }
  return I.get();
}

TargetTransformInfo
LinxV5TargetMachine::getTargetTransformInfo(const Function &F) const {
  if (F.getFnAttribute("__vec__").isValid() || F.getFnAttribute("__mtc__").isValid()) {
    return TargetTransformInfo(LinxV5VecTTIImpl(this, F));
  } else {
    return TargetTransformInfo(LinxV5TTIImpl(this, F));
  }
}

// A LinxV5 hart has a single byte-addressable address space of 2^XLEN bytes
// for all memory accesses, so it is reasonable to assume that an
// implementation has no-op address space casts. If an implementation makes a
// change to this, they can override it here.
bool LinxV5TargetMachine::isNoopAddrSpaceCast(unsigned SrcAS,
                                              unsigned DstAS) const {
  return true;
}

class LinxV5PassConfig : public TargetPassConfig {
public:
  LinxV5PassConfig(LinxV5TargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}
  LinxV5TargetMachine &getLinxV5TargetMachine() {
    return getTM<LinxV5TargetMachine>();
  }

  ScheduleDAGInstrs *
  createMachineScheduler(MachineSchedContext *C) const override {
    auto &STI = C->MF->getSubtarget<LinxV5Subtarget>();
    if (STI.isSIMT()) {
      if (!EnableClockHandSched)
        return nullptr;
    }

    ScheduleDAGMILive *DAG =
        new ScheduleDAGMILive(C, std::make_unique<LinxV5PreRASchedStrategy>(C));

    return DAG;
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
  void addPreRegAlloc() override;
  void addPostRegAlloc() override;
  void addRightBeforeRegAlloc() override;
  void addMachinePasses() override;
  void addPostRewrite() override;
  bool addRegAssignAndRewriteOptimized() override;
  bool addPreISel() override;
  void addCodeGenPrepare() override;
};

TargetPassConfig *LinxV5TargetMachine::createPassConfig(PassManagerBase &PM) {
  return new LinxV5PassConfig(*this, PM);
}

void LinxV5PassConfig::addIRPasses() {
  addPass(createAtomicExpandPass());
  // Call SeparateConstOffsetFromGEP pass to extract constants within indices
  // and lower a GEP with multiple indices to either arithmetic operations or
  // multiple GEPs with single index.
  addPass(createSeparateConstOffsetFromGEPPass(true));
  addPass(createLinxV5ConstantRegOptPass());
  TargetPassConfig::addIRPasses();
  addPass(createLinxV5RebindGetTilePTRPass());
}

void LinxV5PassConfig::addCodeGenPrepare() {
  TargetPassConfig::addCodeGenPrepare();
  addPass(createLinxV5ConstantRegOptPass());
}

bool LinxV5PassConfig::addInstSelector() {
  addPass(createLinxV5ISelDag(getLinxV5TargetMachine()));
  return false;
}

void LinxV5PassConfig::addMachinePasses() {
  addPass(createLinxV5CanonicalizeBlockPass(true)); // only duplicate const
  addPass(createLinxV5FixSGPRCopiesPass());
  addPass(&DeadMachineInstructionElimID); // DCE of VBXLowering
  TargetPassConfig::addMachinePasses();
}

void LinxV5PassConfig::addPreEmitPass() { addPass(&BranchRelaxationPassID); }

void LinxV5PassConfig::addPreEmitPass2() {
  addPass(createLinxV5PreEmitBlockOptPass());
  addPass(createLinxV5ExpandPseudoPass());
  addPass(createLinxV5TRegToOffsetOptPass());
  addPass(createLinxV5EmitHeaderPass());
}

static bool onlyAllocBGPR(const TargetRegisterInfo &TRI,
                          const TargetRegisterClass &RC) {
  return &RC == &LinxV5::GRRegClass || &RC == &LinxV5::GRNoRARegClass ||
         &RC == &LinxV5::GRNoR0RegClass;
}

bool LinxV5PassConfig::addRegAssignAndRewriteOptimized() {
  addPass(createGreedyRegisterAllocator(onlyAllocBGPR));

  addPass(createLinxV5IsolateInlineASMBlockPass());
  addPass(createVirtRegRewriter(false));

  addPass(createLinxV5ClockhandsPreAllocPass());
  addPass(createLinxV5BGPRFixupPass());
  addPass(&RegisterCoalescerID);
  addPass(createLinxV5LoadStoreBridgeOptPass());
  addPass(createLinxV5ClockhandsColoringPass());
  addPass(createGreedyRegisterAllocator());
  addPreRewrite();
  addPass(&VirtRegRewriterID);
  addPass(createLinxV5ClockhandsPostAllocPass());

  return true;
}

bool LinxV5PassConfig::addPreISel() {
  addPass(createStructurizeCFGPass(true)); // true -> SkipUniformRegions
  addPass(createLinxV5AnnotateControlFlowPass());
  return true;
}

void LinxV5PassConfig::addPostRewrite() {
  addPass(createLinxV5SIMTSpillFixupPass());
  addPass(createLinxV5StackSizeFixupPass());
  addPass(createLinxV5TileFixupPass());
}

void LinxV5PassConfig::addPostRegAlloc() {}

void LinxV5PassConfig::addPreRegAlloc() {
  addPass(createLinxV5CanonicalizeBlockPass());
}

void LinxV5PassConfig::addRightBeforeRegAlloc() {
  addPass(createLinxV5IsolateInlineASMBlockPass());
  addPass(createLinxV5RegisterCanonicalizationPass());
  addPass(createLinxV5ScrubRegsPass());
}
