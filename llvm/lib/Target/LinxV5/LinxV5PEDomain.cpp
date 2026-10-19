//===- LinxV5PEDomain.cpp - PTO function PE-domain ownership ------------===//

#include "LinxV5.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

using namespace llvm;

#define DEBUG_TYPE "linx-v5-pe-domain"

namespace {

constexpr StringLiteral PEDomainAttr = "linx-tile-pe-domain";
constexpr StringLiteral PEDomainLoweredAttr = "linx-tile-pe-domain-lowered";
constexpr StringLiteral PEDomainAnnotation = "pto.pe_domain";
constexpr StringLiteral PEDomainLoweredFlag = "linx.pe-domain.lowered";

[[noreturn]] static void peDomainError(Function *F, const Instruction *I,
                                       const Twine &Message) {
  if (F)
    F->getContext().diagnose(DiagnosticInfoUnsupported(
        *F, Twine("PTO PE domain: ") + Message,
        I ? I->getDebugLoc() : DebugLoc(), DS_Error));
  report_fatal_error("PTO PE-domain compilation failed",
                     /*gen_crash_diag=*/false);
}

static bool isNativePEDomain(unsigned Mask) {
  return Mask == 1 || Mask == 2 || Mask == 4 || Mask == 8 || Mask == 12 ||
         Mask == 14 || Mask == 15;
}

static Optional<unsigned> getPEDomain(const Function &F) {
  Attribute A = F.getFnAttribute(PEDomainAttr);
  if (!A.isValid())
    return None;
  unsigned Mask = 0;
  if (A.getValueAsString().getAsInteger(10, Mask) || !isNativePEDomain(Mask))
    peDomainError(const_cast<Function *>(&F), nullptr,
                  "malformed linx-tile-pe-domain function attribute");
  return Mask;
}

static void setPEDomain(Function &F, unsigned Mask) {
  if (!isNativePEDomain(Mask))
    peDomainError(&F, nullptr, "requires a native nonzero architectural mask");
  if (Optional<unsigned> Existing = getPEDomain(F))
    if (*Existing != Mask)
      peDomainError(&F, nullptr, "conflicting explicit function domains");
  F.addFnAttr(PEDomainAttr, utostr(Mask));
  F.addFnAttr(Attribute::NoInline);
}

static Optional<StringRef> getAnnotationName(Constant *Operand) {
  StringRef Text;
  if (!getConstantStringInfo(Operand->stripPointerCasts(), Text))
    return None;
  return Text;
}

static Optional<unsigned> getAnnotationMask(Constant *Operand) {
  auto *GV = dyn_cast<GlobalVariable>(Operand->stripPointerCasts());
  auto *CS = GV && GV->hasInitializer()
                 ? dyn_cast<ConstantStruct>(GV->getInitializer())
                 : nullptr;
  auto *CI = CS && CS->getNumOperands() == 1
                 ? dyn_cast<ConstantInt>(CS->getOperand(0))
                 : nullptr;
  if (!CI || CI->getValue().getActiveBits() > 32)
    return None;
  return unsigned(CI->getZExtValue());
}

static bool safeBoundaryType(Type *Ty, bool Return) {
  return (Return && Ty->isVoidTy()) || Ty->isPointerTy() || Ty->isIntegerTy() ||
         Ty->isFloatingPointTy();
}

static bool boundaryArgumentReachesTileCarrier(Argument &Root) {
  SmallPtrSet<Value *, 32> Seen;
  std::function<bool(Value *)> Trace = [&](Value *V) {
    if (!Seen.insert(V).second)
      return false;
    for (User *U : V->users()) {
      if (auto *Load = dyn_cast<LoadInst>(U))
        if (Load->getPointerOperand() == V &&
            Load->getMetadata("linx.tile.transport"))
          return true;
      if (auto *Store = dyn_cast<StoreInst>(U))
        if (Store->getPointerOperand() == V &&
            Store->getMetadata("linx.tile.transport"))
          return true;
      if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
        if (Trace(GEP))
          return true;
        continue;
      }
      if (auto *Cast = dyn_cast<CastInst>(U)) {
        if (Cast->getType()->isPointerTy() && Trace(Cast))
          return true;
        continue;
      }
      if (auto *Phi = dyn_cast<PHINode>(U)) {
        if (Phi->getType()->isPointerTy() && Trace(Phi))
          return true;
        continue;
      }
      if (auto *Select = dyn_cast<SelectInst>(U)) {
        if (Select->getType()->isPointerTy() && Trace(Select))
          return true;
        continue;
      }
      auto *Call = dyn_cast<CallBase>(U);
      if (!Call)
        continue;
      Function *Callee = Call->getCalledFunction();
      if (Callee && Callee->getIntrinsicID() == Intrinsic::ptr_annotation &&
          Call->arg_size() >= 2 && Call->getArgOperand(0) == V) {
        auto *Annotation = dyn_cast<Constant>(Call->getArgOperand(1));
        Optional<StringRef> Name =
            Annotation ? getAnnotationName(Annotation) : None;
        if (Name && (Name->startswith("linx.tile.carrier:") ||
                     Name->startswith("pto.element.view:")))
          return true;
      }
      if (!Callee || Callee->isDeclaration() || Callee->isVarArg() ||
          Call->isMustTailCall() || Call->hasOperandBundles())
        continue;
      for (unsigned I = 0; I < Call->arg_size() && I < Callee->arg_size(); ++I)
        if (Call->getArgOperand(I) == V &&
            Callee->getArg(I)->getType()->isPointerTy() &&
            Trace(Callee->getArg(I)))
          return true;
    }
    return false;
  };
  return Trace(&Root);
}

