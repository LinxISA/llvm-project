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
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/SROA.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

cl::opt<bool> EnableClockHandSched("linxv5-enable-clock-hand-sched",
                                   cl::init(false));
namespace llvm {
cl::opt<bool> EnableGenericElement(
    "linxv5-enable-generic-element", cl::init(false),
    cl::desc("Use generic CFG/VP predication and explicit PTO Tile legalization"));
} // namespace llvm

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
  initializeLinxV5ScalarShareabilityPass(*PR);
  initializeLinxV5ClockhandsPostAllocPass(*PR);
  initializeLinxV5ClockhandsColoringPass(*PR);
  initializeLinxV5SharedRegAllocPass(*PR);
  initializeLinxV5SharedHandleLegalizePass(*PR);
  initializeLinxV5SharedCopyElimPass(*PR);
  initializeLinxV5TRegToOffsetOptPass(*PR);
  initializeLinxV5SharedReuseFinalizePass(*PR);
  initializeLinxV5TileFixupPass(*PR);
  initializeLinxV5TileDefinednessPass(*PR);
  initializeLinxV5EmitHeaderPass(*PR);
  initializeLinxV5ExpandPseudoPass(*PR);
  initializeLinxV5AnnotateControlFlowPass(*PR);
  initializeLinxV5ElementwiseMaskPass(*PR);
  initializeLinxV5ElementRegionPrepareLegacyPassPass(*PR);
  initializeLinxV5ElementRegionLegacyPassPass(*PR);
  initializeLinxV5ElementRegionVerifierLegacyPassPass(*PR);
  initializeLinxV5PEDomainRawVerifierLegacyPassPass(*PR);
  initializeLinxV5RebindGetTilePTRPass(*PR);
  initializeLinxV5ConstantRegOptPass(*PR);
  initializeLinxV5SIMTSpillFixupPass(*PR);
}

namespace {
// SROA can expose an aggregate ABI's pointer -> integer -> pointer round trip.
// Fold only address/aggregate plumbing, then promote the newly visible local
// carrier. Do not run memory optimizations before request scopes are assigned.
struct LinxV5CarrierPreparePass : PassInfoMixin<LinxV5CarrierPreparePass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
    bool Changed = false, Again;
    do {
      Again = false;
      for (Instruction &I : make_early_inc_range(instructions(F))) {
        if (!isa<CastInst>(I) && !isa<ExtractValueInst>(I) &&
            !isa<InsertValueInst>(I) && !isa<GetElementPtrInst>(I))
          continue;
        if (Value *V = simplifyInstruction(
                &I, SimplifyQuery(F.getParent()->getDataLayout()))) {
          I.replaceAllUsesWith(V);
          I.eraseFromParent();
          Again = Changed = true;
        } else if (I.use_empty()) {
          I.eraseFromParent();
          Again = Changed = true;
        }
      }
    } while (Again);
    if (Changed)
      AM.invalidate(F, PreservedAnalyses::none());
    auto PA = SROAPass().run(F, AM);
    return Changed ? PreservedAnalyses::none() : PA;
  }
};

