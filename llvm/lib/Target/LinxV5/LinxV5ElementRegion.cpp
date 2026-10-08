//===- LinxV5ElementRegion.cpp - PTO element region compiler ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Scalar/SROA.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

using namespace llvm;
using namespace llvm::PatternMatch;

#define DEBUG_TYPE "linx-v5-element-region"

namespace {

constexpr StringLiteral ViewPrefix = "pto.element.view:v1;";
constexpr StringLiteral U32M32View =
    "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32";
constexpr StringLiteral RegionLoopMD = "llvm.loop.linx.pto.element.region";

static bool diagnose(Function &F, const Instruction *I, const Twine &Message) {
  F.getContext().diagnose(
      DiagnosticInfoUnsupported(F, Twine("PTO element region: ") + Message,
                                I ? I->getDebugLoc() : DebugLoc(), DS_Error));
  report_fatal_error("PTO element region compilation failed",
                     /*gen_crash_diag=*/false);
}

static bool isIntrinsic(const CallBase *Call, Intrinsic::ID ID) {
  const auto *II = dyn_cast_or_null<IntrinsicInst>(Call);
  return II && II->getIntrinsicID() == ID;
}

static bool getAnnotationString(const CallInst &Call, StringRef &Text) {
  if (!isIntrinsic(&Call, Intrinsic::ptr_annotation) || Call.arg_size() < 2)
    return false;
  return getConstantStringInfo(Call.getArgOperand(1), Text);
}

static bool collectPointerAccesses(Value *Pointer,
                                   SmallVectorImpl<Instruction *> &Accesses,
                                   SmallPtrSetImpl<Value *> &Visited,
                                   Instruction *&InvalidUse) {
  if (!Visited.insert(Pointer).second)
    return true;
  for (User *U : Pointer->users()) {
    if (auto *Load = dyn_cast<LoadInst>(U)) {
      if (Load->getPointerOperand() == Pointer) {
        Accesses.push_back(Load);
        continue;
      }
    }
    if (auto *Store = dyn_cast<StoreInst>(U)) {
      if (Store->getPointerOperand() == Pointer) {
        Accesses.push_back(Store);
        continue;
      }
    }
    if (isa<BitCastInst>(U)) {
      if (!collectPointerAccesses(cast<Value>(U), Accesses, Visited,
                                  InvalidUse))
        return false;
      continue;
    }
    if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
      bool AllZero = true;
      for (Value *Index : GEP->indices()) {
        auto *C = dyn_cast<ConstantInt>(Index);
        if (!C || !C->isZero()) {
          AllZero = false;
          break;
        }
      }
      if (AllZero &&
          collectPointerAccesses(GEP, Accesses, Visited, InvalidUse))
        continue;
    }
    InvalidUse = dyn_cast<Instruction>(U);
    return false;
  }
  return true;
}

static bool prepareViews(Function &F) {
  SmallVector<CallInst *, 8> Annotations;
  for (Instruction &I : instructions(F)) {
    auto *Call = dyn_cast<CallInst>(&I);
    StringRef Text;
    if (Call && getAnnotationString(*Call, Text) && Text.startswith(ViewPrefix))
      Annotations.push_back(Call);
  }
  if (Annotations.empty())
    return false;
  bool HasElementRegion = false;
  for (Instruction &I : instructions(F)) {
    if (isIntrinsic(dyn_cast<CallInst>(&I),
                    Intrinsic::linx_experimental_element_region)) {
      HasElementRegion = true;
      break;
    }
  }
  if (!HasElementRegion) {
    for (CallInst *Annotation : Annotations) {
      Annotation->replaceAllUsesWith(Annotation->getArgOperand(0));
      Annotation->eraseFromParent();
    }
    return true;
  }
  if (F.hasFnAttribute(Attribute::OptimizeNone))
    return false;

  struct PreparedView {
    CallInst *Annotation;
    Value *AnnotatedPointer;
    Value *Storage;
    int64_t ByteOffset;
    uint64_t StorageID;
    uint64_t ViewID;
    SmallVector<Instruction *, 8> Accesses;
  };

  const DataLayout &DL = F.getParent()->getDataLayout();
  DenseMap<Value *, uint64_t> StorageIDs;
  SmallVector<PreparedView, 8> Plans;
  uint64_t NextStorageID = 1;
  uint64_t NextViewID = 1;

  // Preflight every annotation and access before rewriting any IR. This keeps
  // a rejected region from leaving partially consumed view contracts.
  for (CallInst *Annotation : Annotations) {
    StringRef Text;
    (void)getAnnotationString(*Annotation, Text);
    if (Text != U32M32View)
      diagnose(F, Annotation,
               Twine("unsupported logical view contract '") + Text + "'");

    Value *AnnotatedPointer = Annotation->getArgOperand(0);
    int64_t ByteOffset = 0;
    Value *Storage = GetPointerBaseWithConstantOffset(
        AnnotatedPointer, ByteOffset, DL, /*AllowNonInbounds=*/false);
    if (!Storage || (!isa<AllocaInst>(Storage) && !isa<Argument>(Storage) &&
                     !isa<GlobalValue>(Storage)))
      diagnose(F, Annotation,
               "view storage requires a proven object and constant byte offset");

    uint64_t &StorageID = StorageIDs[Storage];
    if (!StorageID)
      StorageID = NextStorageID++;

    PreparedView Plan{Annotation, AnnotatedPointer, Storage, ByteOffset,
                      StorageID, NextViewID++, {}};
    SmallPtrSet<Value *, 16> Visited;
    Instruction *InvalidUse = nullptr;
    if (!collectPointerAccesses(Annotation, Plan.Accesses, Visited,
                                InvalidUse))
      diagnose(F, InvalidUse ? InvalidUse : Annotation,
               "logical view pointer must remain exact; offset, dynamic and "
               "merged pointer aliases are not in P1a");
    if (!Plan.Accesses.empty() && !isa<AllocaInst>(Storage))
      diagnose(F, Annotation,
               "P1a accessed views require distinct local Tile storage");
    for (Instruction *Access : Plan.Accesses) {
      Value *AccessPointer = isa<LoadInst>(Access)
                                 ? cast<LoadInst>(Access)->getPointerOperand()
                                 : cast<StoreInst>(Access)->getPointerOperand();
      Value *CanonicalPointer = AccessPointer;
      while (true) {
        if (CanonicalPointer == Annotation) {
          CanonicalPointer = AnnotatedPointer;
          break;
        }
        if (auto *Cast = dyn_cast<BitCastInst>(CanonicalPointer)) {
          CanonicalPointer = Cast->getOperand(0);
          continue;
        }
        if (auto *GEP = dyn_cast<GetElementPtrInst>(CanonicalPointer)) {
          CanonicalPointer = GEP->getPointerOperand();
          continue;
        }
        break;
      }
      int64_t AccessOffset = 0;
      Value *AccessStorage = GetPointerBaseWithConstantOffset(
          CanonicalPointer, AccessOffset, DL, /*AllowNonInbounds=*/false);
      if (AccessStorage != Plan.Storage || AccessOffset != Plan.ByteOffset)
        diagnose(F, Access,
                 "logical view access changed its storage or byte range");
      Type *CarrierType = nullptr;
      if (auto *Load = dyn_cast<LoadInst>(Access))
        CarrierType = Load->getType();
      else
        CarrierType = cast<StoreInst>(Access)->getValueOperand()->getType();
      if (CarrierType !=
          FixedVectorType::get(Type::getInt32Ty(F.getContext()), 32))
        diagnose(F, Access,
                 "view access is not an exact <32 x i32> carrier");
    }
    Plans.push_back(std::move(Plan));
  }

  Function *View = Intrinsic::getDeclaration(
      F.getParent(), Intrinsic::linx_experimental_element_view,
      {FixedVectorType::get(Type::getInt32Ty(F.getContext()), 32)});
  bool Changed = false;
  for (PreparedView &Plan : Plans) {
    if (Plan.Accesses.empty()) {
      Plan.Annotation->replaceAllUsesWith(Plan.AnnotatedPointer);
      Plan.Annotation->eraseFromParent();
      Changed = true;
      continue;
    }

    auto BuildArgs = [&](Value *Carrier) {
      SmallVector<Value *, 9> Args;
      Args.push_back(Carrier);
      Type *I64 = Type::getInt64Ty(F.getContext());
      Args.push_back(ConstantInt::get(I64, Plan.ViewID));
      Args.push_back(ConstantInt::get(I64, Plan.StorageID));
      Args.push_back(ConstantInt::getSigned(I64, Plan.ByteOffset));
      Args.push_back(ConstantInt::get(I64, 128));
      Args.push_back(ConstantInt::get(I64, 25));
      Args.push_back(ConstantInt::get(I64, 32));
      Args.push_back(ConstantInt::get(I64, 1));
      Args.push_back(ConstantInt::get(I64, 29));
      return Args;
    };

    for (Instruction *Access : Plan.Accesses) {
      if (auto *Load = dyn_cast<LoadInst>(Access)) {
        IRBuilder<> Builder(Load->getNextNode());
        CallInst *Marker =
            Builder.CreateCall(View, BuildArgs(Load), "pto.element.view");
        Marker->setDebugLoc(Load->getDebugLoc());
        Load->replaceUsesWithIf(Marker,
                                [&](Use &U) { return U.getUser() != Marker; });
      } else {
        auto *Store = cast<StoreInst>(Access);
        IRBuilder<> Builder(Store);
        CallInst *Marker = Builder.CreateCall(
            View, BuildArgs(Store->getValueOperand()), "pto.element.view");
        Marker->setDebugLoc(Store->getDebugLoc());
        Store->setOperand(0, Marker);
      }
      Changed = true;
    }

    Plan.Annotation->replaceAllUsesWith(Plan.AnnotatedPointer);
    Plan.Annotation->eraseFromParent();
    Changed = true;
  }
  return Changed;
}
struct ViewDescriptor {
  CallInst *Marker = nullptr;
  uint64_t ViewID = 0;
  uint64_t StorageID = 0;
  int64_t Offset = 0;
  uint64_t Range = 0;
};