static void validateBoundarySignature(Function &F, bool AllowLocalTile) {
  if (AllowLocalTile && F.hasFnAttribute("linx-local-tile-abi"))
    return;
  if (!AllowLocalTile)
    for (Instruction &I : instructions(F)) {
      auto *Call = dyn_cast<CallBase>(&I);
      Function *Callee = Call ? Call->getCalledFunction() : nullptr;
      if (!Callee || Callee->getIntrinsicID() != Intrinsic::ptr_annotation ||
          Call->arg_size() < 2)
        continue;
      auto *Annotation = dyn_cast<Constant>(Call->getArgOperand(1));
      Optional<StringRef> Name =
          Annotation ? getAnnotationName(Annotation) : None;
      auto *BoundaryArgument =
          dyn_cast<Argument>(getUnderlyingObject(Call->getArgOperand(0)));
      if (BoundaryArgument && BoundaryArgument->getParent() == &F && Name &&
          (Name->startswith("linx.tile.carrier:") ||
           Name->startswith("pto.element.view:")))
        peDomainError(&F, &I,
                      "Tile-reference arguments may not cross a function PE "
                      "domain boundary");
    }
  if (!safeBoundaryType(F.getReturnType(), true) || F.isVarArg())
    peDomainError(&F, nullptr,
                  "cross-domain boundary requires a scalar/GM signature");
  if (!AllowLocalTile && F.hasFnAttribute("linx-local-tile-abi"))
    peDomainError(&F, nullptr,
                  "Local Tile ABI may not escape across PE domains");
  for (Argument &Arg : F.args()) {
    unsigned I = Arg.getArgNo();
    AttributeList AL = F.getAttributes();
    if (!safeBoundaryType(Arg.getType(), false) ||
        AL.hasParamAttr(I, Attribute::ByVal) ||
        AL.hasParamAttr(I, Attribute::StructRet) ||
        AL.hasParamAttr(I, Attribute::InAlloca) ||
        AL.hasParamAttr(I, Attribute::Preallocated))
      peDomainError(&F, nullptr,
                    "cross-domain boundary rejects vector, aggregate, byval "
                    "and sret arguments");
    if (!AllowLocalTile && Arg.getType()->isPointerTy() &&
        boundaryArgumentReachesTileCarrier(Arg))
      peDomainError(&F, nullptr,
                    "Tile-reference arguments may not cross a function PE "
                    "domain boundary");
  }
}

static bool hasDomainAnnotation(const Module &M) {
  const GlobalVariable *GA = M.getGlobalVariable("llvm.global.annotations");
  const auto *Init = GA && GA->hasInitializer()
                         ? dyn_cast<ConstantArray>(GA->getInitializer())
                         : nullptr;
  if (!Init)
    return false;
  for (const Use &U : Init->operands()) {
    const auto *Record = dyn_cast<ConstantStruct>(U.get());
    if (!Record || Record->getNumOperands() < 2)
      continue;
    Optional<StringRef> Name =
        getAnnotationName(cast<Constant>(Record->getOperand(1)));
    if (Name && *Name == PEDomainAnnotation)
      return true;
  }
  return false;
}

