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
constexpr StringLiteral S32M32View =
    "pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32";

struct ElementProfile {
  Type *ScalarTy = nullptr;
  FixedVectorType *VectorTy = nullptr;
  uint64_t DataType = 25;
  bool isSigned() const { return DataType == 17; }
};

static Optional<ElementProfile> integerProfile(uint64_t DataType,
                                                LLVMContext &Context) {
  if (DataType != 25 && DataType != 17)
    return None;
  Type *Scalar = Type::getInt32Ty(Context);
  return ElementProfile{Scalar, FixedVectorType::get(Scalar, 32), DataType};
}

constexpr StringLiteral RegionLoopMD = "llvm.loop.linx.pto.element.region";
constexpr StringLiteral ScalarizedLaneZeroMD =
    "linx.pto.element.scalarized_lane_zero";

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
    ElementProfile Profile;
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
    Optional<ElementProfile> Profile =
        Text == U32M32View ? integerProfile(25, F.getContext())
                          : Text == S32M32View ? integerProfile(17, F.getContext())
                                              : None;
    if (!Profile)
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
                      StorageID, NextViewID++, *Profile, {}};
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
    if (!Plan.Accesses.empty()) {
      auto *Alloca = cast<AllocaInst>(Storage);
      auto *Count = dyn_cast<ConstantInt>(Alloca->getArraySize());
      if (!Count) {
        // The frontend's Tile storage is always a constant-sized alloca.
        // Reject anything else rather than weakening the logical view range.
        diagnose(F, Annotation,
                 "logical view storage requires a constant object size");
      }
      APInt ObjectSize = Count->getValue().zextOrTrunc(128) *
                         APInt(128, DL.getTypeAllocSize(
                                        Alloca->getAllocatedType()));
      if (ObjectSize.getActiveBits() > 64)
        diagnose(F, Annotation,
                 "logical view storage size exceeds the address model");
      uint64_t ObjectBytes = ObjectSize.getZExtValue();
      if (Plan.ByteOffset < 0 ||
          static_cast<uint64_t>(Plan.ByteOffset) > ObjectBytes ||
          ObjectBytes - static_cast<uint64_t>(Plan.ByteOffset) < 128)
        diagnose(F, Annotation,
                 "logical view requires a proved 128-byte backing range");
      uint64_t Offset = static_cast<uint64_t>(Plan.ByteOffset);
      uint64_t ViewAlignment =
          commonAlignment(Alloca->getAlign(), Offset).value();
      if (ViewAlignment < 32)
        diagnose(F, Annotation,
                 "logical view requires proved 32-byte alignment");
    }
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
      auto *ScalarLoad = dyn_cast<LoadInst>(Access);
      bool IsScalarizedLaneZeroLoad =
          Plan.Profile.DataType == 25 && ScalarLoad && CarrierType->isIntegerTy(32) &&
          !ScalarLoad->isVolatile() && !ScalarLoad->isAtomic();
      if (!IsScalarizedLaneZeroLoad &&
          CarrierType != Plan.Profile.VectorTy) {
        std::string Detail;
        raw_string_ostream OS(Detail);
        CarrierType->print(OS);
        OS << " in ";
        Access->print(OS);
        diagnose(F, Access,
                 Twine("view access carrier is ") + OS.str() +
                     "; expected exact <32 x i32>");
      }
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
      Args.push_back(ConstantInt::get(I64, Plan.Profile.DataType));
      Args.push_back(ConstantInt::get(I64, 32));
      Args.push_back(ConstantInt::get(I64, 1));
      Args.push_back(ConstantInt::get(I64, 29));
      return Args;
    };

    for (Instruction *Access : Plan.Accesses) {
      if (auto *Load = dyn_cast<LoadInst>(Access)) {
        if (Load->getType()->isIntegerTy(32)) {
          IRBuilder<> LoadBuilder(Load->getNextNode());
          auto *VectorTy =
              FixedVectorType::get(Type::getInt32Ty(F.getContext()), 32);
          Function *TCI = Intrinsic::getDeclaration(
              F.getParent(), Intrinsic::linx_experimental_ew_tci, {VectorTy});
          Value *Scalar = LoadBuilder.CreateZExt(
              Load, LoadBuilder.getInt64Ty(), "pto.element.lane.zero.scalar");
          Value *Carrier = LoadBuilder.CreateCall(
              TCI,
              {LoadBuilder.getInt64(32), LoadBuilder.getInt64(1),
               LoadBuilder.getInt64(25), LoadBuilder.getInt64(29), Scalar,
               LoadBuilder.getInt64(0)},
              "pto.element.scalarized.carrier");
          CallInst *Marker = LoadBuilder.CreateCall(
              View, BuildArgs(Carrier), "pto.element.view");
          Marker->setMetadata(ScalarizedLaneZeroMD,
                              MDNode::get(F.getContext(), {}));
          Value *LaneZero = LoadBuilder.CreateExtractElement(
              Marker, LoadBuilder.getInt32(0), "pto.element.lane.zero");
          Load->replaceUsesWithIf(LaneZero, [&](Use &U) {
            return U.getUser() != Scalar;
          });
          Changed = true;
          continue;
        }
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
  bool ScalarizedLaneZero = false;
  ElementProfile Profile;
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
  auto Profile = integerProfile(Fields[4], V->getContext());
  if (!Profile || Fields[3] != 128 || Fields[5] != 32 ||
      Fields[6] != 1 || Fields[7] != 29 ||
      V->getType() != Profile->VectorTy)
    return None;
  return ViewDescriptor{Call, static_cast<uint64_t>(Fields[0]),
                        static_cast<uint64_t>(Fields[1]), Fields[2],
                        static_cast<uint64_t>(Fields[3]),
                        Call->getMetadata(ScalarizedLaneZeroMD) != nullptr,
                        *Profile};
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
  Value *PublishedElement = nullptr;
  Value *PublicationIndex = nullptr;
  CallInst *OutputView = nullptr;
  PHINode *OutputPhi = nullptr;
  StoreInst *OutputStore = nullptr;
  SmallVector<CallInst *, 4> ExternalOwnedViews;
  SmallVector<StoreInst *, 2> ExternalStores;
  uint64_t OutputStorageID = 0;
  int64_t OutputOffset = 0;
  uint64_t OutputRange = 0;
  ElementProfile Profile;
  bool IsZeroElseGather = false;
  CallInst *GatherIndexView = nullptr;
  Value *GatherBase = nullptr;
  Value *GatherValid = nullptr;
  Value *GatherOuterPredicate = nullptr;
  LoadInst *GatherLoad = nullptr;
  bool IsRelaxedAtomicAdd = false;
  AtomicRMWInst *AtomicAdd = nullptr;
  CallInst *AtomicIndexView = nullptr;
  CallInst *AtomicKeyView = nullptr;
  Value *AtomicLane = nullptr;
  Value *AtomicPublicationIndex = nullptr;
  InsertElementInst *AtomicPublicationInsert = nullptr;
  SmallVector<Instruction *, 8> AtomicPublicationScaffold;
  LoadInst *AtomicScalarizedLaneZeroLoad = nullptr;
  Value *AtomicScalarizedLaneZeroValue = nullptr;
  Value *AtomicScalarizedLaneZeroCarrier = nullptr;
  Value *AtomicBase = nullptr;
  Value *AtomicValid = nullptr;
  Value *AtomicSelected = nullptr;
  SmallVector<Value *, 2> AtomicOuterPredicates;
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

static bool sameIntegerValue(Value *LHS, Value *RHS, ScalarEvolution &SE) {
  auto *LC = dyn_cast<ConstantInt>(LHS);
  auto *RC = dyn_cast<ConstantInt>(RHS);
  if (LC && RC) {
    unsigned Width = std::max(LC->getBitWidth(), RC->getBitWidth());
    return LC->getValue().zextOrTrunc(Width) ==
           RC->getValue().zextOrTrunc(Width);
  }
  return LHS->getType() == RHS->getType() && SE.getSCEV(LHS) == SE.getSCEV(RHS);
}

static bool isInvariantAtPreheader(Value *V, Loop &L, DominatorTree &DT) {
  if (!L.isLoopInvariant(V))
    return false;
  auto *I = dyn_cast<Instruction>(V);
  return !I || DT.dominates(I, L.getLoopPreheader()->getTerminator());
}

static bool collectAtomicPredicate(Function &F, Value *Condition,
                                   RegionPlan &Plan, ScalarEvolution &SE,
                                   DominatorTree &DT) {
  if (auto *Select = dyn_cast<SelectInst>(Condition)) {
    if (!Select->getType()->isIntegerTy(1) ||
        !match(Select->getFalseValue(), m_Zero()) ||
        !isInvariantAtPreheader(Select->getCondition(), *Plan.L, DT))
      return diagnose(F, Select,
                      "atomic active condition is not a proved conjunction");
    Plan.AtomicOuterPredicates.push_back(Select->getCondition());
    return collectAtomicPredicate(F, Select->getTrueValue(), Plan, SE, DT);
  }
  if (auto *And = dyn_cast<BinaryOperator>(Condition)) {
    if (And->getOpcode() != Instruction::And ||
        !And->getType()->isIntegerTy(1))
      return diagnose(F, And,
                      "atomic active condition is not a proved conjunction");
    return collectAtomicPredicate(F, And->getOperand(0), Plan, SE, DT) &&
           collectAtomicPredicate(F, And->getOperand(1), Plan, SE, DT);
  }
  if (Condition->getType()->isIntegerTy(1) &&
      isInvariantAtPreheader(Condition, *Plan.L, DT)) {
    Plan.AtomicOuterPredicates.push_back(Condition);
    return true;
  }
  auto *Compare = dyn_cast<ICmpInst>(Condition);
  if (!Compare)
    return diagnose(F, dyn_cast<Instruction>(Condition),
                    "atomic active condition is not an integer comparison");

  if (Compare->getPredicate() == ICmpInst::ICMP_ULT &&
      sameElementIndex(Compare->getOperand(0), Plan.IV, SE) &&
      Compare->getOperand(1)->getType()->isIntegerTy(32) &&
      isInvariantAtPreheader(Compare->getOperand(1), *Plan.L, DT)) {
    if (Plan.AtomicValid)
      return diagnose(F, Compare, "atomic region has multiple tail bounds");
    Plan.AtomicValid = Compare->getOperand(1);
    return true;
  }

  if (Compare->getPredicate() == ICmpInst::ICMP_EQ) {
    Value *Element = Compare->getOperand(0);
    Value *Selected = Compare->getOperand(1);
    auto *Extract = dyn_cast<ExtractElementInst>(Element);
    if (!Extract) {
      std::swap(Element, Selected);
      Extract = dyn_cast<ExtractElementInst>(Element);
    }
    auto KeyView =
        Extract ? getViewDescriptor(Extract->getVectorOperand()) : None;
    if (Extract && KeyView && KeyView->Profile.DataType == 25 &&
        sameElementIndex(Extract->getIndexOperand(), Plan.IV, SE) &&
        Selected->getType()->isIntegerTy(32) &&
        isInvariantAtPreheader(Selected, *Plan.L, DT)) {
      if (Plan.AtomicKeyView || Plan.AtomicSelected)
        return diagnose(F, Compare,
                        "atomic region has multiple element predicates");
      Plan.AtomicKeyView = KeyView->Marker;
      Plan.AtomicSelected = Selected;
      return true;
    }

    Value *Lane = Compare->getOperand(0);
    Value *SelectedLane = Compare->getOperand(1);
    if (!sameElementIndex(Lane, Plan.IV, SE))
      std::swap(Lane, SelectedLane);
    if (sameElementIndex(Lane, Plan.IV, SE) &&
        SelectedLane->getType()->isIntegerTy(32) &&
        isInvariantAtPreheader(SelectedLane, *Plan.L, DT)) {
      if (Plan.AtomicLane)
        return diagnose(F, Compare,
                        "atomic region has multiple lane predicates");
      Plan.AtomicLane = SelectedLane;
      return true;
    }
  }

  return diagnose(F, Compare,
                  "unsupported atomic predicate; expected element < valid, "
                  "key[element] == selected, or an invariant boolean");
}

static bool planRelaxedAtomicAdd(
                                 Function &F, BasicBlock *MergeBB,
                                 ArrayRef<std::pair<Value *, BasicBlock *>> Results,
                                 ArrayRef<AtomicRMWInst *> Atomics,
                                 RegionPlan &Plan, ScalarEvolution &SE,
                                 DominatorTree &DT, AAResults &AA,
                                 MemorySSA &MSSA) {
  if (Atomics.empty())
    return true;
  if (Atomics.size() != 1)
    return diagnose(F, Atomics.front(),
                    "element region requires exactly one atomic effect");

  AtomicRMWInst *Atomic = Atomics.front();
  if (Atomic->getOperation() != AtomicRMWInst::Add || Atomic->isVolatile() ||
      Atomic->getOrdering() != AtomicOrdering::Monotonic ||
      Atomic->getSyncScopeID() != SyncScope::System ||
      !Atomic->getType()->isIntegerTy(32) ||
      !match(Atomic->getValOperand(), m_SpecificInt(1)))
    return diagnose(F, Atomic,
                    "requires non-volatile system-scope monotonic atomic add "
                    "of exact i32 value one");

  bool HasAtomicIncoming = false;
  for (const auto &Result : Results) {
    Value *Incoming = Result.first;
    if (Incoming == Atomic) {
      HasAtomicIncoming = true;
      if (Result.second != Atomic->getParent())
        return diagnose(F, Atomic,
                        "atomic result does not arrive from its effect block");
      continue;
    }
    if (!match(Incoming, m_Zero()))
      return diagnose(F, Atomic,
                      "inactive atomic lanes must publish exact zero");
  }
  bool HasUnrelatedAtomicUse = false;
  for (User *U : Atomic->users()) {
    if (auto *Phi = dyn_cast<PHINode>(U))
      if (Phi->getParent() == MergeBB)
        continue;
    if (U == Plan.AtomicPublicationInsert)
      continue;
    HasUnrelatedAtomicUse = true;
    break;
  }
  if (!HasAtomicIncoming || HasUnrelatedAtomicUse)
    return diagnose(F, Atomic,
                    "atomic old value must feed only the element result merge");

  auto *GEP = dyn_cast<GetElementPtrInst>(Atomic->getPointerOperand());
  Value *ElementIndex = GEP && GEP->getNumIndices() == 1
                            ? GEP->idx_begin()->get()
                            : nullptr;
  auto *Wide = dyn_cast_or_null<ZExtInst>(ElementIndex);
  if (!Wide || !Wide->getSrcTy()->isIntegerTy(32) ||
      !Wide->getDestTy()->isIntegerTy(64))
    return diagnose(F, Atomic,
                    "atomic address requires exact zext i32 element index");
  auto *Extract = dyn_cast<ExtractElementInst>(Wide->getOperand(0));
  auto IndexView =
      Extract ? getViewDescriptor(Extract->getVectorOperand()) : None;
  if (!GEP || !GEP->getSourceElementType()->isIntegerTy(32) || !Extract ||
      !IndexView || !Extract->getIndexOperand()->getType()->isIntegerTy(32))
    return diagnose(F, Atomic,
                    "atomic address is not an exact U32 view indexed by the "
                    "region element");
  Value *Base = GEP->getPointerOperand();
  if (!isInvariantAtPreheader(Base, *Plan.L, DT))
    return diagnose(F, Atomic,
                    "atomic base must be loop invariant and dominate the region");

  BasicBlock *EffectBB = Atomic->getParent();
  auto *EffectTerm = dyn_cast<BranchInst>(EffectBB->getTerminator());
  if (!EffectTerm || EffectTerm->isConditional() ||
      EffectTerm->getSuccessor(0) != MergeBB)
    return diagnose(F, Atomic,
                    "atomic effect block must branch directly to its result merge");

  BasicBlock *Active = EffectBB;
  auto IsInactiveBridge = [&](BasicBlock *BB) {
    if (BB == MergeBB)
      return true;
    auto *Term = dyn_cast<BranchInst>(BB->getTerminator());
    if (!Term || Term->isConditional() || Term->getSuccessor(0) != MergeBB)
      return false;
    for (Instruction &I : *BB) {
      if (&I == Term || isa<DbgInfoIntrinsic>(I))
        continue;
      if (isIntrinsic(dyn_cast<CallInst>(&I),
                      Intrinsic::linx_experimental_element_view) &&
          I.use_empty())
        continue;
      if (is_contained(Plan.AtomicPublicationScaffold, &I))
        continue;
      if (!isSafeToSpeculativelyExecute(&I) || !I.use_empty())
        return false;
    }
    return true;
  };
  while (Active != Plan.L->getHeader()) {
    BasicBlock *PredicateBB = Active->getSinglePredecessor();
    auto *Branch = PredicateBB
                       ? dyn_cast<BranchInst>(PredicateBB->getTerminator())
                       : nullptr;
    if (!Branch || !Branch->isConditional() ||
        Branch->getSuccessor(0) != Active ||
        !IsInactiveBridge(Branch->getSuccessor(1)))
      return diagnose(F, Atomic,
                      "atomic active path is not a canonical CFG conjunction");
    if (!collectAtomicPredicate(F, Branch->getCondition(), Plan, SE, DT))
      return false;
    Active = PredicateBB;
  }
  if (!Plan.AtomicValid && !Plan.AtomicLane)
    return diagnose(F, Atomic,
                    "atomic region requires a proved tail or exact lane mask");
  if (!sameElementIndex(Extract->getIndexOperand(), Plan.IV, SE) &&
      (!Plan.AtomicLane ||
       !sameIntegerValue(Extract->getIndexOperand(), Plan.AtomicLane, SE)))
    return diagnose(F, Extract,
                    "atomic index view is not selected by the current element");
  if (IndexView->ScalarizedLaneZero &&
      (!Plan.AtomicLane ||
       !match(Plan.AtomicLane, m_SpecificInt(0)) ||
       !match(Extract->getIndexOperand(), m_SpecificInt(0))))
    return diagnose(F, Extract,
                    "scalarized lane-zero view requires an exact active lane-zero proof");
  if (IndexView->ScalarizedLaneZero) {
    auto *TCI = dyn_cast<CallInst>(IndexView->Marker->getArgOperand(0));
    auto *WideScalar = TCI &&
                               isIntrinsic(TCI,
                                           Intrinsic::linx_experimental_ew_tci)
                           ? dyn_cast<ZExtInst>(TCI->getArgOperand(4))
                           : nullptr;
    Value *ScalarSource = WideScalar ? WideScalar->getOperand(0) : nullptr;
    auto *ScalarLoad = dyn_cast_or_null<LoadInst>(ScalarSource);
    if (ScalarLoad) {
      Value *Pointer = ScalarLoad->getPointerOperand();
      bool PointerReady = Plan.L->isLoopInvariant(Pointer);
      if (auto *PointerI = dyn_cast<Instruction>(Pointer))
        PointerReady &= DT.dominates(
            PointerI, Plan.L->getLoopPreheader()->getTerminator());
      MemoryAccess *Access = MSSA.getMemoryAccess(ScalarLoad);
      MemoryAccess *Clobber =
          Access ? MSSA.getWalker()->getClobberingMemoryAccess(Access) : nullptr;
      if (ScalarLoad->isVolatile() || ScalarLoad->isAtomic() ||
          !ScalarLoad->getType()->isIntegerTy(32) ||
          !Plan.L->contains(ScalarLoad) || !PointerReady ||
          !isa<AllocaInst>(getUnderlyingObject(Pointer)) || !Access ||
          (Clobber && Plan.L->contains(Clobber->getBlock())) ||
          AA.alias(MemoryLocation::get(ScalarLoad),
                   MemoryLocation::get(Atomic)) != AliasResult::NoAlias)
        return diagnose(F, ScalarLoad,
                        "scalarized lane-zero load is not safe to hoist");
      Plan.AtomicScalarizedLaneZeroLoad = ScalarLoad;
    } else if (auto *ScalarExtract =
                   dyn_cast_or_null<ExtractElementInst>(ScalarSource)) {
      if (!match(ScalarExtract->getIndexOperand(), m_SpecificInt(0)) ||
          !isInvariantAtPreheader(ScalarExtract->getVectorOperand(), *Plan.L,
                                  DT))
        return diagnose(F, ScalarExtract,
                        "scalarized lane-zero value is not available at the region preheader");
      Plan.AtomicScalarizedLaneZeroCarrier =
          ScalarExtract->getVectorOperand();
    } else if (!ScalarSource || !ScalarSource->getType()->isIntegerTy(32) ||
               !isInvariantAtPreheader(ScalarSource, *Plan.L, DT)) {
      return diagnose(F, IndexView->Marker,
                      "scalarized lane-zero view does not preserve a safe i32 value");
    }
    Plan.AtomicScalarizedLaneZeroValue = ScalarSource;
  }
  if (Plan.AtomicPublicationIndex &&
      !sameElementIndex(Plan.AtomicPublicationIndex, Plan.IV, SE) &&
      (!Plan.AtomicLane ||
       !sameIntegerValue(Plan.AtomicPublicationIndex, Plan.AtomicLane, SE)))
    return diagnose(F, Plan.AtomicPublicationInsert,
                    "constant atomic publication index does not match the "
                    "proved active lane");

  Plan.IsRelaxedAtomicAdd = true;
  Plan.AtomicAdd = Atomic;
  Plan.AtomicIndexView = IndexView->Marker;
  Plan.AtomicBase = Base;
  return true;
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
    if (Descriptor->Profile.DataType != Plan.Profile.DataType)
      return Fail();
    if (Descriptor->ScalarizedLaneZero &&
        (!Plan.IsRelaxedAtomicAdd ||
         Descriptor->Marker != Plan.AtomicIndexView))
      return Fail();
    if (V->getType() !=
        FixedVectorType::get(Type::getInt32Ty(V->getContext()), 32))
      return Fail();
    if (Descriptor->ScalarizedLaneZero) {
      if (!Plan.AtomicScalarizedLaneZeroValue)
        return Fail();
      Inputs.push_back(*Descriptor);
      return true;
    }
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
      break;
    case Instruction::UDiv:
    case Instruction::URem:
      break;
    case Instruction::SDiv:
    case Instruction::SRem:
    case Instruction::AShr:
      break;
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor:
    case Instruction::Shl:
    case Instruction::LShr:
      break;
    default:
      return Fail();
    }
    return validateTileExpression(Binary->getOperand(0), Plan, SE, DT,
                                  Visited, Inputs, Rejected) &&
           validateTileExpression(Binary->getOperand(1), Plan, SE, DT,
                                  Visited, Inputs, Rejected);
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
  SmallVector<AtomicRMWInst *, 2> AtomicAdds;
  for (BasicBlock *BB : L.blocks()) {
    for (Instruction &I : *BB) {
      if (isa<InvokeInst>(I) || isa<CallBrInst>(I))
        return diagnose(F, &I,
                        "exceptional and indirect call terminators are "
                        "unsupported in an element region");
      if (I.isTerminator() || isa<PHINode>(I) || isa<DbgInfoIntrinsic>(I))
        continue;
      if (auto *Atomic = dyn_cast<AtomicRMWInst>(&I)) {
        AtomicAdds.push_back(Atomic);
        if (!MSSA.getMemoryAccess(Atomic))
          return diagnose(F, Atomic,
                          "atomic effect lacks MemorySSA coverage");
        continue;
      }
      bool IsVolatileOrAtomic = false;
      if (auto *Load = dyn_cast<LoadInst>(&I))
        IsVolatileOrAtomic = Load->isVolatile() || Load->isAtomic();
      else if (auto *Store = dyn_cast<StoreInst>(&I))
        IsVolatileOrAtomic = Store->isVolatile() || Store->isAtomic();
      else
        IsVolatileOrAtomic = isa<AtomicCmpXchgInst>(I) || isa<FenceInst>(I);
      if (IsVolatileOrAtomic)
        return diagnose(F, &I, "volatile and atomic effects are not in P1a");
      if (auto *Call = dyn_cast<CallInst>(&I)) {
        bool IsScalarizedCarrier =
            isIntrinsic(Call, Intrinsic::linx_experimental_ew_tci) &&
            llvm::all_of(Call->users(), [](User *U) {
              auto Descriptor = getViewDescriptor(cast<Value>(U));
              return Descriptor && Descriptor->ScalarizedLaneZero;
            });
        if (!isIntrinsic(Call, Intrinsic::linx_experimental_element_view) &&
            !IsScalarizedCarrier)
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
          if (!DescriptorAccess) {
            for (User *U : Load->users()) {
              auto *Cast = dyn_cast<ZExtInst>(U);
              if (!Cast)
                continue;
              for (User *CastUser : Cast->users()) {
                auto *TCI = dyn_cast<CallInst>(CastUser);
                if (!isIntrinsic(TCI, Intrinsic::linx_experimental_ew_tci))
                  continue;
                for (User *TCIUser : TCI->users()) {
                  auto Descriptor = getViewDescriptor(cast<Value>(TCIUser));
                  if (Descriptor && Descriptor->ScalarizedLaneZero) {
                    DescriptorAccess = true;
                    break;
                  }
                }
              }
            }
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
  PHINode *MergedCarrierPhi = nullptr;
  SmallVector<std::pair<Value *, BasicBlock *>, 4> AtomicResults;
  if (!AtomicAdds.empty()) {
    SmallVector<CallInst *, 2> MergedPublications;
    for (BasicBlock *BB : L.blocks()) {
      for (Instruction &I : *BB) {
        auto *Call = dyn_cast<CallInst>(&I);
        if (!isIntrinsic(Call, Intrinsic::linx_experimental_element_view))
          continue;
        auto *CarrierPhi = dyn_cast<PHINode>(Call->getArgOperand(0));
        if (!CarrierPhi || CarrierPhi->getParent() != Call->getParent())
          continue;
        bool AllInserted = llvm::all_of(
            CarrierPhi->incoming_values(),
            [](Value *V) { return isa<InsertElementInst>(V); });
        if (AllInserted)
          MergedPublications.push_back(Call);
      }
    }
    if (MergedPublications.size() == 1) {
      CallInst *MergedView = MergedPublications.front();
      auto *CarrierPhi = cast<PHINode>(MergedView->getArgOperand(0));
      Value *CommonCarrier = nullptr;
      for (unsigned I = 0; I != CarrierPhi->getNumIncomingValues(); ++I) {
        Value *Incoming = CarrierPhi->getIncomingValue(I);
        auto *Insert = cast<InsertElementInst>(Incoming);
        auto BaseView = getViewDescriptor(Insert->getOperand(0));
        if (!BaseView)
          return diagnose(F, Insert,
                          "merged publication lacks exact input view metadata");
        Value *Carrier = BaseView->Marker->getArgOperand(0);
        if (CommonCarrier && Carrier != CommonCarrier)
          return diagnose(F, Insert,
                          "merged publication has different carrier bases");
        CommonCarrier = Carrier;
        Plan.AtomicPublicationScaffold.push_back(BaseView->Marker);
        Plan.AtomicPublicationScaffold.push_back(Insert);
        if (!sameElementIndex(Insert->getOperand(2), Plan.IV, SE) &&
            !isa<ConstantInt>(Insert->getOperand(2)))
          return diagnose(F, Insert,
                          "merged publication index is not the current or a "
                          "constant proved element");
        Value *Element = Insert->getOperand(1);
        if (Element != AtomicAdds.front() && !match(Element, m_Zero()))
          return diagnose(F, Insert,
                          "merged atomic publication has a nonzero alternate");
        if (Element == AtomicAdds.front()) {
          if (Plan.AtomicPublicationInsert)
            return diagnose(F, Insert,
                            "merged publication contains multiple atomic results");
          Plan.AtomicPublicationInsert = Insert;
          Plan.AtomicPublicationIndex = Insert->getOperand(2);
        } else if (!sameElementIndex(Insert->getOperand(2), Plan.IV, SE)) {
          return diagnose(F, Insert,
                          "inactive merged publication must use the current element");
        }
        AtomicResults.push_back(
            {Element, CarrierPhi->getIncomingBlock(I)});
      }
      MergedCarrierPhi = CarrierPhi;
      OutputViews.clear();
      OutputViews.push_back(MergedView);
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
  Plan.Publication =
      dyn_cast<InsertElementInst>(Plan.OutputView->getArgOperand(0));
  Plan.Profile = Descriptor->Profile;
  if ((!AtomicAdds.empty() || !ScalarLoads.empty()) &&
      Plan.Profile.DataType != 25)
    return diagnose(F, Plan.OutputView,
                    "gather and atomic regions currently require U32 output views");
  Plan.OutputStorageID = Descriptor->StorageID;
  Plan.OutputOffset = Descriptor->Offset;
  Plan.OutputRange = Descriptor->Range;
  if (Plan.Publication) {
    Plan.PublishedElement = Plan.Publication->getOperand(1);
    Plan.PublicationIndex = Plan.Publication->getOperand(2);
    if (!sameElementIndex(Plan.PublicationIndex, Plan.IV, SE))
      return diagnose(F, Plan.Publication,
                      "publication index is not the region induction element");
  } else if (MergedCarrierPhi) {
    Plan.PublicationIndex = Plan.IV;
  } else {
    return diagnose(F, Plan.OutputView,
                    "publication is not a proved element merge");
  }
  if (!DT.dominates(Plan.OutputView, L.getLoopLatch()->getTerminator()))
    return diagnose(F, Plan.OutputView,
                    "publication must execute on every region iteration");

  if (MergedCarrierPhi) {
    if (!planRelaxedAtomicAdd(F, MergedCarrierPhi->getParent(), AtomicResults,
                              AtomicAdds, Plan, SE, DT, AA, MSSA))
      return false;
    Plan.PublishedElement = Plan.AtomicAdd;
  } else if (auto *Merge = dyn_cast<PHINode>(Plan.PublishedElement)) {
    SmallVector<std::pair<Value *, BasicBlock *>, 4> Results;
    for (unsigned I = 0; I != Merge->getNumIncomingValues(); ++I)
      Results.push_back(
          {Merge->getIncomingValue(I), Merge->getIncomingBlock(I)});
    if (!planRelaxedAtomicAdd(F, Merge->getParent(), Results, AtomicAdds, Plan,
                              SE, DT, AA, MSSA))
      return false;
    if (Merge->getNumIncomingValues() == 2 &&
        Merge->getParent() == Plan.Publication->getParent() &&
        AtomicAdds.empty()) {
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
          IndexView->Profile.DataType == 25 &&
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
  if (!AtomicAdds.empty() && !Plan.IsRelaxedAtomicAdd)
    return diagnose(F, AtomicAdds.front(),
                    "atomic effect is not the published element result");
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
  if (Plan.IsRelaxedAtomicAdd && Plan.OutputStore &&
      AA.alias(MemoryLocation::get(Plan.AtomicAdd),
               MemoryLocation::get(Plan.OutputStore)) != AliasResult::NoAlias)
    return diagnose(F, Plan.AtomicAdd,
                    "atomic target may alias its Tile publication storage");
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
  } else if (Plan.IsRelaxedAtomicAdd) {
    ExpressionValid = validateTileExpression(
        Plan.AtomicIndexView, Plan, SE, DT, Visited, Inputs, Rejected);
    if (ExpressionValid && Plan.AtomicKeyView)
      ExpressionValid = validateTileExpression(
          Plan.AtomicKeyView, Plan, SE, DT, Visited, Inputs, Rejected);
  } else {
    ExpressionValid = validateTileExpression(
        Plan.PublishedElement, Plan, SE, DT, Visited, Inputs, Rejected);
  }
  if (!ExpressionValid) {
    std::string Detail;
    raw_string_ostream OS(Detail);
    if (Rejected) {
      Rejected->printAsOperand(OS, /*PrintType=*/true);
      if (auto *RejectedI = dyn_cast<Instruction>(Rejected))
        OS << " (" << RejectedI->getOpcodeName() << ")";
    }
    return diagnose(F, Plan.Publication ? cast<Instruction>(Plan.Publication)
                                        : cast<Instruction>(Plan.OutputView),
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
      V = Plan.Profile.isSigned()
              ? Builder.CreateSExtOrTrunc(V, Builder.getInt64Ty())
              : Builder.CreateZExtOrTrunc(V, Builder.getInt64Ty());
    return Builder.CreateCall(TCI,
                              {Builder.getInt64(32), Builder.getInt64(1),
                               Builder.getInt64(Plan.Profile.DataType), Builder.getInt64(29), V,
                               Builder.getInt64(0)},
                              "pto.element.splat");
  }

public:
  TileExpressionBuilder(Function &F, RegionPlan &Plan, ScalarEvolution &SE,
                        DominatorTree &DT)
      : Plan(Plan), SE(SE), DT(DT),
        Builder(Plan.L->getLoopPreheader()->getTerminator()) {
    Type *VectorTy = Plan.Profile.VectorTy;
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
                                   Builder.getInt64(Plan.Profile.DataType), Builder.getInt64(29),
                                   Builder.getInt64(0),
                                   Builder.getInt64(1ULL << 32)},
                                  "pto.element.index");
    } else if (auto *C = dyn_cast<ConstantInt>(V)) {
      Result = splat(C);
    } else if (auto Descriptor = getViewDescriptor(V)) {
      Value *Carrier = Descriptor->Marker->getArgOperand(0);
      if (auto *Load = dyn_cast<LoadInst>(Carrier)) {
        IRBuilder<> LoadBuilder(Load);
        Result = LoadBuilder.CreateCall(
            TLoad,
            {LoadBuilder.getInt64(1), LoadBuilder.getInt64(32),
             LoadBuilder.getInt64(1), LoadBuilder.getInt64(Plan.Profile.DataType),
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
      case Instruction::SDiv:
        Opcode = 3;
        break;
      case Instruction::URem:
      case Instruction::SRem:
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
      case Instruction::AShr:
        Opcode = 9;
        break;
      default:
        return nullptr;
      }
      Value *LHS = lower(Binary->getOperand(0));
      Value *RHS = lower(Binary->getOperand(1));
      if (!LHS || !RHS)
        return nullptr;
      uint64_t OperationDataType = Plan.Profile.DataType;
      switch (Binary->getOpcode()) {
      case Instruction::SDiv:
      case Instruction::SRem:
      case Instruction::AShr:
        OperationDataType = 17;
        break;
      case Instruction::UDiv:
      case Instruction::URem:
      case Instruction::LShr:
        OperationDataType = 25;
        break;
      default:
        break;
      }
      // LLVM may legally change ashr to lshr when later uses discard sign
      // bits. The IR opcode owns arithmetic signedness; the view owns storage.
      auto EmitBinary = [&](unsigned Operation, Value *Left, Value *Right) {
        return Builder.CreateCall(
            TBinary,
            {Builder.getInt64(32), Builder.getInt64(1),
             Builder.getInt64(OperationDataType), Builder.getInt64(29),
             Builder.getInt64(Operation), Left, Right},
            "pto.element.value");
      };
      if (Binary->getOpcode() == Instruction::SRem) {
        // C++/LLVM srem truncates toward zero; PTO TREM has divisor-sign
        // floor modulo. Preserve source remainder through the truncating TDIV.
        Value *Quotient = EmitBinary(3, LHS, RHS);
        Value *Product = EmitBinary(2, Quotient, RHS);
        Result = EmitBinary(1, LHS, Product);
      } else {
        Result = EmitBinary(Opcode, LHS, RHS);
      }
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
    } else if (Plan.IsRelaxedAtomicAdd) {
      IRBuilder<> Builder(Plan.L->getLoopPreheader()->getTerminator());
      Type *ValueTy = FixedVectorType::get(Builder.getInt32Ty(), 32);
      Type *OffsetTy = FixedVectorType::get(Builder.getInt64Ty(), 32);
      Function *TCI = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tci, {ValueTy});
      Value *Indices = nullptr;
      if (Plan.AtomicScalarizedLaneZeroValue) {
        if (Plan.AtomicScalarizedLaneZeroCarrier) {
          Indices = Plan.AtomicScalarizedLaneZeroCarrier;
        } else {
          Value *ScalarValue = Plan.AtomicScalarizedLaneZeroValue;
          if (Plan.AtomicScalarizedLaneZeroLoad)
            ScalarValue = Builder.Insert(
                Plan.AtomicScalarizedLaneZeroLoad->clone(),
                "pto.atomic.lane.zero.load");
          Value *Scalar =
              Builder.CreateZExt(ScalarValue, Builder.getInt64Ty());
          Indices = Builder.CreateCall(
              TCI,
              {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
               Builder.getInt64(29), Scalar, Builder.getInt64(0)},
              "pto.atomic.lane.zero.indices");
        }
      } else {
        Indices = Expressions.lower(Plan.AtomicIndexView);
      }
      Value *Lanes = Builder.CreateCall(
          TCI,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Builder.getInt64(0),
           Builder.getInt64(1ULL << 32)},
          "pto.atomic.lanes");
      Function *Compare = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tcmps_gpr,
          {ValueTy});
      Value *Mask = nullptr;
      if (Plan.AtomicValid) {
        Value *Valid = Plan.AtomicValid;
        if (!Valid->getType()->isIntegerTy(64))
          Valid = Builder.CreateZExtOrTrunc(Valid, Builder.getInt64Ty());
        Mask = Builder.CreateCall(
            Compare,
            {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
             Builder.getInt64(29), Lanes, Valid, Builder.getInt64(2)},
            "pto.atomic.tail.mask");
      }
      if (Plan.AtomicLane) {
        Value *Lane = Plan.AtomicLane;
        if (!Lane->getType()->isIntegerTy(64))
          Lane = Builder.CreateZExtOrTrunc(Lane, Builder.getInt64Ty());
        Value *LaneMask = Builder.CreateCall(
            Compare,
            {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
             Builder.getInt64(29), Lanes, Lane, Builder.getInt64(0)},
            "pto.atomic.lane.mask");
        Mask = Mask ? Builder.CreateAnd(Mask, LaneMask,
                                        "pto.atomic.active.mask")
                    : LaneMask;
      }
      if (Plan.AtomicKeyView) {
        Value *Keys = Expressions.lower(Plan.AtomicKeyView);
        Value *Selected = Plan.AtomicSelected;
        if (!Selected->getType()->isIntegerTy(64))
          Selected =
              Builder.CreateZExtOrTrunc(Selected, Builder.getInt64Ty());
        Value *KeyMask = Builder.CreateCall(
            Compare,
            {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
             Builder.getInt64(29), Keys, Selected, Builder.getInt64(0)},
            "pto.atomic.key.mask");
        Mask = Mask ? Builder.CreateAnd(Mask, KeyMask,
                                        "pto.atomic.active.mask")
                    : KeyMask;
      }
      for (Value *Predicate : Plan.AtomicOuterPredicates)
        Mask = Builder.CreateSelect(Predicate, Mask, Builder.getInt64(0),
                                    "pto.atomic.outer.mask");
      Function *TLEA = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_tlea,
          {OffsetTy, ValueTy});
      Value *Offsets = Builder.CreateCall(
          TLEA,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Indices, Builder.getInt64(32)},
          "pto.atomic.byte.offsets");
      Value *Ones = Builder.CreateCall(
          TCI,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(29), Builder.getInt64(1), Builder.getInt64(0)},
          "pto.atomic.ones");
      Function *Atomic = Intrinsic::getDeclaration(
          F.getParent(), Intrinsic::linx_experimental_ew_mgather_add_masked,
          {ValueTy, OffsetTy, ValueTy});
      Result = Builder.CreateCall(
          Atomic,
          {Builder.getInt64(32), Builder.getInt64(1), Builder.getInt64(25),
           Builder.getInt64(3), Builder.getInt64(29), Plan.AtomicBase, Offsets,
           Ones, Mask, Builder.getInt64(0), Builder.getInt64(0),
           Builder.getInt64(1)},
          "pto.atomic.old");
    } else {
      Result = Expressions.lower(Plan.PublishedElement);
    }
    if (!Result) {
      diagnose(F, Plan.Publication ? cast<Instruction>(Plan.Publication)
                                  : cast<Instruction>(Plan.OutputView),
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
           StoreBuilder.getInt64(1), StoreBuilder.getInt64(Plan.Profile.DataType),
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
           Builder.getInt64(Plan.Profile.DataType), Builder.getInt64(24),
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