static Optional<ViewDescriptor> getViewDescriptor(Value *V) {
  auto *Call = dyn_cast<CallInst>(V);
  if (!isIntrinsic(Call, Intrinsic::linx_experimental_element_view) ||
      Call->arg_size() != 9)
    return None;
  SmallVector<int64_t, 8> Fields;
  for (unsigned I = 1; I != 9; ++I) {
    auto *C = dyn_cast<ConstantInt>(Call->getArgOperand(I));
    if (!C)
      return None;
    Fields.push_back(C->getSExtValue());
  }
  if (Fields[3] != 128 || Fields[4] != 25 || Fields[5] != 32 ||
      Fields[6] != 1 || Fields[7] != 29)
    return None;
  return ViewDescriptor{Call, static_cast<uint64_t>(Fields[0]),
                        static_cast<uint64_t>(Fields[1]), Fields[2],
                        static_cast<uint64_t>(Fields[3])};
}

static MDNode *getElementRegionToken(const Loop &L) {
  MDNode *LoopID = L.getLoopID();
  if (!LoopID)
    return nullptr;
  for (const MDOperand &Operand : drop_begin(LoopID->operands())) {
    auto *Property = dyn_cast_or_null<MDNode>(Operand.get());
    if (!Property || Property->getNumOperands() != 2)
      continue;
    auto *Name = dyn_cast_or_null<MDString>(Property->getOperand(0));
    if (Name && Name->getString() == RegionLoopMD)
      return dyn_cast_or_null<MDNode>(Property->getOperand(1));
  }
  return nullptr;
}

static bool hasElementRegionLoopMetadata(const Instruction &I) {
  MDNode *LoopID = I.getMetadata(LLVMContext::MD_loop);
  if (!LoopID)
    return false;
  for (const MDOperand &Operand : drop_begin(LoopID->operands())) {
    auto *Property = dyn_cast_or_null<MDNode>(Operand.get());
    if (!Property || Property->getNumOperands() < 1)
      continue;
    auto *Name = dyn_cast_or_null<MDString>(Property->getOperand(0));
    if (Name && Name->getString() == RegionLoopMD)
      return true;
  }
  return false;
}

static CallInst *findRegionSentinel(Function &F, MDNode *Token) {
  for (Instruction &I : instructions(F)) {
    auto *Call = dyn_cast<CallInst>(&I);
    if (!isIntrinsic(Call, Intrinsic::linx_experimental_element_region) ||
        Call->arg_size() != 1)
      continue;
    auto *MAV = dyn_cast<MetadataAsValue>(Call->getArgOperand(0));
    if (MAV && MAV->getMetadata() == Token)
      return Call;
  }
  return nullptr;
}

static PHINode *findUnitInduction(Loop &L, ScalarEvolution &SE) {
  for (PHINode &Phi : L.getHeader()->phis()) {
    if (!Phi.getType()->isIntegerTy())
      continue;
    const auto *AR = dyn_cast<SCEVAddRecExpr>(SE.getSCEV(&Phi));
    if (!AR || AR->getLoop() != &L || !AR->isAffine())
      continue;
    const auto *Start = dyn_cast<SCEVConstant>(AR->getStart());
    const auto *Step = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
    if (Start && Step && Start->getAPInt().isZero() && Step->getAPInt().isOne())
      return &Phi;
  }
  return nullptr;
}