static void rejectUnsupportedModule(Module &M, StringRef Surface) {
  if (M.getModuleFlag(PEDomainLoweredFlag)) {
    for (Function &F : M)
      if (F.hasFnAttribute(PEDomainAttr) &&
          !F.hasFnAttribute(PEDomainLoweredAttr))
        peDomainError(&F, nullptr,
                      "function domain was prepared but not specialized");
    GlobalVariable *GA = M.getGlobalVariable("llvm.global.annotations");
    auto *Init = GA && GA->hasInitializer()
                     ? dyn_cast<ConstantArray>(GA->getInitializer())
                     : nullptr;
    if (Init)
      for (Use &U : Init->operands()) {
        auto *Record = dyn_cast<ConstantStruct>(U.get());
        if (!Record || Record->getNumOperands() < 5)
          continue;
        Optional<StringRef> Name =
            getAnnotationName(cast<Constant>(Record->getOperand(1)));
        if (!Name || *Name != PEDomainAnnotation)
          continue;
        auto *F = dyn_cast<Function>(
            cast<Constant>(Record->getOperand(0))->stripPointerCasts());
        Optional<unsigned> Mask =
            getAnnotationMask(cast<Constant>(Record->getOperand(4)));
        Optional<unsigned> Domain = F ? getPEDomain(*F) : None;
        if (!F || !Mask || !Domain || *Mask != *Domain ||
            !F->hasFnAttribute(PEDomainLoweredAttr))
          peDomainError(F, nullptr,
                        "linked annotation was not consumed by PE-domain "
                        "preparation");
      }
    return;
  }
  for (Function &F : M)
    if (F.hasFnAttribute(PEDomainAttr))
      peDomainError(&F, nullptr, Twine(Surface) +
                                     " cannot consume a PTO PE domain");
  if (hasDomainAnnotation(M))
    report_fatal_error(Twine("PTO PE domain: ") + Surface +
                           " cannot consume pto.pe_domain annotation",
                       /*gen_crash_diag=*/false);
}

class LinxV5PEDomainRawVerifierLegacyPass : public ModulePass {
public:
  static char ID;
  LinxV5PEDomainRawVerifierLegacyPass() : ModulePass(ID) {
    initializeLinxV5PEDomainRawVerifierLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override {
    rejectUnsupportedModule(M, "raw llc input");
    return false;
  }
};

} // namespace

char LinxV5PEDomainRawVerifierLegacyPass::ID = 0;

INITIALIZE_PASS(LinxV5PEDomainRawVerifierLegacyPass,
                "linx-v5-pe-domain-raw-verify",
                "LinxV5 PTO PE-domain raw input verifier", false, false)

ModulePass *llvm::createLinxV5PEDomainRawVerifierPass() {
  return new LinxV5PEDomainRawVerifierLegacyPass();
}

