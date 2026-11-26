//===-- LinxV5ScalarShareability.cpp - scoped Scalar GM lowering
//-----------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5TargetMachine.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/AtomicExpandUtils.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"

using namespace llvm;

namespace {
// Run both before generic optimization and immediately before selection.
// Early conversion prevents DSE, vectorization and libc formation from losing
// request scope; the late sweep covers accesses synthesized by target passes.

class LinxV5ScalarShareability : public FunctionPass {
public:
  static char ID;
  const LinxV5TargetMachine *TM;
  LinxV5ScalarShareability(const LinxV5TargetMachine *TM = nullptr)
      : FunctionPass(ID), TM(TM) {
    initializeLinxV5ScalarShareabilityPass(*PassRegistry::getPassRegistry());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }
  StringRef getPassName() const override {
    return "LinxV5 Scalar shareability";
  }
  bool runOnFunction(Function &F) override;
};
} // namespace

char LinxV5ScalarShareability::ID = 0;
INITIALIZE_PASS_BEGIN(LinxV5ScalarShareability, "linxv5-scalar-shareability",
                      "LinxV5 Scalar shareability", false, false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_END(LinxV5ScalarShareability, "linxv5-scalar-shareability",
                    "LinxV5 Scalar shareability", false, false)

// A private object must be a complete, nonescaping allocation. noalias on a
// parameter only separates parameters; it says nothing about the same buffer
// being passed to Tile work in another function. ptrtoint, stores of pointers,
// calls (including nocapture Tile intrinsics), returns and inline asm all make
// an alloca potentially shared. This deliberately uses no byte-range AA: one
// object can be reached through GEP/PHI/select at several Tile request ranges.
static bool
isPrivateObject(Value *P, SmallDenseMap<const Value *, bool, 8> &Cache,
                const SmallPtrSetImpl<const BasicBlock *> &TileBlocks) {
  const Value *Object = getUnderlyingObject(P);
  auto *Allocation = dyn_cast<AllocaInst>(Object);
  if (!Allocation)
    return false;
  auto Found = Cache.find(Object);
  if (Found != Cache.end())
    return Found->second;
  bool Private = true;
  {
    SmallVector<const Value *, 16> Work{Object};
    SmallPtrSet<const Value *, 32> Seen;
    while (!Work.empty() && Private) {
      const Value *V = Work.pop_back_val();
      if (!Seen.insert(V).second)
        continue;
      for (const User *U : V->users()) {
        if (auto *I = dyn_cast<Instruction>(U))
          if (TileBlocks.contains(I->getParent())) {
            Private = false;
            break;
          }
        if (auto *C = dyn_cast<CallBase>(U)) {
          if (isa<MemIntrinsic>(C) ||
              (C->getCalledFunction() &&
               (C->getCalledFunction()->getIntrinsicID() ==
                    Intrinsic::lifetime_start ||
                C->getCalledFunction()->getIntrinsicID() == Intrinsic::vastart ||
                C->getCalledFunction()->getIntrinsicID() == Intrinsic::vacopy ||
                C->getCalledFunction()->getIntrinsicID() == Intrinsic::vaend ||
                C->getCalledFunction()->getIntrinsicID() ==
                    Intrinsic::lifetime_end)))
            continue;
          Private = false;
          break;
        }
        if (isa<PHINode>(U) || isa<SelectInst>(U)) {
          // A merge with a shared object requires IntraCore, including
          // initialization through the alloca before the merge is optimized.
          for (const Use &Input : U->operands())
            if (Input->getType()->isPointerTy() &&
                getUnderlyingObject(Input.get()) != Object) {
              Private = false;
              break;
            }
          if (!Private)
            break;
          Work.push_back(cast<Value>(U));
          continue;
        }
        if (isa<GetElementPtrInst>(U) || isa<BitCastInst>(U) ||
            isa<AddrSpaceCastInst>(U)) {
          Work.push_back(cast<Value>(U));
          continue;
        }
        if (auto *L = dyn_cast<LoadInst>(U)) {
          // Native Tile carriers access this object through IntraCore even
          // when its address never escapes. Scalar initialization must match.
          if (isa<FixedVectorType>(L->getType()) &&
              L->getModule()->getDataLayout().getTypeStoreSize(L->getType()) >=
                  TypeSize::Fixed(128)) {
            Private = false;
            break;
          }
          continue;
        }
        if (isa<ICmpInst>(U))
          continue;
        if (auto *S = dyn_cast<StoreInst>(U)) {
          Type *T = S->getValueOperand()->getType();
          if (isa<FixedVectorType>(T) &&
              S->getModule()->getDataLayout().getTypeStoreSize(T) >=
                  TypeSize::Fixed(128)) {
            Private = false;
            break;
          }
          if (S->getValueOperand() != V)
            continue;
        }
        if (auto *R = dyn_cast<AtomicRMWInst>(U)) {
          if (R->getValOperand() != V)
            continue;
        }
        if (auto *C = dyn_cast<AtomicCmpXchgInst>(U)) {
          if (C->getNewValOperand() != V && C->getCompareOperand() != V)
            continue;
        }
        Private = false;
        break;
      }
    }
  }
  Cache[Object] = Private;
  return Private;
}

static bool lowerScalarShareability(Function &F, const TargetTransformInfo &TTI,
                                    const TargetLibraryInfo &TLI) {
  if (F.isDeclaration() || F.hasFnAttribute("__vec__") ||
      F.hasFnAttribute("__mtc__") ||
      (F.hasFnAttribute("target-cpu") &&
       F.getFnAttribute("target-cpu").getValueAsString() == "simt"))
    return false;
  auto Attr = F.getFnAttribute("linx-scalar-shareability");
  bool Explicit = Attr.isStringAttribute();
  StringRef Name = Explicit ? Attr.getValueAsString() : "intra";
  unsigned Scope = Name == "private" ? 0
                   : Name == "intra" ? 1
                   : Name == "inter" ? 2
                                     : 3;
  if (Scope == 3)
    report_fatal_error("invalid linx-scalar-shareability attribute");
  const DataLayout &DL = F.getParent()->getDataLayout();
  auto Fail = [&](const Twine &Why) {
    F.getContext().diagnose(DiagnosticInfoUnsupported(
        F, Twine("Scalar shareability: ") + Why, DebugLoc(), DS_Error));
    report_fatal_error("Scalar shareability lowering failed", false);
  };
  // A marked element loop is an IndexedGenerated program. Its memory
  // operations must remain recognizable by element predication/legalization;
  // that pipeline verifies complete conversion to fixed-IntraCore requests.
  // Scalar accesses outside these loops still require ordinary alias handling.
  SmallPtrSet<const BasicBlock *, 32> TileBlocks;
  if (!Explicit) {
    DominatorTree DT(F);
    LoopInfo LI(DT);
    for (Loop *L : LI.getLoopsInPreorder()) {
      MDNode *ID = L->getLoopID();
      if (!ID)
        continue;
      for (const MDOperand &Operand : drop_begin(ID->operands())) {
        auto *Property = dyn_cast_or_null<MDNode>(Operand.get());
        if (!Property || Property->getNumOperands() == 0)
          continue;
        auto *Key = dyn_cast_or_null<MDString>(Property->getOperand(0));
        if (Key && Key->getString() == "llvm.loop.linx.pto.element.region")
          TileBlocks.insert(L->block_begin(), L->block_end());
      }
    }
  }
  SmallDenseMap<const Value *, bool, 8> PrivateCache;
  auto NeedsScope = [&](Value *P) {
    return Explicit || !isPrivateObject(P, PrivateCache, TileBlocks);
  };
  SmallVector<MemIntrinsic *, 8> Mem;
  SmallVector<CallBase *, 8> Library;
  SmallVector<CallBase *, 4> VaCopies;
  SmallVector<Instruction *, 4> PrefetchHints;
  for (Instruction &I : instructions(F)) {
    if (TileBlocks.contains(I.getParent()))
      continue;
    if (auto *M = dyn_cast<MemIntrinsic>(&I)) {
      bool Shared = NeedsScope(M->getRawDest());
      if (auto *T = dyn_cast<MemTransferInst>(M))
        Shared |= NeedsScope(T->getRawSource());
      if (Shared)
        Mem.push_back(M);
    } else if (auto *C = dyn_cast<CallBase>(&I)) {
      Function *Callee = C->getCalledFunction();
      if (C->isInlineAsm()) {
        if (Explicit)
          Fail("inline assembly needs explicit scoped builtins");
        // Assembly owns its encoded requests. Its pointer operands still
        // prevent an underlying alloca from being classified Private.
        continue;
      }
      if (Callee && Callee->isIntrinsic()) {
        // Target requests own their architectural scope.
        if (!Explicit && Callee->getName().startswith("llvm.linx."))
          continue;
        switch (Callee->getIntrinsicID()) {
        case Intrinsic::linx_scalar_load:
        case Intrinsic::linx_scalar_store:
        case Intrinsic::linx_scalar_atomic:
        case Intrinsic::lifetime_start:
        case Intrinsic::lifetime_end:
        case Intrinsic::dbg_declare:
        case Intrinsic::dbg_value:
        case Intrinsic::dbg_label:
        case Intrinsic::assume:
        case Intrinsic::stacksave:
        case Intrinsic::stackrestore:
        case Intrinsic::vaend:
        case Intrinsic::trap:
        case Intrinsic::debugtrap:
        case Intrinsic::invariant_start:
        case Intrinsic::invariant_end:
        case Intrinsic::launder_invariant_group:
        case Intrinsic::strip_invariant_group:
        case Intrinsic::ptr_annotation:
        case Intrinsic::var_annotation:
        case Intrinsic::experimental_noalias_scope_decl:
          continue;
        case Intrinsic::vastart:
          // The varargs ABI lowers these to pointer stores/copies. Keep the
          // ordinary private lowering only for nonescaping stack va_lists;
          // shared va_list objects still need explicit scoped operations.
          if (NeedsScope(C->getArgOperand(0)))
            Fail("shared va_list needs an explicit scoped lowering");
          continue;
        case Intrinsic::vacopy:
          VaCopies.push_back(C);
          continue;
        case Intrinsic::prefetch:
          if (!Explicit && F.hasFnAttribute("linx-runtime-intra-abi")) {
            // Prefetch is optional; omit the hint until a scoped form exists.
            PrefetchHints.push_back(C);
            continue;
          }
          Fail("prefetch needs an explicit scoped lowering");
          continue;
        default:
          if (!C->doesNotAccessMemory())
            Fail(Twine("memory intrinsic '") + Callee->getName() +
                 "' needs an explicit scoped lowering");
          continue;
        }
      }
      if (C->doesNotAccessMemory())
        continue;
      LibFunc LF;
      if (TLI.getLibFunc(*C, LF) && TLI.has(LF) &&
          (LF == LibFunc_memset || LF == LibFunc_memcpy ||
           LF == LibFunc_memmove)) {
        Library.push_back(C);
        continue;
      }
      // Runtime libraries may explicitly assert a uniform call ABI at each
      // call site (including constructor/function-pointer calls). Ordinary
      // code still requires a checked direct callee contract. Do not override
      // a visible incompatible callee with the call-site assertion.
      auto CallContract = C->getFnAttr("linx-scalar-shareability");
      auto CalleeContract = Callee ? Callee->getFnAttribute(
                                        "linx-scalar-shareability")
                                  : Attribute();
      // EH preparation and optimization can synthesize calls after Clang
      // attaches call-site contracts. Only runtime builds assert this default;
      // explicit function scopes and incompatible callee scopes stay checked.
      if (((CallContract.isStringAttribute() &&
            CallContract.getValueAsString() == Name) ||
           (!Explicit && F.hasFnAttribute("linx-runtime-intra-abi") &&
            Name == "intra")) &&
          (!CalleeContract.isStringAttribute() ||
           CalleeContract.getValueAsString() == Name))
        continue;
      if (!Callee)
        Fail("indirect memory call needs a scoped callee contract");
      auto Contract = Callee->getFnAttribute("linx-scalar-shareability");
      if (Contract.isStringAttribute() && Contract.getValueAsString() == Name)
        continue;
      // Every definition in this native compilation receives the same early
      // and late sweep. Module boundaries require an explicit ABI contract.
      if (!Explicit && !Contract.isStringAttribute() &&
          !Callee->isDeclaration() && !Callee->hasFnAttribute("__vec__") &&
          !Callee->hasFnAttribute("__mtc__") &&
          (!Callee->hasFnAttribute("target-cpu") ||
           Callee->getFnAttribute("target-cpu").getValueAsString() != "simt"))
        continue;
      if (Explicit)
        Fail("memory-accessing callee needs a matching shareability contract");
      Fail(Twine("external memory call '") + Callee->getName() +
           "' needs linx_scalar_shareability(\"intra\") or a visible "
           "definition");
    }
  }
  for (Instruction *Hint : PrefetchHints)
    Hint->eraseFromParent();
  // Linx va_list is a single pointer. Materialize its copy before the scoped
  // memory sweep, so even escaped/shared source and destination lists retain
  // the appropriate request scope (vfprintf uses this across calls).
  for (CallBase *C : VaCopies) {
    IRBuilder<> B(C);
    Type *PtrTy = B.getPtrTy();
    Value *V = B.CreateAlignedLoad(PtrTy, C->getArgOperand(1), Align(1));
    B.CreateAlignedStore(V, C->getArgOperand(0), Align(1));
    C->eraseFromParent();
  }
  for (CallBase *C : Library) {
    if (!isa<CallInst>(C))
      Fail("memory library invoke cannot be expanded");
    IRBuilder<> B(C);
    LibFunc LF;
    TLI.getLibFunc(*C, LF);
    CallInst *M;
    if (LF == LibFunc_memset)
      M = B.CreateMemSet(C->getArgOperand(0),
                         B.CreateTrunc(C->getArgOperand(1), B.getInt8Ty()),
                         C->getArgOperand(2), MaybeAlign(1));
    else if (LF == LibFunc_memcpy)
      M = B.CreateMemCpy(C->getArgOperand(0), MaybeAlign(1),
                         C->getArgOperand(1), MaybeAlign(1),
                         C->getArgOperand(2));
    else
      M = B.CreateMemMove(C->getArgOperand(0), MaybeAlign(1),
                          C->getArgOperand(1), MaybeAlign(1),
                          C->getArgOperand(2));
    C->replaceAllUsesWith(C->getArgOperand(0));
    C->eraseFromParent();
    Mem.push_back(cast<MemIntrinsic>(M));
  }
  for (MemIntrinsic *M : Mem) {
    if (auto *C = dyn_cast<MemCpyInst>(M))
      expandMemCpyAsLoop(C, TTI);
    else if (auto *S = dyn_cast<MemSetInst>(M))
      expandMemSetAsLoop(S);
    else if (auto *V = dyn_cast<MemMoveInst>(M))
      expandMemMoveAsLoop(V);
    else
      Fail("unsupported memory intrinsic");
    M->eraseFromParent();
  }
  SmallVector<AtomicRMWInst *, 8> Expand;
  for (Instruction &I : instructions(F))
    if (!TileBlocks.contains(I.getParent()))
      if (auto *R = dyn_cast<AtomicRMWInst>(&I))
        if (NeedsScope(R->getPointerOperand()) &&
            (R->getOperation() == AtomicRMWInst::Nand ||
             R->isFloatingPointOperation() ||
             (DL.getTypeStoreSize(R->getType()) < 4 &&
              R->getOperation() != AtomicRMWInst::Xchg)))
          Expand.push_back(R);
  for (AtomicRMWInst *R : Expand)
    expandAtomicRMWToCmpXchg(R, [](IRBuilder<> &B, Value *P, Value *Old,
                                   Value *New, Align A, AtomicOrdering O,
                                   SyncScope::ID S, Value *&Success,
                                   Value *&Loaded) {
      Type *ValueTy = Old->getType();
      if (ValueTy->isFloatingPointTy()) {
        Type *BitsTy = B.getIntNTy(ValueTy->getPrimitiveSizeInBits());
        Old = B.CreateBitCast(Old, BitsTy);
        New = B.CreateBitCast(New, BitsTy);
      }
      auto *C = B.CreateAtomicCmpXchg(
          P, Old, New, A, O, AtomicCmpXchgInst::getStrongestFailureOrdering(O),
          S);
      Success = B.CreateExtractValue(C, 1);
      Loaded = B.CreateExtractValue(C, 0);
      if (ValueTy->isFloatingPointTy())
        Loaded = B.CreateBitCast(Loaded, ValueTy);
    });
  // Split aggregate accesses into their typed fields before applying scope.
  // Struct padding is not a value and must not be turned into scalar data.
  SmallVector<Instruction *, 8> Aggregates;
  for (Instruction &I : instructions(F)) {
    if (TileBlocks.contains(I.getParent()))
      continue;
    if (auto *L = dyn_cast<LoadInst>(&I)) {
      if (L->getType()->isAggregateType() && NeedsScope(L->getPointerOperand()))
        Aggregates.push_back(L);
    } else if (auto *S = dyn_cast<StoreInst>(&I)) {
      if (S->getValueOperand()->getType()->isAggregateType() &&
          NeedsScope(S->getPointerOperand()))
        Aggregates.push_back(S);
    }
  }
  for (Instruction *I : Aggregates) {
    IRBuilder<> B(I);
    B.SetCurrentDebugLocation(I->getDebugLoc());
    auto *L = dyn_cast<LoadInst>(I);
    auto *S = dyn_cast<StoreInst>(I);
    bool Volatile = L ? L->isVolatile() : S->isVolatile();
    auto Split = [&](auto &&Self, Type *Ty, Value *P, Value *V,
                     Align A) -> Value * {
      if (!Ty->isAggregateType()) {
        if (V) {
          B.CreateAlignedStore(V, P, A, Volatile);
          return nullptr;
        }
        return B.CreateAlignedLoad(Ty, P, A, Volatile);
      }
      auto *ST = dyn_cast<StructType>(Ty);
      uint64_t Count = ST ? ST->getNumElements()
                          : cast<ArrayType>(Ty)->getNumElements();
      Value *Result = UndefValue::get(Ty);
      for (uint64_t N = 0; N < Count; ++N) {
        Type *FieldTy = ST ? ST->getElementType(N)
                            : cast<ArrayType>(Ty)->getElementType();
        uint64_t Offset = ST ? DL.getStructLayout(ST)->getElementOffset(N)
                             : N * DL.getTypeAllocSize(FieldTy).getFixedValue();
        Value *FieldP = B.CreateConstGEP2_32(Ty, P, 0, N);
        Value *FieldV = V ? B.CreateExtractValue(V, N) : nullptr;
        Value *Field = Self(Self, FieldTy, FieldP, FieldV,
                            commonAlignment(A, Offset));
        if (!V)
          Result = B.CreateInsertValue(Result, Field, N);
      }
      return Result;
    };
    if (L) {
      Value *Result = Split(Split, L->getType(), L->getPointerOperand(),
                            nullptr, L->getAlign());
      L->replaceAllUsesWith(Result);
    } else {
      Split(Split, S->getValueOperand()->getType(), S->getPointerOperand(),
            S->getValueOperand(), S->getAlign());
    }
    I->eraseFromParent();
  }
  SmallVector<Instruction *, 32> Work;
  for (Instruction &I : instructions(F)) {
    if (TileBlocks.contains(I.getParent()))
      continue;
    Value *P = nullptr;
    if (auto *L = dyn_cast<LoadInst>(&I))
      P = L->getPointerOperand();
    if (auto *S = dyn_cast<StoreInst>(&I))
      P = S->getPointerOperand();
    if (auto *R = dyn_cast<AtomicRMWInst>(&I))
      P = R->getPointerOperand();
    if (auto *C = dyn_cast<AtomicCmpXchgInst>(&I))
      P = C->getPointerOperand();
    // Native vectors of at least 128 bytes use BLK_TLOAD/TSTORE, whose
    // requests are fixed IntraCore. Preserve carriers for inlining, SROA
    // and element-region legalization instead of extracting Scalar lanes.
    Type *AccessTy = nullptr;
    if (auto *L = dyn_cast<LoadInst>(&I))
      AccessTy = L->getType();
    if (auto *S = dyn_cast<StoreInst>(&I))
      AccessTy = S->getValueOperand()->getType();
    if (!Explicit && AccessTy && isa<FixedVectorType>(AccessTy) &&
        DL.getTypeStoreSize(AccessTy) >= TypeSize::Fixed(128))
      continue;
    // CUBE Local Tile ABI carriers are transported by their own pass.
    if (P && !I.getMetadata("linx.tile.transport") && NeedsScope(P))
      Work.push_back(&I);
  }
  if (Work.empty())
    return !Mem.empty() || !VaCopies.empty() || !Aggregates.empty() ||
           !PrefetchHints.empty();
  // Scoped intrinsics conservatively model normal GM aliasing and effects.
  // Do not leave preexisting function summaries claiming these new calls are
  // readonly/argmemonly/speculatable or cannot synchronize.
  for (Attribute::AttrKind A :
       {Attribute::ReadNone, Attribute::ReadOnly, Attribute::WriteOnly,
        Attribute::ArgMemOnly, Attribute::InaccessibleMemOnly,
        Attribute::InaccessibleMemOrArgMemOnly, Attribute::Speculatable,
        Attribute::NoSync})
    F.removeFnAttr(A);
  Function *Load =
      Intrinsic::getDeclaration(F.getParent(), Intrinsic::linx_scalar_load);
  Function *Store =
      Intrinsic::getDeclaration(F.getParent(), Intrinsic::linx_scalar_store);
  Function *Atomic =
      Intrinsic::getDeclaration(F.getParent(), Intrinsic::linx_scalar_atomic);
  auto OrderCode = [&](AtomicOrdering O) -> unsigned {
    switch (O) {
    case AtomicOrdering::NotAtomic:
    case AtomicOrdering::Unordered:
    case AtomicOrdering::Monotonic:
      return 0;
    case AtomicOrdering::Acquire:
      return 2;
    case AtomicOrdering::Release:
      return 1;
    case AtomicOrdering::AcquireRelease:
      return 3;
    case AtomicOrdering::SequentiallyConsistent:
      return 4;
    }
    llvm_unreachable("atomic order");
  };
  for (Instruction *I : Work) {
    // Request scope does not make an immutable initializer mutable. Fold
    // ordinary constant loads before scoping hides them from IPO/SCCP; shape
    // factories and host tiling constants must remain visible to those passes.
    if (!Explicit)
      if (auto *L = dyn_cast<LoadInst>(I))
        if (L->isSimple())
          if (auto *P = dyn_cast<Constant>(L->getPointerOperand()))
            if (Constant *V = ConstantFoldLoadFromConstPtr(P, L->getType(), DL)) {
              if (!isa<UndefValue>(V) && !isa<PoisonValue>(V)) {
                L->replaceAllUsesWith(V);
                L->eraseFromParent();
                continue;
              }
            }
    IRBuilder<> B(I);
    B.SetCurrentDebugLocation(I->getDebugLoc());
    if (auto *R = dyn_cast<AtomicRMWInst>(I)) {
      if (R->getOperation() != AtomicRMWInst::Xchg &&
          R->getOperation() != AtomicRMWInst::Sub &&
          R->getOperation() != AtomicRMWInst::Add &&
          R->getOperation() != AtomicRMWInst::And &&
          R->getOperation() != AtomicRMWInst::Or &&
          R->getOperation() != AtomicRMWInst::Xor &&
          R->getOperation() != AtomicRMWInst::Min &&
          R->getOperation() != AtomicRMWInst::Max &&
          R->getOperation() != AtomicRMWInst::UMin &&
          R->getOperation() != AtomicRMWInst::UMax)
        Fail("atomic operation must be expanded before scoped lowering");
    }
    auto BitsOf = [&](Value *V) {
      Type *T = V->getType();
      unsigned Bits = DL.getTypeSizeInBits(T).getFixedValue();
      if (T->isPointerTy())
        V = B.CreatePtrToInt(V, B.getIntNTy(Bits));
      else if (!T->isIntegerTy())
        V = B.CreateBitCast(V, B.getIntNTy(Bits));
      return B.CreateZExtOrTrunc(V, B.getInt64Ty());
    };
    auto FromBits = [&](Value *V, Type *T) -> Value * {
      unsigned Bits = DL.getTypeSizeInBits(T).getFixedValue();
      V = B.CreateTruncOrBitCast(V, B.getIntNTy(Bits));
      if (T->isPointerTy())
        return B.CreateIntToPtr(V, T);
      return B.CreateBitCast(V, T);
    };
    auto AtomicCall = [&](Value *P, Value *V, Value *Desired, unsigned Bytes,
                          unsigned O, unsigned Op) -> Value * {
      if (Bytes != 1 && Bytes != 2 && Bytes != 4 && Bytes != 8)
        Fail("unsupported atomic width");
      return B.CreateCall(Atomic,
                          {P, V, Desired, B.getInt32(Bytes), B.getInt32(Scope),
                           B.getInt32(O), B.getInt32(Op)});
    };
    Value *Pointer = nullptr, *ValueIn = nullptr;
    Type *Ty = nullptr;
    Align Alignment(1);
    AtomicOrdering Ordering = AtomicOrdering::NotAtomic;
    if (auto *L = dyn_cast<LoadInst>(I)) {
      Pointer = L->getPointerOperand();
      Ty = L->getType();
      Alignment = L->getAlign();
      Ordering = L->getOrdering();
    } else if (auto *S = dyn_cast<StoreInst>(I)) {
      Pointer = S->getPointerOperand();
      ValueIn = S->getValueOperand();
      Ty = ValueIn->getType();
      Alignment = S->getAlign();
      Ordering = S->getOrdering();
    } else {
      auto *R = dyn_cast<AtomicRMWInst>(I);
      auto *C = dyn_cast<AtomicCmpXchgInst>(I);
      Value *P = R ? R->getPointerOperand() : C->getPointerOperand();
      if (P->getType()->getPointerAddressSpace() != 0)
        Fail("non-GM atomic address space is unsupported");
      Value *V = R ? R->getValOperand() : C->getCompareOperand();
      unsigned Bytes = DL.getTypeStoreSize(V->getType()).getFixedValue();
      if ((R ? R->getAlign() : C->getAlign()).value() < Bytes)
        Fail("unaligned atomic requires a scoped runtime implementation");
      unsigned Op = 2;
      if (R)
        switch (R->getOperation()) {
        case AtomicRMWInst::Xchg:
          Op = 1;
          break;
        case AtomicRMWInst::Sub:
          V = B.CreateNeg(V);
          LLVM_FALLTHROUGH;
        case AtomicRMWInst::Add:
          Op = 0;
          break;
        case AtomicRMWInst::And:
          Op = 5;
          break;
        case AtomicRMWInst::Or:
          Op = 6;
          break;
        case AtomicRMWInst::Xor:
          Op = 7;
          break;
        case AtomicRMWInst::Min:
          Op = 8;
          break;
        case AtomicRMWInst::Max:
          Op = 9;
          break;
        case AtomicRMWInst::UMin:
          Op = 10;
          break;
        case AtomicRMWInst::UMax:
          Op = 11;
          break;
        default:
          llvm_unreachable("checked above");
        }
      Value *Old = AtomicCall(
          P, BitsOf(V), C ? BitsOf(C->getNewValOperand()) : B.getInt64(0),
          Bytes, OrderCode(R ? R->getOrdering() : C->getMergedOrdering()), Op);
      Old = FromBits(Old, V->getType());
      Value *Result = Old;
      if (C) {
        Result = B.CreateInsertValue(UndefValue::get(C->getType()), Old, 0);
        Result = B.CreateInsertValue(Result, B.CreateICmpEQ(Old, V), 1);
      }
      I->replaceAllUsesWith(Result);
      I->eraseFromParent();
      continue;
    }
    if (Pointer->getType()->getPointerAddressSpace() != 0)
      Fail("non-GM address spaces require a separate scope contract");
    if (DL.getTypeStoreSize(Ty).isScalable())
      Fail("scalable memory access is unsupported");
    if (Ty->isAggregateType())
      Fail("aggregate access requires explicit scoped copies");
    unsigned Bytes = DL.getTypeStoreSize(Ty).getFixedValue();
    if (Ordering != AtomicOrdering::NotAtomic) {
      if (Alignment.value() < Bytes)
        Fail("unaligned atomic requires a scoped runtime implementation");
      Value *Result;
      if (!ValueIn)
        Result = AtomicCall(Pointer, B.getInt64(0), B.getInt64(0), Bytes,
                            OrderCode(Ordering), 3);
      else
        Result = AtomicCall(Pointer, BitsOf(ValueIn), B.getInt64(0), Bytes,
                            OrderCode(Ordering), 1);
      if (!ValueIn)
        I->replaceAllUsesWith(FromBits(Result, Ty));
    } else {
      // Preserve wide/vector representation by chunks; avoid a change to a
      // fixed-IntraCore Tile access when the requested domain is InterCore.
      unsigned TotalBits = DL.getTypeSizeInBits(Ty).getFixedValue();
      if (Ty->isIntegerTy(1))
        TotalBits = 8;
      if (TotalBits != Bytes * 8)
        Fail("non-byte-sized access requires explicit builtins");
      Type *IntTy = B.getIntNTy(TotalBits);
      Value *All =
          ValueIn ? (Ty->isPointerTy()   ? B.CreatePtrToInt(ValueIn, IntTy)
                     : Ty->isIntegerTy() ? B.CreateZExtOrTrunc(ValueIn, IntTy)
                                         : B.CreateBitCast(ValueIn, IntTy))
                  : ConstantInt::get(IntTy, 0);
      for (unsigned Offset = 0; Offset < Bytes;) {
        unsigned N = 8;
        while (N > 1 && (N > Bytes - Offset || N > Alignment.value()))
          N /= 2;
        unsigned Shift =
            DL.isLittleEndian() ? Offset * 8 : (Bytes - Offset - N) * 8;
        Value *P = B.CreateConstGEP1_64(B.getInt8Ty(), Pointer, Offset);
        if (ValueIn) {
          Value *Part = B.CreateLShr(All, Shift);
          Part = B.CreateZExtOrTrunc(Part, B.getInt64Ty());
          CallInst *Request =
              B.CreateCall(Store, {P, Part, B.getInt32(N), B.getInt32(Scope)});
          if (!Explicit && !cast<StoreInst>(I)->isVolatile())
            Request->setMetadata("linx.scalar.implicit",
                                 MDNode::get(F.getContext(), {}));
        } else {
          CallInst *Request =
              B.CreateCall(Load, {P, B.getInt32(N), B.getInt32(Scope)});
          if (!Explicit && !cast<LoadInst>(I)->isVolatile())
            Request->setMetadata("linx.scalar.implicit",
                                 MDNode::get(F.getContext(), {}));
          Value *Part = B.CreateZExtOrTrunc(Request, IntTy);
          All = B.CreateOr(All, B.CreateShl(Part, Shift));
        }
        Offset += N;
      }
      if (!ValueIn)
        I->replaceAllUsesWith(Ty->isPointerTy()   ? B.CreateIntToPtr(All, Ty)
                              : Ty->isIntegerTy() ? B.CreateZExtOrTrunc(All, Ty)
                                                  : B.CreateBitCast(All, Ty));
    }
    I->eraseFromParent();
  }
  return !Work.empty() || !Mem.empty();
}

bool LinxV5ScalarShareability::runOnFunction(Function &F) {
  if (TM && (!TM->getSubtargetImpl(F)->isScalar() ||
             TM->getSubtargetImpl(F)->enableLegacyISel()))
    return false;
  return lowerScalarShareability(
      F, getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F),
      getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F));
}

PreservedAnalyses
LinxV5ScalarShareabilityEarlyPass::run(Function &F,
                                       FunctionAnalysisManager &AM) {
  if (TM && (!TM->getSubtargetImpl(F)->isScalar() ||
             TM->getSubtargetImpl(F)->enableLegacyISel()))
    return PreservedAnalyses::all();
  return lowerScalarShareability(F, AM.getResult<TargetIRAnalysis>(F),
                                 AM.getResult<TargetLibraryAnalysis>(F))
             ? PreservedAnalyses::none()
             : PreservedAnalyses::all();
}

FunctionPass *
llvm::createLinxV5ScalarShareabilityPass(const LinxV5TargetMachine *TM) {
  return new LinxV5ScalarShareability(TM);
}