static Optional<uint64_t> getProvenTripCount(Loop &L, PHINode *IV,
                                             ScalarEvolution &SE) {
  auto GetConstant = [](Value *V) -> Optional<int64_t> {
    while (auto *Cast = dyn_cast_or_null<CastInst>(V)) {
      if (Cast->getOpcode() != Instruction::Trunc &&
          Cast->getOpcode() != Instruction::ZExt &&
          Cast->getOpcode() != Instruction::SExt)
        break;
      V = Cast->getOperand(0);
    }
    if (auto *C = dyn_cast_or_null<ConstantInt>(V))
      return C->getSExtValue();
    return None;
  };
  if (auto *Branch = dyn_cast<BranchInst>(L.getHeader()->getTerminator())) {
    auto *Compare = Branch->isConditional()
                        ? dyn_cast<ICmpInst>(Branch->getCondition())
                        : nullptr;
    if (Compare) {
      Value *LHS = Compare->getOperand(0);
      Value *RHS = Compare->getOperand(1);
      auto StripCasts = [](Value *V) {
        while (auto *Cast = dyn_cast<CastInst>(V)) {
          if (Cast->getOpcode() != Instruction::Trunc &&
              Cast->getOpcode() != Instruction::ZExt &&
              Cast->getOpcode() != Instruction::SExt)
            break;
          V = Cast->getOperand(0);
        }
        return V;
      };
      LHS = StripCasts(LHS);
      RHS = StripCasts(RHS);
      ICmpInst::Predicate Predicate = Compare->getPredicate();
      if (SE.getSCEV(LHS) != SE.getSCEV(IV) &&
          SE.getSCEV(RHS) == SE.getSCEV(IV)) {
        std::swap(LHS, RHS);
        Predicate = ICmpInst::getSwappedPredicate(Predicate);
      }
      if (SE.getSCEV(LHS) == SE.getSCEV(IV)) {
        Optional<int64_t> Bound = GetConstant(RHS);
        if (Bound && *Bound > 0 &&
            (Predicate == ICmpInst::ICMP_ULT ||
             Predicate == ICmpInst::ICMP_SLT)) {
          uint64_t Trip = static_cast<uint64_t>(*Bound);
          if (L.getLoopLatch() == L.getHeader())
            ++Trip;
          return Trip;
        }
      }
    }
  }
  if (auto Bounds = L.getBounds(SE)) {
    Optional<int64_t> Step = GetConstant(Bounds->getStepValue());
    Optional<int64_t> Initial = GetConstant(&Bounds->getInitialIVValue());
    Optional<int64_t> Final = GetConstant(&Bounds->getFinalIVValue());
    if (Step && Initial && Final && *Step == 1) {
      int64_t Trip = 0;
      switch (Bounds->getCanonicalPredicate()) {
      case ICmpInst::ICMP_SLT:
      case ICmpInst::ICMP_ULT:
      case ICmpInst::ICMP_EQ:
        Trip = *Final - *Initial;
        break;
      case ICmpInst::ICMP_SLE:
      case ICmpInst::ICMP_ULE:
        Trip = *Final - *Initial + 1;
        break;
      default:
        break;
      }
      if (Trip > 0)
        return static_cast<uint64_t>(Trip);
    }
  }
  uint64_t Small = SE.getSmallConstantTripCount(&L);
  return Small ? Optional<uint64_t>(Small) : None;
}

struct RegionPlan {
  Loop *L = nullptr;
  CallInst *Sentinel = nullptr;
  PHINode *IV = nullptr;
  InsertElementInst *Publication = nullptr;
  CallInst *OutputView = nullptr;
  PHINode *OutputPhi = nullptr;
  StoreInst *OutputStore = nullptr;
  SmallVector<CallInst *, 4> ExternalOwnedViews;
  SmallVector<StoreInst *, 2> ExternalStores;
  uint64_t OutputStorageID = 0;
  int64_t OutputOffset = 0;
  uint64_t OutputRange = 0;
  bool IsZeroElseGather = false;
  CallInst *GatherIndexView = nullptr;
  Value *GatherBase = nullptr;
  Value *GatherValid = nullptr;
  Value *GatherOuterPredicate = nullptr;
  LoadInst *GatherLoad = nullptr;
};

static bool isStructuredTileConsumerCall(const CallBase &Call,
                                         Value *Carrier) {
  if (!Call.isInlineAsm())
    return false;
  const auto *Assembly = cast<InlineAsm>(Call.getCalledOperand());
  StringRef Text(Assembly->getAsmString());
  if (!Text.contains("BSTART.TLSU TSTORE") &&
      !Text.contains("BSTART.TEPL 32,"))
    return false;
  for (const Use &Argument : Call.args())
    if (Argument.get() == Carrier &&
        Carrier->getType() ==
            FixedVectorType::get(Type::getInt32Ty(Call.getContext()), 32))
      return true;
  return false;
}

static bool validateExternalConsumers(Function &F, RegionPlan &Plan, Value *V,
                                      SmallPtrSetImpl<Value *> &Visited,
                                      bool &HasConsumer) {
  if (!Visited.insert(V).second)
    return true;
  for (User *U : V->users()) {
    auto *I = dyn_cast<Instruction>(U);
    if (!I || Plan.L->contains(I))
      continue;
    if (auto *Phi = dyn_cast<PHINode>(I)) {
      for (Value *Incoming : Phi->incoming_values()) {
        if (Incoming == V || Incoming == Plan.OutputView ||
            Incoming == Plan.OutputPhi || Incoming == Plan.Publication)
          continue;
        auto *IncomingI = dyn_cast<Instruction>(Incoming);
        if (Incoming->getType() != V->getType() ||
            (IncomingI && Plan.L->contains(IncomingI)))
          return diagnose(F, Phi,
                          "carrier live-out phi has unproved alternate values");
      }
      if (!validateExternalConsumers(F, Plan, Phi, Visited, HasConsumer))
        return false;
      continue;
    }
    if (auto *Store = dyn_cast<StoreInst>(I)) {
      if (Store->getValueOperand() != V || Store->isVolatile() ||
          Store->isAtomic())
        return diagnose(F, Store,
                        "unsupported external carrier store");
      Plan.ExternalStores.push_back(Store);
      HasConsumer = true;
      continue;
    }
    if (auto *Return = dyn_cast<ReturnInst>(I)) {
      if (Return->getReturnValue() != V)
        return diagnose(F, Return, "unsupported carrier return");
      HasConsumer = true;
      continue;
    }
    if (auto *Call = dyn_cast<CallBase>(I)) {
      if (auto Descriptor = getViewDescriptor(Call)) {
        if (Descriptor->StorageID == Plan.OutputStorageID &&
            Descriptor->Offset == Plan.OutputOffset &&
            Descriptor->Range == Plan.OutputRange) {
          HasConsumer = true;
          continue;
        }
      }
      if (!isStructuredTileConsumerCall(*Call, V))
        return diagnose(
            F, Call,
            Twine("unsupported external carrier call: ") +
                (Call->isInlineAsm()
                     ? StringRef(cast<InlineAsm>(Call->getCalledOperand())
                                     ->getAsmString())
                           .take_front(32)
                     : Call->getCalledFunction()
                           ? Call->getCalledFunction()->getName()
                           : StringRef("indirect")));
      HasConsumer = true;
      continue;
    }
    return diagnose(F, I,
                    "unsupported external scalarized carrier consumer");
  }
  return true;
}

static bool sameElementIndex(Value *Index, PHINode *IV, ScalarEvolution &SE) {
  return SE.getSCEV(Index) == SE.getSCEV(IV);
}