PreservedAnalyses
LinxV5PEDomainPreparePass::run(Module &M, ModuleAnalysisManager &) {
  GlobalVariable *GA = M.getGlobalVariable("llvm.global.annotations");
  auto *Init = GA && GA->hasInitializer()
                   ? dyn_cast<ConstantArray>(GA->getInitializer())
                   : nullptr;
  if (!Init)
    return PreservedAnalyses::all();

  bool Changed = false;
  for (Use &U : Init->operands()) {
    auto *Record = dyn_cast<ConstantStruct>(U.get());
    if (!Record || Record->getNumOperands() < 5)
      continue;
    Optional<StringRef> Name =
        getAnnotationName(cast<Constant>(Record->getOperand(1)));
    if (!Name || *Name != PEDomainAnnotation)
      continue;
    auto *F = dyn_cast<Function>(
        cast<Constant>(Record->getOperand(0))->stripPointerCasts());
    Optional<unsigned> Mask =
        getAnnotationMask(cast<Constant>(Record->getOperand(4)));
    if (!F || F->isDeclaration() || !Mask || !isNativePEDomain(*Mask))
      peDomainError(F, nullptr,
                    "annotation requires one defined function and a native "
                    "nonzero architectural mask");
    if (F->hasFnAttribute(Attribute::AlwaysInline))
      peDomainError(F, nullptr,
                    "explicit PE-domain boundaries may not be force-inlined");
    validateBoundarySignature(*F, /*AllowLocalTile=*/false);
    setPEDomain(*F, *Mask);
    Changed = true;
  }
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

PreservedAnalyses
LinxV5PEDomainSpecializePass::run(Module &M, ModuleAnalysisManager &) {
  using Context = std::pair<Function *, unsigned>;
  DenseSet<Context> Seen;
  DenseSet<Context> Active;
  MapVector<Function *, SmallSet<unsigned, 4>> Domains;
  SmallVector<Context, 16> Roots;
  bool HasExplicitDomain = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    if (Optional<unsigned> D = getPEDomain(F)) {
      HasExplicitDomain = true;
      validateBoundarySignature(F, /*AllowLocalTile=*/false);
      Domains[&F].insert(*D);
      Roots.emplace_back(&F, *D);
    } else if (!F.hasLocalLinkage()) {
      Domains[&F].insert(15);
      Roots.emplace_back(&F, 15);
    }
  }
  if (!HasExplicitDomain)
    return PreservedAnalyses::all();

  std::function<void(Function *, unsigned)> Walk =
      [&](Function *F, unsigned Domain) {
        Context Key{F, Domain};
        if (Active.contains(Key)) {
          if (Domain == 15)
            return;
          peDomainError(F, nullptr,
                        "recursive helper graph has no proved domain closure");
        }
        if (!Seen.insert(Key).second)
          return;
        Active.insert(Key);
        for (Instruction &I : instructions(F)) {
          auto *Call = dyn_cast<CallBase>(&I);
          if (!Call)
            continue;
          if (Call->isInlineAsm()) {
            if (Domain != 15) {
              StringRef Asm = cast<InlineAsm>(Call->getCalledOperand())
                                  ->getAsmString();
              std::string Lower = Asm.lower();
              if (StringRef(Lower).contains(';'))
                peDomainError(F, &I,
                              "inline assembly statement separators are not "
                              "allowed in a nondefault PE domain");
              SmallVector<StringRef, 16> Statements;
              StringRef(Lower).split(Statements, '\n');
              for (StringRef Statement : Statements) {
                Statement = Statement.trim();
                if (Statement.empty())
                  continue;
                const bool Conditional = Statement.startswith(".if ") ||
                                         Statement.startswith(".elseif ") ||
                                         Statement == ".else" ||
                                         Statement == ".endif";
                const bool TileStatement =
                    Statement.startswith("bstart.t") ||
                    Statement.startswith("bstart.cube") ||
                    Statement.startswith("b.datr") ||
                    Statement.startswith("b.fpatr") ||
                    Statement.startswith("b.dim") ||
                    Statement.startswith("c.b.dim") ||
                    Statement.startswith("b.iot") ||
                    Statement.startswith("b.ior") ||
                    Statement.startswith("b.assemble") ||
                    Statement.startswith("b.subview");
                const bool Hint = Statement == "b.hint trace.begin" ||
                                  Statement == "b.hint trace.end";
                const bool Stop = Statement == "bstop" ||
                                  Statement == "c.bstop";
                if (Statement.startswith("b.ios") ||
                    Statement.startswith("bstart.gmov") ||
                    Statement.startswith("bstart.timg2col"))
                  peDomainError(F, &I,
                                "nondefault domains support proved Local "
                                "Tile operations only");
                if (!Conditional && !TileStatement && !Hint && !Stop)
                  peDomainError(
                      F, &I,
                      Twine("inline assembly has no proved local PE-domain "
                            "transfer contract: `") +
                          Statement + "`");
              }
            }
            continue;
          }
          Function *Callee = Call->getCalledFunction();
          if (!Callee) {
            if (Domain == 15)
              continue;
            peDomainError(F, &I,
                          "indirect calls have no proved PE-domain ownership");
          }
          if (Callee->isIntrinsic()) {
            if (Domain != 15 && isa<MemIntrinsic>(Call) &&
                !isa<MemSetInst>(Call))
              peDomainError(F, &I,
                            "nondefault domains support only locally "
                            "expandable memset memory intrinsics");
            continue;
          }
          if (Optional<unsigned> Explicit = getPEDomain(*Callee)) {
            if (Domain != 15 && *Explicit != Domain)
              peDomainError(F, &I, "explicit nested domain conflicts with caller");
            continue;
          }
          if (Callee->isDeclaration()) {
            if (Domain != 15)
              peDomainError(F, &I,
                            "external calls have no proved PE-domain ownership");
            continue;
          }
          if (Domain != 15 &&
              ((!Callee->hasLocalLinkage() &&
                !Callee->hasLinkOnceODRLinkage()) ||
               Callee->hasAddressTaken()))
            peDomainError(F, &I,
                          "helper graph must be closed, local and direct");
          Domains[Callee].insert(Domain);
          Walk(Callee, Domain);
        }
        Active.erase(Key);
      };
  for (Context Root : Roots)
    Walk(Root.first, Root.second);

  MapVector<Context, Function *> Variants;
  DenseMap<Function *, Function *> Origins;
  unsigned CloneID = 0;
  SmallVector<Function *, 8> MaybeDead;
  for (auto &Entry : Domains) {
    Function *F = Entry.first;
    SmallSet<unsigned, 4> &Set = Entry.second;
    if (Optional<unsigned> Explicit = getPEDomain(*F)) {
      Variants[{F, *Explicit}] = F;
      continue;
    }
    // A linkonce_odr definition can acquire callers from another translation
    // unit after linking.  Preserve its default-domain body even when this
    // module currently observes only a nondefault call context.
    bool Has15 = Set.contains(15) || F->hasLinkOnceODRLinkage();
    SmallVector<unsigned, 4> Nondefault;
    for (unsigned D : Set)
      if (D != 15)
        Nondefault.push_back(D);
    if (Has15)
      Variants[{F, 15}] = F;
    if (Nondefault.empty())
      continue;

    for (User *U : F->users()) {
      auto *Call = dyn_cast<CallInst>(U);
      if (!Call || Call->getCalledFunction() != F || Call->isMustTailCall() ||
          Call->hasOperandBundles())
        peDomainError(F, nullptr,
                      "helper has indirect, bundled or non-call uses");
    }
    if (!Has15 && Nondefault.size() == 1) {
      setPEDomain(*F, Nondefault.front());
      Variants[{F, Nondefault.front()}] = F;
      continue;
    }
    validateBoundarySignature(*F, /*AllowLocalTile=*/true);
    for (unsigned D : Nondefault) {
      ValueToValueMapTy Map;
      Function *Clone = CloneFunction(F, Map);
      Clone->setName(F->getName() + ".pto.pe" + Twine(D) + "." +
                     Twine(CloneID++));
      Clone->setLinkage(GlobalValue::InternalLinkage);
      Clone->setComdat(nullptr);
      setPEDomain(*Clone, D);
      Variants[{F, D}] = Clone;
      Origins[Clone] = F;
    }
    if (!Has15)
      MaybeDead.push_back(F);
  }

  for (auto &Entry : Variants) {
    Function *Original = Entry.first.first;
    unsigned Domain = Entry.first.second;
    Function *Variant = Entry.second;
    Variant->addFnAttr(PEDomainLoweredAttr);
    (void)Original;
    for (Instruction &I : instructions(Variant)) {
      auto *Call = dyn_cast<CallInst>(&I);
      if (!Call || Call->isInlineAsm())
        continue;
      Function *Callee = Call->getCalledFunction();
      if (!Callee || Callee->isIntrinsic() || getPEDomain(*Callee))
        continue;
      Function *CalleeOrigin = Origins.lookup(Callee);
      if (!CalleeOrigin)
        CalleeOrigin = Callee;
      if (Function *Target = Variants.lookup({CalleeOrigin, Domain}))
        Call->setCalledFunction(Target);
    }
    if (Domain != 15) {
      SmallVector<MemSetInst *, 4> Memsets;
      for (Instruction &I : instructions(Variant))
        if (auto *Memset = dyn_cast<MemSetInst>(&I))
          Memsets.push_back(Memset);
      for (MemSetInst *Memset : Memsets)
        expandMemSetAsLoop(Memset);
      for (MemSetInst *Memset : Memsets)
        Memset->eraseFromParent();
    }
  }

  for (Function *F : MaybeDead)
    if (F->use_empty() && F->isDiscardableIfUnused())
      F->eraseFromParent();

  if (!Domains.empty())
    M.addModuleFlag(Module::Error, PEDomainLoweredFlag, 1);
  return Domains.empty() ? PreservedAnalyses::all()
                         : PreservedAnalyses::none();
}

PreservedAnalyses
LinxV5PEDomainUnsupportedPass::run(Module &M, ModuleAnalysisManager &) {
  rejectUnsupportedModule(M, "-O0");
  return PreservedAnalyses::all();
}