// Ordinary inline shape factories must disappear before their sret stores are
// scoped. Once converted to Scalar GM intrinsics those stores cannot be
// promoted, even when later inlining exposes a constant asm operand. Restrict
// this preparation to small scalar computations: never optimize real GM
// accesses, Tile/Shared operations or explicit scope/domain boundaries here.
struct LinxV5InlineAsmConstantPreparePass
    : PassInfoMixin<LinxV5InlineAsmConstantPreparePass> {
  static bool isShapeFactory(Function &F, SmallPtrSetImpl<Function *> &Active,
                             unsigned &Budget, unsigned Depth = 0) {
    if (F.isDeclaration() || F.isVarArg() || Depth == 8 ||
        F.getInstructionCount() > Budget ||
        F.hasFnAttribute(Attribute::NoInline) ||
        F.hasFnAttribute(Attribute::OptimizeNone) ||
        F.hasFnAttribute("linx-scalar-shareability") ||
        F.hasFnAttribute("linx-tile-pe-domain") ||
        F.hasFnAttribute("target-cpu") || !Active.insert(&F).second)
      return false;
    Budget -= F.getInstructionCount();
    auto Allowed = [&]() {
      for (Instruction &I : instructions(F)) {
        if (auto *L = dyn_cast<LoadInst>(&I)) {
          if (!L->isSimple() || !L->getType()->isSingleValueType() ||
              L->getType()->isVectorTy() ||
              !isa<AllocaInst>(getUnderlyingObject(L->getPointerOperand())))
            return false;
        } else if (auto *S = dyn_cast<StoreInst>(&I)) {
          Value *Object = getUnderlyingObject(S->getPointerOperand());
          auto *A = dyn_cast<Argument>(Object);
          if (!S->isSimple() || S->getValueOperand()->getType()->isVectorTy() ||
              (!isa<AllocaInst>(Object) && (!A || !A->hasStructRetAttr())))
            return false;
        } else if (auto *C = dyn_cast<CallBase>(&I)) {
          if (auto *II = dyn_cast<IntrinsicInst>(C)) {
            if (II->getIntrinsicID() == Intrinsic::lifetime_start ||
                II->getIntrinsicID() == Intrinsic::lifetime_end ||
                isa<DbgInfoIntrinsic>(II))
              continue;
            return false;
          }
          Function *Callee = C->getCalledFunction();
          if (!isa<CallInst>(C) || !Callee || C->isMustTailCall() ||
              C->hasFnAttr(Attribute::NoInline) ||
              C->hasFnAttr("linx-scalar-shareability") ||
              C->hasFnAttr("linx-tile-pe-domain") ||
              !isShapeFactory(*Callee, Active, Budget, Depth + 1))
            return false;
          // A nested sret factory may write only this factory's return
          // object or a temporary local object, never another pointer arg.
          for (unsigned N = 0; N != C->arg_size(); ++N) {
            if (!Callee->hasParamAttribute(N, Attribute::StructRet))
              continue;
            Value *Object = getUnderlyingObject(C->getArgOperand(N));
            auto *A = dyn_cast<Argument>(Object);
            if (!isa<AllocaInst>(Object) && (!A || !A->hasStructRetAttr()))
              return false;
          }
        } else if (I.mayReadOrWriteMemory() || I.mayHaveSideEffects() ||
                   I.getType()->isVectorTy() || isa<PtrToIntInst>(I) ||
                   isa<IntToPtrInst>(I)) {
          return false;
        }
      }
      return true;
    };
    bool Result = Allowed();
    Active.erase(&F);
    return Result;
  }

  static void collectFactories(Value *V, SmallPtrSetImpl<Value *> &Seen,
                               SmallPtrSetImpl<CallBase *> &Calls) {
    if (Seen.size() == 512 || !Seen.insert(V).second)
      return;
    auto *I = dyn_cast<Instruction>(V);
    if (!I)
      return;
    if (auto *C = dyn_cast<CallBase>(I))
      Calls.insert(C);
    if (auto *L = dyn_cast<LoadInst>(I)) {
      auto *A =
          dyn_cast<AllocaInst>(getUnderlyingObject(L->getPointerOperand()));
      if (A) {
        // Follow uses of the return buffer to discover its sret producer.
        SmallVector<Value *, 16> Work{A};
        SmallPtrSet<Value *, 32> Uses;
        while (!Work.empty() && Uses.size() < 128) {
          Value *P = Work.pop_back_val();
          if (!Uses.insert(P).second)
            continue;
          for (User *U : P->users()) {
            if (auto *C = dyn_cast<CallBase>(U))
              Calls.insert(C);
            else if (isa<GetElementPtrInst>(U) || isa<BitCastInst>(U))
              Work.push_back(cast<Value>(U));
          }
        }
      }
    }
    for (Value *Operand : I->operands())
      collectFactories(Operand, Seen, Calls);
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    auto &FAM =
        AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    bool Changed = false;
    for (unsigned Round = 0; Round != 8; ++Round) {
      SmallPtrSet<CallBase *, 32> Calls;
      for (Function &F : M) {
        if (F.hasFnAttribute(Attribute::OptimizeNone))
          continue;
        for (Instruction &I : instructions(F)) {
          auto *C = dyn_cast<CallInst>(&I);
          auto *Asm = C ? dyn_cast<InlineAsm>(C->getCalledOperand()) : nullptr;
          if (!Asm)
            continue;
          unsigned Arg = 0;
          for (const auto &Constraint : Asm->ParseConstraints()) {
            if (!Constraint.hasArg())
              continue;
            if (Arg >= C->arg_size())
              break;
            if (Constraint.Type == InlineAsm::isInput &&
                !Constraint.isIndirect && !Constraint.isMultipleAlternative &&
                Constraint.Codes.size() == 1 && Constraint.Codes[0] == "i") {
              SmallPtrSet<Value *, 32> Seen;
              collectFactories(C->getArgOperand(Arg), Seen, Calls);
            }
            ++Arg;
          }
        }
      }
      SmallPtrSet<Function *, 16> Modified;
      for (CallBase *C : Calls) {
        Function *Callee = C->getCalledFunction();
        SmallPtrSet<Function *, 8> Active;
        unsigned Budget = 256;
        if (!isa<CallInst>(C) || !Callee || C->isMustTailCall() ||
            C->hasFnAttr(Attribute::NoInline) ||
            C->hasFnAttr("linx-scalar-shareability") ||
            C->hasFnAttr("linx-tile-pe-domain") ||
            !isShapeFactory(*Callee, Active, Budget) ||
            !isInlineViable(*Callee).isSuccess())
          continue;
        // Body viability does not establish call-site legality. In particular,
        // a replaceable definition must remain callable at link time, and
        // caller/callee attributes must obey the regular inliner's rules.
        auto GetTLI = [&](Function &F) -> const TargetLibraryInfo & {
          return FAM.getResult<TargetLibraryAnalysis>(F);
        };
        auto Decision = getAttributeBasedInliningDecision(
            *C, Callee, FAM.getResult<TargetIRAnalysis>(*Callee), GetTLI);
        if (Decision && !Decision->isSuccess())
          continue;
        // At the entry boundary every sret write must target local storage.
        // The local buffer may be a non-sret argument of a factory with a
        // separate GM output. Do not pull that write into the caller's scope.
        bool LocalReturn = true;
        for (unsigned N = 0; N != C->arg_size(); ++N)
          if (Callee->hasParamAttribute(N, Attribute::StructRet) &&
              !isa<AllocaInst>(getUnderlyingObject(C->getArgOperand(N))))
            LocalReturn = false;
        if (!LocalReturn)
          continue;
        Function *Caller = C->getCaller();
        InlineFunctionInfo IFI;
        if (InlineFunction(*C, IFI).isSuccess())
          Modified.insert(Caller);
      }
      if (Modified.empty())
        break;
      Changed = true;
      for (Function *F : Modified) {
        FAM.invalidate(*F, PreservedAnalyses::none());
        // SROA only promotes local storage; no GM CSE/DSE is allowed here.
        auto PA = LinxV5CarrierPreparePass().run(*F, FAM);
        FAM.invalidate(*F, PA);
      }
    }
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};

// Tile handles are register values even when their C++ wrappers are passed by
// reference. Expose small helpers before assigning memory request scopes:
// scoping an unpromoted wrapper first makes its stack traffic irreversible.
// Inline only; do not run GM load/store optimizations at this point.
// The runtime intra ABI attaches the default scope to every call site. It is
// compatible with an unannotated native caller/callee; explicit function
// contracts and non-default call-site scopes remain boundaries.
struct LinxV5TileHelperPreparePass
    : PassInfoMixin<LinxV5TileHelperPreparePass> {
  static bool hasTileAsm(Function &F) {
    for (Instruction &I : instructions(F))
      if (auto *C = dyn_cast<CallInst>(&I))
        if (auto *Asm = dyn_cast<InlineAsm>(C->getCalledOperand()))
          for (const auto &Constraint : Asm->ParseConstraints())
            for (const auto &Code : Constraint.Codes)
              if (Code == "Sr" || Code == "Tr")
                return true;
    return false;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM) {
    auto &FAM =
        AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
    bool Changed = false;
    for (unsigned Round = 0; Round != 8; ++Round) {
      SmallVector<CallInst *, 32> Calls;
      for (Function &F : M) {
        if (F.hasFnAttribute(Attribute::OptimizeNone) ||
            F.hasFnAttribute("linx-scalar-shareability") ||
            F.hasFnAttribute("linx-tile-pe-domain"))
          continue;
        for (Instruction &I : instructions(F)) {
          auto *C = dyn_cast<CallInst>(&I);
          Function *Callee = C ? C->getCalledFunction() : nullptr;
          if (!Callee || Callee == &F || Callee->isDeclaration() ||
              Callee->isVarArg() || C->isMustTailCall() ||
              Callee->getInstructionCount() > 8192 ||
              F.getInstructionCount() + Callee->getInstructionCount() > 65536 ||
              C->hasFnAttr(Attribute::NoInline) ||
              Callee->hasFnAttribute(Attribute::NoInline) ||
              Callee->hasFnAttribute(Attribute::OptimizeNone) ||
              (C->hasFnAttr("linx-scalar-shareability") &&
               C->getFnAttr("linx-scalar-shareability").getValueAsString() !=
                   "intra") ||
              C->hasFnAttr("linx-tile-pe-domain") ||
              Callee->hasFnAttribute("linx-scalar-shareability") ||
              Callee->hasFnAttribute("linx-tile-pe-domain") ||
              !hasTileAsm(*Callee) || !isInlineViable(*Callee).isSuccess())
            continue;
          auto GetTLI = [&](Function &Fn) -> const TargetLibraryInfo & {
            return FAM.getResult<TargetLibraryAnalysis>(Fn);
          };
          auto Decision = getAttributeBasedInliningDecision(
              *C, Callee, FAM.getResult<TargetIRAnalysis>(*Callee), GetTLI);
          if (!Decision || Decision->isSuccess())
            Calls.push_back(C);
        }
      }
      SmallPtrSet<Function *, 16> Modified;
      for (CallInst *C : Calls) {
        Function *Caller = C->getCaller();
        if (Caller->getInstructionCount() +
                C->getCalledFunction()->getInstructionCount() >
            65536)
          continue;
        InlineFunctionInfo IFI;
        if (InlineFunction(*C, IFI).isSuccess())
          Modified.insert(Caller);
      }
      if (Modified.empty())
        break;
      Changed = true;
      for (Function *F : Modified) {
        FAM.invalidate(*F, PreservedAnalyses::none());
        auto PA = LinxV5CarrierPreparePass().run(*F, FAM);
        FAM.invalidate(*F, PA);
      }
    }
    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }
};
} // namespace

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
  if (F.getFnAttribute("__vec__").isValid() ||
      F.getFnAttribute("__mtc__").isValid()) {
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

void LinxV5TargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, ModulePassManager &MPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "linx-v5-pe-domain-prepare") {
          MPM.addPass(LinxV5PEDomainPreparePass());
          return true;
        }
        if (Name == "linx-v5-inline-asm-constant-prepare") {
          MPM.addPass(LinxV5InlineAsmConstantPreparePass());
          return true;
        }
        if (Name == "linx-v5-tile-helper-prepare") {
          MPM.addPass(LinxV5TileHelperPreparePass());
          return true;
        }
        if (Name == "linx-v5-pe-domain-specialize") {
          MPM.addPass(LinxV5PEDomainSpecializePass());
          return true;
        }
        if (Name == "linx-v5-pe-domain-unsupported") {
          MPM.addPass(LinxV5PEDomainUnsupportedPass());
          return true;
        }
        if (Name == "linx-v5-element-call-context") {
          MPM.addPass(LinxV5ElementCallContextPass());
          return true;
        }
        return false;
      });
  PB.registerPipelineParsingCallback(
      [this](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "linx-v5-private-object-promotion") {
          FPM.addPass(LinxV5PrivateObjectPromotionPass());
          return true;
        }
        if (Name == "linx-v5-scalar-shareability") {
          FPM.addPass(LinxV5ScalarShareabilityEarlyPass(this));
          return true;
        }
        if (Name == "linx-v5-element-region-prepare") {
          FPM.addPass(LinxV5ElementRegionPreparePass());
          return true;
        }
        if (Name == "linx-v5-element-predication") {
          FPM.addPass(LinxV5ElementPredicationPass());
          return true;
        }
        if (Name == "linx-v5-element-tile-legalize") {
          FPM.addPass(LinxV5ElementTileLegalizationPass());
          return true;
        }
        if (Name == "linx-v5-element-generic-prepare") {
          FPM.addPass(LinxV5GenericElementPreparePass());
          return true;
        }
        if (Name == "linx-v5-element-region") {
          FPM.addPass(LinxV5ElementRegionPass());
          return true;
        }
        if (Name == "linx-v5-element-region-promote") {
          FPM.addPass(LinxV5ElementRegionPromotePass());
          return true;
        }
        if (Name == "linx-v5-element-region-verify") {
          FPM.addPass(LinxV5ElementRegionVerifierPass());
          return true;
        }
        return false;
      });
  PB.registerPipelineStartEPCallback(
      [this](ModulePassManager &MPM, OptimizationLevel) {
        MPM.addPass(LinxV5PEDomainPreparePass());
        MPM.addPass(AlwaysInlinerPass());
        MPM.addPass(LinxV5InlineAsmConstantPreparePass());
        MPM.addPass(LinxV5TileHelperPreparePass());
        FunctionPassManager FPM;
        // Promote local induction/control and Tile carrier temporaries first.
        // This does not optimize externally visible GM accesses, but prevents
        // a marked loop's local control storage becoming a scoped GM request.
        FPM.addPass(SROAPass());
        // Protect surviving RowMajor and M-layout carrier accesses before
        // GVN/PRE can clone a load without its frontend type metadata. Local
        // carriers are promoted first; N8 transport waits for fixp inlining.
        if (!EnableGenericElement)
          FPM.addPass(LinxV5TileCarrierTransportPass(/*Early=*/true));
        FPM.addPass(LinxV5CarrierPreparePass());
        FPM.addPass(LinxV5PrivateObjectPromotionPass());
        FPM.addPass(LinxV5ScalarShareabilityEarlyPass(this));
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
      });
  PB.registerOptimizerLastEPCallback(
      [](ModulePassManager &MPM, OptimizationLevel) {
        FunctionPassManager FPM;
        if (EnableGenericElement) {
          MPM.addPass(LinxV5ElementCallContextPass());
          MPM.addPass(LinxV5LocalTileABIPromotionPass());
          MPM.addPass(LinxV5PEDomainSpecializePass());
          FPM.addPass(LinxV5GenericElementPreparePass());
          FPM.addPass(LinxV5ElementPredicationPass());
          FPM.addPass(LinxV5ElementTileLegalizationPass());
          FPM.addPass(LinxV5ElementRegionVerifierPass());
          MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
          return;
        }
        FPM.addPass(LinxV5ElementRegionPreparePass());
        FPM.addPass(LinxV5ElementRegionPromotePass());
        FPM.addPass(LinxV5ElementRegionPass());
        FPM.addPass(LinxV5ElementRegionVerifierPass());
        FPM.addPass(LinxV5PrivateObjectPromotionPass());
        FPM.addPass(LinxV5TileCarrierTransportPass());
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
      });
  // Issue #120: at -O0 the OptimizerLast hook never fires and nothing is
  // inlined, so rewrite the boundary carrier accesses early there. At -O1+
  // full RowMajor accesses are resolved just after initial inlining/SROA;
  // CUBE accesses and cross-call demotion retain the OptimizerLast hook.
  PB.registerPipelineEarlySimplificationEPCallback(
      [](ModulePassManager &MPM, OptimizationLevel Level) {
        if (Level.getSpeedupLevel() > 0 || Level.getSizeLevel() > 0)
          return;
        MPM.addPass(LinxV5PEDomainUnsupportedPass());
        FunctionPassManager FPM;
        FPM.addPass(LinxV5PrivateObjectPromotionPass());
        FPM.addPass(LinxV5TileCarrierTransportPass());
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
      });
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
  bool addPostFastRegAllocRewrite() override;
  void addFastRegAlloc() override;
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
  addPass(createLinxV5PEDomainRawVerifierPass());
  addPass(createLinxV5ElementRegionPreparePass());
  addPass(createLinxV5ElementRegionPass());
  addPass(createLinxV5ScalarShareabilityPass(&getLinxV5TargetMachine()));
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
  addPass(createLinxV5SharedReuseFinalizePass());
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
  addPass(createLinxV5SharedRegAllocPass());
  addPass(createGreedyRegisterAllocator());
  addPreRewrite();
  addPass(&VirtRegRewriterID);
  addPass(createLinxV5ClockhandsPostAllocPass());

  return true;
}