static bool validateTileExpression(Value *V, const RegionPlan &Plan,
                                   ScalarEvolution &SE, DominatorTree &DT,
                                   SmallPtrSetImpl<Value *> &Visited,
                                   SmallVectorImpl<ViewDescriptor> &Inputs,
                                   Value *&Rejected) {
  auto Fail = [&]() {
    Rejected = V;
    return false;
  };
  if (!Visited.insert(V).second)
    return true;
  if (V == Plan.IV)
    return V->getType()->isIntegerTy(32) || Fail();
  if (isa<ConstantInt>(V))
    return V->getType()->isIntegerTy(32) || Fail();
  if (!V->getType()->isIntegerTy(32) &&
      !V->getType()->isVectorTy())
    return Fail();
  if (isa<CastInst>(V))
    return Fail();
  if (auto Descriptor = getViewDescriptor(V)) {
    if (V->getType() !=
        FixedVectorType::get(Type::getInt32Ty(V->getContext()), 32))
      return Fail();
    Value *Carrier = Descriptor->Marker->getArgOperand(0);
    if (auto *CarrierI = dyn_cast<Instruction>(Carrier)) {
      if (auto *Load = dyn_cast<LoadInst>(CarrierI)) {
        if (Load->isVolatile() || Load->isAtomic())
          return Fail();
        if (Plan.L->contains(Load) ||
            !DT.dominates(Load,
                          Plan.L->getLoopPreheader()->getTerminator()))
          return Fail();
      } else if (!DT.dominates(
                     CarrierI, Plan.L->getLoopPreheader()->getTerminator())) {
        return Fail();
      }
    }
    Inputs.push_back(*Descriptor);
    return true;
  }
  if (auto *Extract = dyn_cast<ExtractElementInst>(V)) {
    if (!Extract->getType()->isIntegerTy(32) ||
        !sameElementIndex(Extract->getIndexOperand(), Plan.IV, SE))
      return Fail();
    return validateTileExpression(Extract->getVectorOperand(), Plan, SE, DT,
                                  Visited, Inputs, Rejected);
  }
  if (auto *Binary = dyn_cast<BinaryOperator>(V)) {
    if (!Binary->getType()->isIntegerTy(32) ||
        !Binary->getOperand(0)->getType()->isIntegerTy(32) ||
        !Binary->getOperand(1)->getType()->isIntegerTy(32))
      return Fail();
    switch (Binary->getOpcode()) {
    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Mul:
    case Instruction::UDiv:
    case Instruction::URem:
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
    case Instruction::Shl:
    case Instruction::LShr:
      return validateTileExpression(Binary->getOperand(0), Plan, SE, DT,
                                    Visited, Inputs, Rejected) &&
             validateTileExpression(Binary->getOperand(1), Plan, SE, DT,
                                    Visited, Inputs, Rejected);
    default:
      return Fail();
    }
  }
  if (auto *I = dyn_cast<Instruction>(V)) {
    if (!Plan.L->contains(I) && I->getType()->isIntegerTy(32) &&
        DT.dominates(I, Plan.L->getLoopPreheader()->getTerminator()))
      return true;
    return Fail();
  }
  return V->getType()->isIntegerTy(32) || Fail();
}

static bool planRegion(Function &F, Loop &L, LoopInfo &LI, ScalarEvolution &SE,
                       DominatorTree &DT, PostDominatorTree &PDT,
                       AAResults &AA, MemorySSA &MSSA, RegionPlan &Plan) {
  (void)LI;
  (void)PDT;
  MDNode *Token = getElementRegionToken(L);
  if (!Token)
    return true;
  Plan.L = &L;
  Plan.Sentinel = findRegionSentinel(F, Token);
  if (!Plan.Sentinel)
    return diagnose(F, L.getHeader()->getTerminator(),
                    "marked loop has no matching mandatory sentinel");
  if (!L.isInnermost() || !L.getLoopPreheader() || !L.getLoopLatch() ||
      !L.getExitBlock())
    return diagnose(F, L.getHeader()->getTerminator(),
                    "requires an innermost loop with preheader, latch and "
                    "unique exit");
  Plan.IV = findUnitInduction(L, SE);
  if (!Plan.IV)
    return diagnose(F, L.getHeader()->getTerminator(),
                    "requires an induction equivalent to 0..31 step 1");
  Optional<uint64_t> TripCount = getProvenTripCount(L, Plan.IV, SE);
  if (!TripCount || *TripCount != 32)
    return diagnose(F, L.getHeader()->getTerminator(),
                    "requires a proven 32-element iteration domain");

  SmallVector<CallInst *, 4> OutputViews;
  SmallVector<StoreInst *, 4> DescriptorStores;
  SmallVector<LoadInst *, 2> ScalarLoads;
  for (BasicBlock *BB : L.blocks()) {
    for (Instruction &I : *BB) {
      if (isa<InvokeInst>(I) || isa<CallBrInst>(I) ||
          isa<LandingPadInst>(I) || isa<ResumeInst>(I))
        return diagnose(F, &I,
                        "exceptional control flow is not supported in PTO "
                        "element regions");
      if (I.isTerminator() || isa<PHINode>(I) || isa<DbgInfoIntrinsic>(I))
        continue;
      bool IsVolatileOrAtomic = false;
      if (auto *Load = dyn_cast<LoadInst>(&I))
        IsVolatileOrAtomic = Load->isVolatile() || Load->isAtomic();
      else if (auto *Store = dyn_cast<StoreInst>(&I))
        IsVolatileOrAtomic = Store->isVolatile() || Store->isAtomic();
      else
        IsVolatileOrAtomic = isa<AtomicRMWInst>(I) ||
                             isa<AtomicCmpXchgInst>(I) || isa<FenceInst>(I);
      if (IsVolatileOrAtomic)
        return diagnose(F, &I, "volatile and atomic effects are not in P1a");
      if (auto *Call = dyn_cast<CallInst>(&I)) {
        if (!isIntrinsic(Call, Intrinsic::linx_experimental_element_view))
          return diagnose(F, Call,
                          "calls are not in the P1a arithmetic region");
        if (isa<InsertElementInst>(Call->getArgOperand(0)))
          OutputViews.push_back(Call);
        continue;
      }
      if (I.mayReadOrWriteMemory()) {
        bool DescriptorAccess = false;
        if (auto *Load = dyn_cast<LoadInst>(&I)) {
          for (User *U : Load->users())
            if (getViewDescriptor(cast<Value>(U))) {
              DescriptorAccess = true;
              break;
            }
          if (!DescriptorAccess && Load->getType()->isIntegerTy(32) &&
              !Load->isVolatile() && !Load->isAtomic()) {
            ScalarLoads.push_back(Load);
            DescriptorAccess = true;
          }
        } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
          DescriptorAccess =
              getViewDescriptor(Store->getValueOperand()).hasValue();
          if (DescriptorAccess)
            DescriptorStores.push_back(Store);
        }
        if (!DescriptorAccess || !MSSA.getMemoryAccess(&I))
          return diagnose(F, &I,
                          "unpromoted or unproved memory access in arithmetic region");
      }
    }
  }
  if (OutputViews.size() != 1)
    return diagnose(F, L.getHeader()->getTerminator(),
                    "P1a requires one complete carrier publication");
  Plan.OutputView = OutputViews.front();
  auto Descriptor = getViewDescriptor(Plan.OutputView);
  if (!Descriptor)
    return diagnose(F, Plan.OutputView,
                    "publication lacks exact U32/M32 view metadata");
  Plan.Publication = cast<InsertElementInst>(Plan.OutputView->getArgOperand(0));
  Plan.OutputStorageID = Descriptor->StorageID;
  Plan.OutputOffset = Descriptor->Offset;
  Plan.OutputRange = Descriptor->Range;
  if (!sameElementIndex(Plan.Publication->getOperand(2), Plan.IV, SE))
    return diagnose(F, Plan.Publication,
                    "publication index is not the region induction element");
  if (!DT.dominates(Plan.OutputView, L.getLoopLatch()->getTerminator()))
    return diagnose(F, Plan.OutputView,
                    "publication must execute on every region iteration");

  if (auto *Merge = dyn_cast<PHINode>(Plan.Publication->getOperand(1))) {
    if (Merge->getNumIncomingValues() == 2 &&
        Merge->getParent() == Plan.Publication->getParent()) {
      LoadInst *Loaded = nullptr;
      BasicBlock *LoadIncoming = nullptr;
      bool HasZero = false;
      for (unsigned I = 0; I != 2; ++I) {
        Value *Incoming = Merge->getIncomingValue(I);
        if (auto *C = dyn_cast<ConstantInt>(Incoming))
          HasZero |= C->isZero();
        if (auto *LI = dyn_cast<LoadInst>(Incoming)) {
          Loaded = LI;
          LoadIncoming = Merge->getIncomingBlock(I);
        }
      }
      auto *GEP = Loaded
                      ? dyn_cast<GetElementPtrInst>(Loaded->getPointerOperand())
                      : nullptr;
      Value *ElementIndex = GEP && GEP->getNumIndices() == 1
                                ? GEP->idx_begin()->get()
                                : nullptr;
      if (auto *Cast = dyn_cast_or_null<ZExtInst>(ElementIndex)) {
        if (Cast->getSrcTy()->isIntegerTy(32) &&
            Cast->getDestTy()->isIntegerTy(64))
          ElementIndex = Cast->getOperand(0);
        else
          ElementIndex = nullptr;
      } else if (ElementIndex && !ElementIndex->getType()->isIntegerTy(32)) {
        ElementIndex = nullptr;
      }
      auto *Extract = dyn_cast_or_null<ExtractElementInst>(ElementIndex);
      auto IndexView = Extract
                           ? getViewDescriptor(Extract->getVectorOperand())
                           : None;
      BasicBlock *BranchBlock =
          LoadIncoming && LoadIncoming->getSinglePredecessor()
              ? LoadIncoming->getSinglePredecessor()
              : nullptr;
      auto *Branch = BranchBlock
                         ? dyn_cast<BranchInst>(BranchBlock->getTerminator())
                         : nullptr;
      Value *Condition = Branch && Branch->isConditional()
                             ? Branch->getCondition()
                             : nullptr;
      Value *OuterPredicate = nullptr;
      if (auto *Select = dyn_cast_or_null<SelectInst>(Condition)) {
        if (match(Select->getFalseValue(), m_Zero())) {
          OuterPredicate = Select->getCondition();
          Condition = Select->getTrueValue();
        }
      }
      auto *Compare = dyn_cast_or_null<ICmpInst>(Condition);
      Value *Valid = nullptr;
      if (Compare && Compare->getPredicate() == ICmpInst::ICMP_ULT) {
        if (SE.getSCEV(Compare->getOperand(0)) == SE.getSCEV(Plan.IV))
          Valid = Compare->getOperand(1);
      }
      Value *Base = GEP ? GEP->getPointerOperand() : nullptr;
      const Value *Underlying = Base ? getUnderlyingObject(Base) : nullptr;
      auto *BaseArgument = dyn_cast_or_null<Argument>(Underlying);
      bool LoadArm = Branch && Branch->getSuccessor(0) == LoadIncoming;
      bool MergeArm = Branch && Branch->getSuccessor(1) == Merge->getParent();
      bool BaseReady = Base && L.isLoopInvariant(Base);
      if (auto *BaseI = dyn_cast_or_null<Instruction>(Base))
        BaseReady &= DT.dominates(
            BaseI, L.getLoopPreheader()->getTerminator());
      bool PredicateReady = !OuterPredicate || L.isLoopInvariant(OuterPredicate);
      if (auto *PredicateI = dyn_cast_or_null<Instruction>(OuterPredicate))
        PredicateReady &= DT.dominates(
            PredicateI, L.getLoopPreheader()->getTerminator());
      bool ClobberedInLoop = false;
      if (Loaded) {
        MemoryAccess *Access = MSSA.getMemoryAccess(Loaded);
        MemoryAccess *Clobber =
            Access ? MSSA.getWalker()->getClobberingMemoryAccess(Access)
                   : nullptr;
        if (auto *Def = dyn_cast_or_null<MemoryDef>(Clobber))
          ClobberedInLoop = L.contains(Def->getBlock());
      }
      if (HasZero && Loaded && Extract && IndexView &&
          sameElementIndex(Extract->getIndexOperand(), Plan.IV, SE) &&
          Valid && Valid->getType()->isIntegerTy(32) &&
          L.isLoopInvariant(Valid) && PredicateReady && LoadArm && MergeArm &&
          GEP->getSourceElementType()->isIntegerTy(32) && BaseReady &&
          BaseArgument &&
          !ClobberedInLoop && ScalarLoads.size() == 1 &&
          ScalarLoads.front() == Loaded) {
        Plan.IsZeroElseGather = true;
        Plan.GatherIndexView = IndexView->Marker;
        Plan.GatherBase = Base;
        Plan.GatherValid = Valid;
        Plan.GatherOuterPredicate = OuterPredicate;
        Plan.GatherLoad = Loaded;
      }
    }
  }
  if (!ScalarLoads.empty() && !Plan.IsZeroElseGather)
    return diagnose(F, ScalarLoads.front(),
                    "P1b requires one proved zero-inactive gather diamond");

  for (PHINode &Phi : L.getHeader()->phis()) {
    int LatchIndex = Phi.getBasicBlockIndex(L.getLoopLatch());
    if (LatchIndex >= 0 &&
        (Phi.getIncomingValue(LatchIndex) == Plan.OutputView ||
         Phi.getIncomingValue(LatchIndex) == Plan.Publication)) {
      Plan.OutputPhi = &Phi;
      break;
    }
  }
  for (User *U : Plan.OutputView->users())
    if (auto *Store = dyn_cast<StoreInst>(U))
      if (L.contains(Store) && Store->getValueOperand() == Plan.OutputView) {
        Plan.OutputStore = Store;
        break;
      }
  if (!DescriptorStores.empty()) {
    if (DescriptorStores.size() != 1 ||
        DescriptorStores.front() != Plan.OutputStore)
      return diagnose(F, DescriptorStores.front(),
                      "P1a requires exactly one proved Tile publication store");
    if (!DT.dominates(Plan.OutputStore,
                      L.getLoopLatch()->getTerminator()))
      return diagnose(F, Plan.OutputStore,
                      "publication store must execute on every iteration");
    Value *Pointer = Plan.OutputStore->getPointerOperand();
    if (auto *PointerI = dyn_cast<Instruction>(Pointer))
      if (!DT.dominates(PointerI,
                        L.getLoopPreheader()->getTerminator()))
        return diagnose(F, Plan.OutputStore,
                        "publication pointer must dominate the element region");
  }
  if (Plan.IsZeroElseGather && Plan.OutputStore &&
      AA.alias(MemoryLocation::get(Plan.GatherLoad),
               MemoryLocation::get(Plan.OutputStore)) != AliasResult::NoAlias)
    return diagnose(F, Plan.GatherLoad,
                    "gather source may alias its Tile publication storage");
  bool HasWholeCarrierConsumer = Plan.OutputStore != nullptr;
  SmallPtrSet<Value *, 8> ConsumerVisited;
  if (!validateExternalConsumers(F, Plan, Plan.OutputView, ConsumerVisited,
                                 HasWholeCarrierConsumer))
    return false;
  if (Plan.OutputPhi &&
      !validateExternalConsumers(F, Plan, Plan.OutputPhi, ConsumerVisited,
                                 HasWholeCarrierConsumer))
    return false;
  if (!HasWholeCarrierConsumer)
    return diagnose(F, Plan.OutputView,
                    "publication has no proved whole-carrier consumer");
  if (Plan.OutputPhi) {
    int PreheaderIndex =
        Plan.OutputPhi->getBasicBlockIndex(L.getLoopPreheader());
    if (PreheaderIndex >= 0) {
      if (auto Initial = getViewDescriptor(
              Plan.OutputPhi->getIncomingValue(PreheaderIndex))) {
        for (User *U : Initial->Marker->users()) {
          auto *UserI = dyn_cast<Instruction>(U);
          if (!UserI || !L.contains(UserI))
            return diagnose(
                F, Initial->Marker,
                "logical output view is also used outside its region");
        }
        if (!L.contains(Initial->Marker))
          Plan.ExternalOwnedViews.push_back(Initial->Marker);
      }
    }
  }
  if (!DT.dominates(Plan.Sentinel, L.getHeader()))
    return diagnose(F, Plan.Sentinel,
                    "region sentinel does not dominate its loop");
  for (PHINode &Phi : L.getHeader()->phis()) {
    int LatchIndex = Phi.getBasicBlockIndex(L.getLoopLatch());
    if (LatchIndex >= 0 && &Phi != Plan.IV && &Phi != Plan.OutputPhi)
      return diagnose(F, &Phi,
                      "loop-carried recurrences are not in P1a");
  }
  for (BasicBlock *BB : L.blocks()) {
    for (Instruction &I : *BB) {
      if (&I == Plan.OutputView || &I == Plan.OutputPhi)
        continue;
      for (User *U : I.users()) {
        auto *UserI = dyn_cast<Instruction>(U);
        if (UserI && !L.contains(UserI))
          return diagnose(F, &I,
                          "scalar and carrier live-outs are not in P1a");
      }
    }
  }
  SmallPtrSet<Value *, 32> Visited;
  SmallVector<ViewDescriptor, 4> Inputs;
  Value *Rejected = nullptr;
  bool ExpressionValid = true;
  if (Plan.IsZeroElseGather) {
    ExpressionValid = validateTileExpression(
        Plan.GatherIndexView, Plan, SE, DT, Visited, Inputs, Rejected);
  } else {
    ExpressionValid = validateTileExpression(
        Plan.Publication->getOperand(1), Plan, SE, DT, Visited, Inputs,
        Rejected);
  }
  if (!ExpressionValid) {
    std::string Detail;
    raw_string_ostream OS(Detail);
    if (Rejected)
      Rejected->printAsOperand(OS, /*PrintType=*/true);
    return diagnose(F, Plan.Publication,
                    Twine("unsupported scalar expression in P1a arithmetic region: ") +
                        OS.str());
  }
  for (const ViewDescriptor &Input : Inputs) {
    for (User *U : Input.Marker->users()) {
      auto *UserI = dyn_cast<Instruction>(U);
      if (!UserI || !L.contains(UserI))
        return diagnose(F, Input.Marker,
                        "logical input view is also used outside its region");
    }
    if (!L.contains(Input.Marker))
      Plan.ExternalOwnedViews.push_back(Input.Marker);
    bool SameStorage = Input.StorageID == Descriptor->StorageID;
    bool SameRange = Input.Offset == Descriptor->Offset &&
                     Input.Range == Descriptor->Range;
    bool Disjoint = Input.Offset + static_cast<int64_t>(Input.Range) <=
                        Descriptor->Offset ||
                    Descriptor->Offset +
                            static_cast<int64_t>(Descriptor->Range) <=
                        Input.Offset;
    if (SameStorage && !SameRange && !Disjoint)
      return diagnose(F, Input.Marker,
                      "partially overlapping logical views are not independent");
  }
  return true;
}