bool LinxV5PassConfig::addPreISel() {
  addPass(createLinxV5ScalarShareabilityPass(&getLinxV5TargetMachine()));
  addPass(createLinxV5ElementRegionVerifierPass());
  addPass(createLinxV5ElementwiseMaskPass());
  addPass(createStructurizeCFGPass(true)); // true -> SkipUniformRegions
  addPass(createLinxV5AnnotateControlFlowPass());
  return true;
}

void LinxV5PassConfig::addPostRewrite() {
  addPass(createLinxV5SharedCopyElimPass());
  addPass(createLinxV5SIMTSpillFixupPass());
  addPass(createLinxV5StackSizeFixupPass());
  addPass(createLinxV5TileFixupPass());
}

bool LinxV5PassConfig::addPostFastRegAllocRewrite() {
  addPass(createLinxV5SharedCopyElimPass());
  return true;
}

void LinxV5PassConfig::addPostRegAlloc() {}

void LinxV5PassConfig::addPreRegAlloc() {
  addPass(createLinxV5TileDefinednessPass());
  if (getOptLevel() != CodeGenOpt::None)
    addPass(createLinxV5SharedHandleLegalizePass());
  addPass(createLinxV5CanonicalizeBlockPass());
}

void LinxV5PassConfig::addFastRegAlloc() {
  addPass(&PHIEliminationID);
  addPass(&TwoAddressInstructionPassID);
  addPass(createLinxV5SharedCopyElimPass());
  addRegAssignAndRewriteFast();
}

void LinxV5PassConfig::addRightBeforeRegAlloc() {
  addPass(createLinxV5IsolateInlineASMBlockPass());
  addPass(createLinxV5RegisterCanonicalizationPass());
  addPass(createLinxV5ScrubRegsPass());
}