class TileExpressionBuilder {
  RegionPlan &Plan;
  ScalarEvolution &SE;
  DominatorTree &DT;
  IRBuilder<> Builder;
  DenseMap<Value *, Value *> Values;
  Function *TCI;
  Function *TBinary;
  Function *TLoad;

  Value *splat(Value *V) {
    if (!V->getType()->isIntegerTy(64))
      V = Builder.CreateZExtOrTrunc(V, Builder.getInt64Ty());
    return Builder.CreateCall(TCI,
                              {Builder.getInt64(32), Builder.getInt64(1),
                               Builder.getInt64(25), Builder.getInt64(29), V,
                               Builder.getInt64(0)},
                              "pto.element.splat");
  }

public:
  TileExpressionBuilder(Function &F, RegionPlan &Plan, ScalarEvolution &SE,
                        DominatorTree &DT)
      : Plan(Plan), SE(SE), DT(DT),
        Builder(Plan.L->getLoopPreheader()->getTerminator()) {
    Type *VectorTy = FixedVectorType::get(Builder.getInt32Ty(), 32);
    TCI = Intrinsic::getDeclaration(
        F.getParent(), Intrinsic::linx_experimental_ew_tci, {VectorTy});
    TBinary = Intrinsic::getDeclaration(
        F.getParent(), Intrinsic::linx_experimental_ew_tbinary, {VectorTy});
    TLoad = Intrinsic::getDeclaration(
        F.getParent(), Intrinsic::linx_blk_tload, {VectorTy});
  }

  Value *lower(Value *V) {
    auto It = Values.find(V);
    if (It != Values.end())
      return It->second;
    Value *Result = nullptr;
    if (V == Plan.IV) {
      Result = Builder.CreateCall(TCI,
                                  {Builder.getInt64(32), Builder.getInt64(1),
                                   Builder.getInt64(25), Builder.getInt64(29),
                                   Builder.getInt64(0),
                                   Builder.getInt64(1ULL << 32)},
                                  "pto.element.index");
    } else if (auto *C = dyn_cast<ConstantInt>(V)) {
      Result = splat(ConstantInt::get(Builder.getInt64Ty(),
                                      C->getValue().zextOrTrunc(64)));
    } else if (auto Descriptor = getViewDescriptor(V)) {
      Value *Carrier = Descriptor->Marker->getArgOperand(0);
      if (auto *Load = dyn_cast<LoadInst>(Carrier)) {
        IRBuilder<> LoadBuilder(Load);
        Result = LoadBuilder.CreateCall(
            TLoad,
            {LoadBuilder.getInt64(1), LoadBuilder.getInt64(32),
             LoadBuilder.getInt64(1), LoadBuilder.getInt64(25),
             LoadBuilder.getInt64(0), LoadBuilder.getInt64(21),
             Load->getPointerOperand(), LoadBuilder.getInt64(4)},
            "pto.element.carrier");
      } else {
        if (auto *CarrierI = dyn_cast<Instruction>(Carrier))
          if (!DT.dominates(
                  CarrierI,
                  Plan.L->getLoopPreheader()->getTerminator()))
            return nullptr;
        Result = Carrier;
      }
    } else if (auto *Extract = dyn_cast<ExtractElementInst>(V)) {
      if (!sameElementIndex(Extract->getIndexOperand(), Plan.IV, SE))
        return nullptr;
      Result = lower(Extract->getVectorOperand());
    } else if (auto *Binary = dyn_cast<BinaryOperator>(V)) {
      if (!Binary->getType()->isIntegerTy(32) ||
          !Binary->getOperand(0)->getType()->isIntegerTy(32) ||
          !Binary->getOperand(1)->getType()->isIntegerTy(32))
        return nullptr;
      unsigned Opcode;
      switch (Binary->getOpcode()) {
      case Instruction::Add:
        Opcode = 0;
        break;
      case Instruction::Sub:
        Opcode = 1;
        break;
      case Instruction::Mul:
        Opcode = 2;
        break;
      case Instruction::UDiv:
        Opcode = 3;
        break;
      case Instruction::URem:
        Opcode = 4;
        break;
      case Instruction::And:
        Opcode = 5;
        break;
      case Instruction::Or:
        Opcode = 6;
        break;
      case Instruction::Xor:
        Opcode = 7;
        break;
      case Instruction::Shl:
        Opcode = 8;
        break;
      case Instruction::LShr:
        Opcode = 9;
        break;
      default:
        return nullptr;
      }
      Value *LHS = lower(Binary->getOperand(0));
      Value *RHS = lower(Binary->getOperand(1));
      if (!LHS || !RHS)
        return nullptr;
      Result = Builder.CreateCall(TBinary,
                                  {Builder.getInt64(32), Builder.getInt64(1),
                                   Builder.getInt64(25), Builder.getInt64(29),
                                   Builder.getInt64(Opcode), LHS, RHS},
                                  "pto.element.value");
    } else if (auto *I = dyn_cast<Instruction>(V)) {
      if (!Plan.L->contains(I) && DT.dominates(I, Builder.GetInsertBlock()) &&
          I->getType()->isIntegerTy(32))
        Result = splat(I);
    } else if (V->getType()->isIntegerTy(32)) {
      Result = splat(V);
    }
    if (Result)
      Values[V] = Result;
    return Result;
  }
};

static bool lowerRegions(Function &F, LoopInfo &LI, ScalarEvolution &SE,
                         DominatorTree &DT, PostDominatorTree &PDT,
                         AAResults &AA, MemorySSA &MSSA) {
  SmallVector<Loop *, 8> Loops;
  std::function<void(Loop *)> Collect = [&](Loop *L) {
    for (Loop *Sub : *L)
      Collect(Sub);
    if (getElementRegionToken(*L))
      Loops.push_back(L);
  };
  for (Loop *L : LI)
    Collect(L);
  if (Loops.empty())
    return false;
  if (F.hasFnAttribute(Attribute::OptimizeNone)) {
    CallInst *Sentinel =
        findRegionSentinel(F, getElementRegionToken(*Loops.front()));
    return diagnose(F, Sentinel,
                    "typed Tile spill/reload is unsupported at -O0");
  }

  SmallVector<RegionPlan, 8> Plans;
  for (Loop *L : Loops) {
    RegionPlan Plan;
    if (!planRegion(F, *L, LI, SE, DT, PDT, AA, MSSA, Plan))
      return false;
    Plans.push_back(Plan);
  }

  for (RegionPlan &Plan : Plans) {
    TileExpressionBuilder Expressions(F, Plan, SE, DT);
    Value *Result = nullptr;
    if (Plan.IsZeroElseGather) {
      IRBuilder<> Builder(Plan.L->getLoopPreheader()->getTerminator());
      Type *ValueTy = FixedVectorType::get(Builder.getInt32Ty(), 32);
      Type *OffsetTy = FixedVectorType::get(Builder.getInt64Ty(), 32);
      Value *Indices = Expressions.lower(Plan.GatherIndexView);
      Function *TCI = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tci, {ValueTy});
      Value *Lanes = Builder.CreateCall(
          TCI,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Builder.getInt64(0),
           Builder.getInt64(1ULL << 32)},
          "pto.gather.lanes");
      Function *Compare = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tcmps_gpr,
          {ValueTy});
      Value *Valid = Plan.GatherValid;
      if (!Valid->getType()->isIntegerTy(64))
        Valid = Builder.CreateZExtOrTrunc(Valid, Builder.getInt64Ty());
      Value *Mask = Builder.CreateCall(
          Compare,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Lanes, Valid, Builder.getInt64(2)},
          "pto.gather.mask");
      if (Plan.GatherOuterPredicate)
        Mask = Builder.CreateSelect(Plan.GatherOuterPredicate, Mask,
                                    Builder.getInt64(0),
                                    "pto.gather.active.mask");
      Function *TLEA = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tlea,
          {OffsetTy, ValueTy});
      Value *Offsets = Builder.CreateCall(
          TLEA,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Indices, Builder.getInt64(32)},
          "pto.gather.byte.offsets");
      Function *Gather = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_mgather_gpr_masked,
          {ValueTy, OffsetTy});
      Result = Builder.CreateCall(
          Gather,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(0), Builder.getInt64(29), Builder.getInt64(24),
           Plan.GatherBase, Offsets, Mask, Builder.getInt64(0),
           Builder.getInt64(0), Builder.getInt64(1)},
          "pto.gather.value");
    } else {
      Result = Expressions.lower(Plan.Publication->getOperand(1));
    }
    if (!Result) {
      diagnose(F, Plan.Publication,
               "unsupported scalar expression in P1a arithmetic region");
      return false;
    }

    SmallVector<Use *, 8> ExternalUses;
    auto CollectExternalUses = [&](Value *V) {
      for (Use &U : V->uses()) {
        auto *UserI = dyn_cast<Instruction>(U.getUser());
        if (UserI && !Plan.L->contains(UserI))
          ExternalUses.push_back(&U);
      }
    };
    if (Plan.OutputPhi)
      CollectExternalUses(Plan.OutputPhi);
    CollectExternalUses(Plan.OutputView);
    for (Use *U : ExternalUses)
      U->set(Result);
    for (StoreInst *Store : Plan.ExternalStores) {
      IRBuilder<> StoreBuilder(Store);
      Type *VectorTy = FixedVectorType::get(StoreBuilder.getInt32Ty(), 32);
      Function *TStore = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_blk_tstore, {VectorTy});
      StoreBuilder.CreateCall(
          TStore,
          {StoreBuilder.getInt64(1), StoreBuilder.getInt64(32),
           StoreBuilder.getInt64(1), StoreBuilder.getInt64(25),
           StoreBuilder.getInt64(24), Store->getPointerOperand(),
           StoreBuilder.getInt64(4), Result});
      Store->eraseFromParent();
    }
    if (Plan.OutputStore) {
      IRBuilder<> Builder(Plan.L->getLoopPreheader()->getTerminator());
      Type *VectorTy = FixedVectorType::get(Builder.getInt32Ty(), 32);
      Function *TStore = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_blk_tstore, {VectorTy});
      Builder.CreateCall(
          TStore,
          {Builder.getInt64(1), Builder.getInt64(32), Builder.getInt64(1),
           Builder.getInt64(25), Builder.getInt64(24),
           Plan.OutputStore->getPointerOperand(), Builder.getInt64(4), Result});
    }
    Plan.Sentinel->eraseFromParent();
    formLCSSARecursively(*Plan.L, DT, &LI, &SE);
    deleteDeadLoop(Plan.L, &DT, &SE, &LI, &MSSA);
    for (CallInst *Marker : Plan.ExternalOwnedViews) {
      if (!Marker->getParent())
        continue;
      if (!Marker->use_empty())
        diagnose(F, Marker, "consumed logical view still has live users");
      Marker->eraseFromParent();
    }
  }
  return true;
}

static bool verifyNoResidualElementContracts(Function &F) {
  for (Instruction &I : instructions(F)) {
    if (hasElementRegionLoopMetadata(I))
      return diagnose(F, &I, "marked scalar loop survived required lowering");
    auto *Call = dyn_cast<CallInst>(&I);
    if (isIntrinsic(Call, Intrinsic::linx_experimental_element_region) ||
        isIntrinsic(Call, Intrinsic::linx_experimental_element_view))
      return diagnose(F, &I, "required region lowering did not complete");
    StringRef Text;
    if (Call && getAnnotationString(*Call, Text) && Text.startswith(ViewPrefix))
      return diagnose(F, &I, "logical view annotation survived lowering");
  }
  return false;
}

class LinxV5ElementRegionPrepareLegacyPass : public FunctionPass {
public:
  static char ID;
  LinxV5ElementRegionPrepareLegacyPass() : FunctionPass(ID) {
    initializeLinxV5ElementRegionPrepareLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override {
    return prepareViews(F);
  }
};

class LinxV5ElementRegionLegacyPass : public FunctionPass {
public:
  static char ID;
  LinxV5ElementRegionLegacyPass() : FunctionPass(ID) {
    initializeLinxV5ElementRegionLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override {
    return lowerRegions(
        F, getAnalysis<LoopInfoWrapperPass>().getLoopInfo(),
        getAnalysis<ScalarEvolutionWrapperPass>().getSE(),
        getAnalysis<DominatorTreeWrapperPass>().getDomTree(),
        getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree(),
        getAnalysis<AAResultsWrapperPass>().getAAResults(),
        getAnalysis<MemorySSAWrapperPass>().getMSSA());
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<MemorySSAWrapperPass>();
  }
};

class LinxV5ElementRegionVerifierLegacyPass : public FunctionPass {
public:
  static char ID;
  LinxV5ElementRegionVerifierLegacyPass() : FunctionPass(ID) {
    initializeLinxV5ElementRegionVerifierLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override {
    verifyNoResidualElementContracts(F);
    return false;
  }
};

} // end anonymous namespace

char LinxV5ElementRegionPrepareLegacyPass::ID = 0;
char LinxV5ElementRegionLegacyPass::ID = 0;
char LinxV5ElementRegionVerifierLegacyPass::ID = 0;

INITIALIZE_PASS(LinxV5ElementRegionPrepareLegacyPass,
                "linx-v5-element-region-prepare",
                "LinxV5 PTO element region view preparation", false, false)

INITIALIZE_PASS_BEGIN(LinxV5ElementRegionLegacyPass, "linx-v5-element-region",
                      "LinxV5 PTO element region compiler", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MemorySSAWrapperPass)
INITIALIZE_PASS_END(LinxV5ElementRegionLegacyPass, "linx-v5-element-region",
                    "LinxV5 PTO element region compiler", false, false)

INITIALIZE_PASS(LinxV5ElementRegionVerifierLegacyPass,
                "linx-v5-element-region-verify",
                "LinxV5 PTO element region verifier", false, false)

FunctionPass *llvm::createLinxV5ElementRegionPreparePass() {
  return new LinxV5ElementRegionPrepareLegacyPass();
}

FunctionPass *llvm::createLinxV5ElementRegionPass() {
  return new LinxV5ElementRegionLegacyPass();
}

FunctionPass *llvm::createLinxV5ElementRegionVerifierPass() {
  return new LinxV5ElementRegionVerifierLegacyPass();
}

PreservedAnalyses
LinxV5ElementRegionPreparePass::run(Function &F, FunctionAnalysisManager &AM) {
  (void)AM;
  return prepareViews(F) ? PreservedAnalyses::none()
                         : PreservedAnalyses::all();
}

PreservedAnalyses
LinxV5ElementRegionPromotePass::run(Function &F, FunctionAnalysisManager &AM) {
  bool HasRegion = false;
  for (Instruction &I : instructions(F)) {
    if (isIntrinsic(dyn_cast<CallInst>(&I),
                    Intrinsic::linx_experimental_element_region)) {
      HasRegion = true;
      break;
    }
  }
  if (!HasRegion)
    return PreservedAnalyses::all();

  bool HadOptNone = F.hasFnAttribute(Attribute::OptimizeNone);
  if (HadOptNone)
    F.removeFnAttr(Attribute::OptimizeNone);
  PreservedAnalyses SROAPA = SROAPass().run(F, AM);
  AM.invalidate(F, SROAPA);
  PreservedAnalyses PromotePA = PromotePass().run(F, AM);
  AM.invalidate(F, PromotePA);
  if (HadOptNone)
    F.addFnAttr(Attribute::OptimizeNone);
  return PreservedAnalyses::none();
}

PreservedAnalyses LinxV5ElementRegionPass::run(Function &F,
                                               FunctionAnalysisManager &AM) {
  bool Changed = lowerRegions(F, AM.getResult<LoopAnalysis>(F),
                              AM.getResult<ScalarEvolutionAnalysis>(F),
                              AM.getResult<DominatorTreeAnalysis>(F),
                              AM.getResult<PostDominatorTreeAnalysis>(F),
                              AM.getResult<AAManager>(F),
                              AM.getResult<MemorySSAAnalysis>(F).getMSSA());
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

PreservedAnalyses
LinxV5ElementRegionVerifierPass::run(Function &F, FunctionAnalysisManager &) {
  verifyNoResidualElementContracts(F);
  return PreservedAnalyses::all();
}
