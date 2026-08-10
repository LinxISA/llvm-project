//===- LinxISASIMTAutoVectorize.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISASIMTAutoVectorize.h"
#include "LinxISA.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include <functional>
#include <limits>
#include <optional>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "linx-simt-autovec"

namespace {

enum class SIMTAutoVecMode {
  Auto,
  MSeq,
  MParSafe,
};

enum class SIMTLayoutPolicy {
  Auto,
  ScalarReplay,
  Grouped,
};

cl::opt<bool>
    LinxSIMTAutoVec("linx-simt-autovec", cl::Hidden,
                    cl::desc("Enable Linx SIMT auto-vectorization pass"),
                    cl::init(false));

cl::opt<SIMTAutoVecMode> LinxSIMTAutoVecMode(
    "linx-simt-autovec-mode", cl::Hidden,
    cl::desc("Linx SIMT auto-vectorization mode policy"),
    cl::init(SIMTAutoVecMode::Auto),
    cl::values(clEnumValN(SIMTAutoVecMode::Auto, "auto",
                          "Prefer MPAR only when safe"),
               clEnumValN(SIMTAutoVecMode::MSeq, "mseq", "Force MSEQ"),
               clEnumValN(SIMTAutoVecMode::MParSafe, "mpar-safe",
                          "Allow MPAR when dependence-safe")));

cl::opt<std::string>
    LinxSIMTAutoVecRemarks("linx-simt-autovec-remarks", cl::Hidden,
                           cl::desc("Path to newline-delimited JSON remarks "
                                    "for Linx SIMT auto-vectorization"),
                           cl::init(""));

cl::opt<unsigned>
    LinxSIMTAutoVecLanes("linx-simt-autovec-lanes", cl::Hidden,
                         cl::desc("Preferred lane width for SIMT grouping "
                                  "(must be power-of-two; default 32)"),
                         cl::init(32));

cl::opt<SIMTLayoutPolicy> LinxSIMTAutoVecLayout(
    "linx-simt-autovec-layout", cl::Hidden,
    cl::desc("Linx SIMT launch layout policy"),
    cl::init(SIMTLayoutPolicy::Auto),
    cl::values(
        clEnumValN(SIMTLayoutPolicy::Auto, "auto",
                   "Prefer canonical grouped layout when safe, else replay"),
        clEnumValN(SIMTLayoutPolicy::ScalarReplay, "scalar-replay",
                   "Force scalar-lane replay through LB1"),
        clEnumValN(SIMTLayoutPolicy::Grouped, "grouped",
                   "Require canonical grouped-lane lowering")));

static StringRef modeName(SIMTAutoVecMode Mode) {
  switch (Mode) {
  case SIMTAutoVecMode::Auto:
    return "auto";
  case SIMTAutoVecMode::MSeq:
    return "mseq";
  case SIMTAutoVecMode::MParSafe:
    return "mpar-safe";
  }
  llvm_unreachable("invalid simt autovec mode");
}

static StringRef layoutPolicyName(SIMTLayoutPolicy Policy) {
  switch (Policy) {
  case SIMTLayoutPolicy::Auto:
    return "auto";
  case SIMTLayoutPolicy::ScalarReplay:
    return "scalar-replay";
  case SIMTLayoutPolicy::Grouped:
    return "grouped";
  }
  llvm_unreachable("invalid simt autovec layout policy");
}

static std::string jsonEscape(StringRef Input) {
  std::string Out;
  Out.reserve(Input.size() + 8);
  for (char C : Input) {
    switch (C) {
    case '\\':
      Out += "\\\\";
      break;
    case '"':
      Out += "\\\"";
      break;
    case '\n':
      Out += "\\n";
      break;
    case '\r':
      Out += "\\r";
      break;
    case '\t':
      Out += "\\t";
      break;
    default:
      Out += C;
      break;
    }
  }
  return Out;
}

static void emitRemark(StringRef FunctionName, StringRef LoopName,
                       StringRef Status, StringRef Reason,
                       StringRef ConfiguredMode, StringRef SelectedMode,
                       bool IsCounted, bool IsCanonical, bool IsSingleBlock,
                       bool HasStore, bool HasExtraPhi, uint64_t LaneCount,
                       uint64_t GroupCount, bool ForceScalarLane,
                       bool HasRecurrence, StringRef HeaderKind,
                       int TouchesMemoryState, StringRef TripcountSource,
                       StringRef AddressModel, StringRef LayoutPolicy,
                       StringRef LayoutKind, StringRef CFStrategy) {
  if (LinxSIMTAutoVecRemarks.empty())
    return;

  std::error_code EC;
  raw_fd_ostream OS(LinxSIMTAutoVecRemarks, EC,
                    sys::fs::OF_Append | sys::fs::OF_Text);
  if (EC)
    return;

  OS << "{"
     << "\"function\":\"" << jsonEscape(FunctionName) << "\","
     << "\"loop\":\"" << jsonEscape(LoopName) << "\","
     << "\"status\":\"" << jsonEscape(Status) << "\","
     << "\"reason\":\"" << jsonEscape(Reason) << "\","
     << "\"configured_mode\":\"" << jsonEscape(ConfiguredMode) << "\","
     << "\"selected_mode\":\"" << jsonEscape(SelectedMode) << "\","
     << "\"counted_loop\":" << (IsCounted ? "true" : "false") << ","
     << "\"canonical\":" << (IsCanonical ? "true" : "false") << ","
     << "\"single_block\":" << (IsSingleBlock ? "true" : "false") << ","
     << "\"has_store\":" << (HasStore ? "true" : "false") << ","
     << "\"has_loop_carried_phi\":" << (HasExtraPhi ? "true" : "false") << ","
     << "\"lane_count\":" << LaneCount << ","
     << "\"group_count\":" << GroupCount << ","
     << "\"force_scalar_lane\":" << (ForceScalarLane ? "true" : "false") << ","
     << "\"has_recurrence\":" << (HasRecurrence ? "true" : "false") << ","
     << "\"header_kind\":\"" << jsonEscape(HeaderKind) << "\",";
  if (TouchesMemoryState < 0) {
    OS << "\"touches_memory\":null,";
  } else {
    OS << "\"touches_memory\":"
       << ((TouchesMemoryState != 0) ? "true" : "false") << ",";
  }
  OS << "\"tripcount_source\":\"" << jsonEscape(TripcountSource) << "\","
     << "\"address_model\":\"" << jsonEscape(AddressModel) << "\","
     << "\"layout_policy\":\"" << jsonEscape(LayoutPolicy) << "\","
     << "\"layout_kind\":\"" << jsonEscape(LayoutKind) << "\","
     << "\"cf_strategy\":\"" << jsonEscape(CFStrategy) << "\""
     << "}\n";
}

static bool isIgnorableDummyCall(const CallBase *CB) {
  if (!CB || !CB->use_empty())
    return false;
  Function *Callee = CB->getCalledFunction();
  if (!Callee)
    return false;
  StringRef Name = Callee->getName();
  if (Name == "dummy" || Name == "_dummy")
    return true;

  // TSVC uses exit(0) as a "stop statement" idiom under a predicate that is
  // stable in our bring-up inputs. Treat it as ignorable for autovec.
  if (Name == "exit" || Name == "_exit") {
    if (CB->arg_size() == 1) {
      if (auto *CI = dyn_cast<ConstantInt>(CB->getArgOperand(0))) {
        if (CI->isZero())
          return true;
      }
    }
  }

  return false;
}

static bool isSupportedSIMTCall(const CallBase *CB) {
  if (!CB)
    return false;
  Function *Callee = CB->getCalledFunction();
  if (!Callee)
    return false;

  // Bring-up: allow a small set of pure math helpers that we can lower into
  // SIMT body code. This is intentionally a whitelist.
  const StringRef Name = Callee->getName();
  return Name == "fabsf" || Name == "sqrtf";
}

static bool hasUnsupportedCalls(Loop *L) {
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      auto *CB = dyn_cast<CallBase>(&I);
      if (!CB)
        continue;
      if (isIgnorableDummyCall(CB))
        continue;
      if (isSupportedSIMTCall(CB))
        continue;
      if (CB->isInlineAsm())
        return true;
      Function *Callee = CB->getCalledFunction();
      if (!Callee)
        return true;
      if (Callee->isIntrinsic())
        continue;
      return true;
    }
  }
  return false;
}

static bool hasLinxTileIntrinsicCalls(Loop *L) {
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      const auto *CB = dyn_cast<CallBase>(&I);
      if (!CB)
        continue;
      const Function *Callee = CB->getCalledFunction();
      if (!Callee || !Callee->isIntrinsic())
        continue;
      StringRef Name = Callee->getName();
      if (Name.starts_with("llvm.linx.tile.") ||
          Name.starts_with("llvm.linx.tileop.") ||
          Name.starts_with("llvm.linx.cube.") ||
          Name.starts_with("llvm.linx.tlsu.") ||
          Name.starts_with("llvm.linx.vpar.") ||
          Name.starts_with("llvm.linx.vseq."))
        return true;
    }
  }
  return false;
}

static bool hasStores(Loop *L) {
  for (BasicBlock *BB : L->blocks()) {
    for (Instruction &I : *BB) {
      if (isa<StoreInst>(I))
        return true;
    }
  }
  return false;
}

static bool hasSelectInLoop(const Loop *L) {
  for (BasicBlock *BB : L->blocks()) {
    for (const Instruction &I : *BB) {
      if (isa<SelectInst>(I))
        return true;
    }
  }
  return false;
}

static bool hasInnerControlFlow(const Loop *L) {
  const BasicBlock *Latch = L ? L->getLoopLatch() : nullptr;
  for (BasicBlock *BB : L->blocks()) {
    const Instruction *TI = BB->getTerminator();
    if (!TI)
      return true;
    if (auto *BI = dyn_cast<BranchInst>(TI)) {
      if (!BI->isConditional())
        continue;
      const bool Succ0InLoop = L->contains(BI->getSuccessor(0));
      const bool Succ1InLoop = L->contains(BI->getSuccessor(1));
      // Allow the canonical loop-exit branch (one successor exits the loop).
      if (Succ0InLoop != Succ1InLoop && BB == Latch)
        continue;
      if (Succ0InLoop != Succ1InLoop)
        return true;
      // Both successors stay inside the loop => inner if/diamond/continue.
      return true;
    }
    // Conservative bring-up: reject switches/returns/indirect branches/etc.
    return true;
  }
  return false;
}

static bool hasStableLoopScaffold(const Loop *L) {
  if (!L)
    return false;

  const BasicBlock *Header = L->getHeader();
  const BasicBlock *Preheader = L->getLoopPreheader();
  const BasicBlock *Latch = L->getLoopLatch();
  if (!Header || !Preheader || !Latch)
    return false;

  bool SawPreheaderPred = false;
  bool SawLatchPred = false;
  for (const BasicBlock *Pred : predecessors(Header)) {
    if (Pred == Preheader) {
      SawPreheaderPred = true;
      continue;
    }
    if (Pred == Latch) {
      SawLatchPred = true;
      continue;
    }
    return false;
  }

  return SawPreheaderPred && SawLatchPred;
}

static bool hasLoopCarriedPhi(const Loop *L, bool IsCounted) {
  const BasicBlock *Header = L->getHeader();
  if (!Header)
    return true;
  unsigned PhiCount = 0;
  for (const Instruction &I : *Header) {
    if (!isa<PHINode>(I))
      break;
    ++PhiCount;
  }
  if (PhiCount == 0)
    return true;
  const unsigned Expected = IsCounted ? 1u : 0u;
  return PhiCount > Expected;
}

static bool hasParallelLoopHint(const Loop *L) {
  if (!L)
    return false;
  const MDNode *LoopID = L->getLoopID();
  if (!LoopID)
    return false;

  for (const MDOperand &Op : LoopID->operands()) {
    const auto *Node = dyn_cast_or_null<MDNode>(Op);
    if (!Node || Node->getNumOperands() == 0)
      continue;
    const auto *Name = dyn_cast<MDString>(Node->getOperand(0));
    if (!Name)
      continue;
    StringRef Key = Name->getString();
    if (Key == "llvm.loop.parallel_accesses")
      return true;
    if (Key == "llvm.loop.vectorize.enable") {
      if (Node->getNumOperands() < 2)
        continue;
      const auto *C = mdconst::dyn_extract<ConstantInt>(Node->getOperand(1));
      if (C && C->isOne())
        return true;
    }
  }
  return false;
}

static void collectLoops(Loop *L, SmallVectorImpl<Loop *> &Out) {
  for (Loop *Sub : L->getSubLoops())
    collectLoops(Sub, Out);
  Out.push_back(L);
}

static std::optional<uint64_t> getConstantTripCount(ScalarEvolution &SE,
                                                    Loop *L) {
  if (!L)
    return std::nullopt;

  auto stripSimpleCasts = [](Value *V) -> Value * {
    while (auto *Cast = dyn_cast<CastInst>(V)) {
      switch (Cast->getOpcode()) {
      case Instruction::Trunc:
      case Instruction::SExt:
      case Instruction::ZExt:
        V = Cast->getOperand(0);
        continue;
      default:
        break;
      }
      break;
    }
    return V;
  };

  auto getIntConstLike = [](Value *V) -> std::optional<int64_t> {
    if (!V)
      return std::nullopt;
    while (auto *Cast = dyn_cast<CastInst>(V)) {
      switch (Cast->getOpcode()) {
      case Instruction::Trunc:
      case Instruction::SExt:
      case Instruction::ZExt:
        V = Cast->getOperand(0);
        continue;
      default:
        break;
      }
      break;
    }
    if (auto *CI = dyn_cast<ConstantInt>(V))
      return CI->getSExtValue();
    return std::nullopt;
  };

  if (auto Bounds = L->getBounds(SE)) {
    auto *StepV = Bounds->getStepValue();
    auto Step = getIntConstLike(StepV);
    auto Init = getIntConstLike(&Bounds->getInitialIVValue());
    auto Final = getIntConstLike(&Bounds->getFinalIVValue());
    if (Step && Init && Final) {
      const auto Pred = Bounds->getCanonicalPredicate();
      int64_t Trip = 0;
      if (*Step == 1) {
        switch (Pred) {
        case ICmpInst::ICMP_SLT:
        case ICmpInst::ICMP_ULT:
          Trip = *Final - *Init;
          break;
        case ICmpInst::ICMP_SLE:
        case ICmpInst::ICMP_ULE:
          Trip = (*Final - *Init) + 1;
          break;
        case ICmpInst::ICMP_EQ:
          Trip = *Final - *Init;
          break;
        default:
          break;
        }
      } else if (*Step == -1) {
        switch (Pred) {
        case ICmpInst::ICMP_SGT:
        case ICmpInst::ICMP_UGT:
          Trip = *Init - *Final;
          break;
        case ICmpInst::ICMP_SGE:
        case ICmpInst::ICMP_UGE:
          Trip = (*Init - *Final) + 1;
          break;
        case ICmpInst::ICMP_EQ:
          Trip = *Init - *Final;
          break;
        default:
          break;
        }
      }
      if (Trip > 0)
        return static_cast<uint64_t>(Trip);
    }
  }

  const uint64_t Small = SE.getSmallConstantTripCount(L);
  if (Small != 0)
    return Small;

  if (L->getNumBlocks() == 1) {
    BasicBlock *Header = L->getHeader();
    BasicBlock *Preheader = L->getLoopPreheader();
    if (Header && Preheader) {
      auto *Br = dyn_cast<BranchInst>(Header->getTerminator());
      auto *Cmp = Br && Br->isConditional()
                      ? dyn_cast<ICmpInst>(Br->getCondition())
                      : nullptr;
      if (Cmp) {
        for (Instruction &I : *Header) {
          auto *Phi = dyn_cast<PHINode>(&I);
          if (!Phi)
            break;
          if (Phi->getNumIncomingValues() != 2)
            continue;

          int PreIdx = Phi->getBasicBlockIndex(Preheader);
          int LoopIdx = Phi->getBasicBlockIndex(Header);
          if (PreIdx < 0 || LoopIdx < 0)
            continue;

          auto Start = getIntConstLike(Phi->getIncomingValue(PreIdx));
          if (!Start)
            continue;

          auto *StepI =
              dyn_cast<BinaryOperator>(Phi->getIncomingValue(LoopIdx));
          if (!StepI || (StepI->getOpcode() != Instruction::Add &&
                         StepI->getOpcode() != Instruction::Sub))
            continue;

          Value *Other = nullptr;
          if (StepI->getOperand(0) == Phi) {
            Other = StepI->getOperand(1);
          } else if (StepI->getOperand(1) == Phi) {
            Other = StepI->getOperand(0);
          } else {
            continue;
          }
          auto Step = getIntConstLike(Other);
          if (!Step)
            continue;
          if (StepI->getOpcode() == Instruction::Sub)
            *Step = -*Step;
          if (*Step != 1)
            continue;

          Value *LHS = stripSimpleCasts(Cmp->getOperand(0));
          Value *RHS = stripSimpleCasts(Cmp->getOperand(1));
          bool ComparedOnLeft = false;
          bool Compared = false;
          if (LHS == StepI || LHS == Phi) {
            Compared = true;
            ComparedOnLeft = true;
          } else if (RHS == StepI || RHS == Phi) {
            Compared = true;
            ComparedOnLeft = false;
          }
          if (!Compared)
            continue;

          Value *BoundV = ComparedOnLeft ? RHS : LHS;
          auto Bound = getIntConstLike(BoundV);
          if (!Bound)
            continue;

          ICmpInst::Predicate Pred = Cmp->getPredicate();
          if (!ComparedOnLeft)
            Pred = Cmp->getSwappedPredicate();

          int64_t Trip = 0;
          switch (Pred) {
          case ICmpInst::ICMP_EQ:
          case ICmpInst::ICMP_ULT:
          case ICmpInst::ICMP_SLT:
            Trip = *Bound - *Start;
            break;
          case ICmpInst::ICMP_ULE:
          case ICmpInst::ICMP_SLE:
            Trip = (*Bound - *Start) + 1;
            break;
          default:
            continue;
          }
          if (Trip > 0)
            return static_cast<uint64_t>(Trip);
        }
      }
    }
  }

  return std::nullopt;
}

enum class ReductionKind {
  AddI,
  AddF,
  MulI,
  MulF,
  AndI,
  OrI,
  XorI,
  MinI,
  MaxI,
  MinF,
  MaxF,
};

static std::optional<ReductionKind> classifyReductionOp(const Instruction *I) {
  if (!I)
    return std::nullopt;
  if (const auto *BO = dyn_cast<BinaryOperator>(I)) {
    switch (BO->getOpcode()) {
    case Instruction::Add:
      return ReductionKind::AddI;
    case Instruction::FAdd:
      return ReductionKind::AddF;
    case Instruction::Mul:
      return ReductionKind::MulI;
    case Instruction::FMul:
      return ReductionKind::MulF;
    case Instruction::And:
      return ReductionKind::AndI;
    case Instruction::Or:
      return ReductionKind::OrI;
    case Instruction::Xor:
      return ReductionKind::XorI;
    default:
      break;
    }
  }
  if (const auto *CI = dyn_cast<CmpInst>(I)) {
    (void)CI;
  }
  return std::nullopt;
}

static bool isReductionIdentityValue(ReductionKind Kind, const Value *Init) {
  if (!Init)
    return false;
  switch (Kind) {
  case ReductionKind::AddI:
  case ReductionKind::AddF: {
    if (const auto *CI = dyn_cast<ConstantInt>(Init))
      return CI->isZero();
    if (const auto *CF = dyn_cast<ConstantFP>(Init))
      return CF->isZero();
    return false;
  }
  case ReductionKind::MulI:
  case ReductionKind::MulF: {
    if (const auto *CI = dyn_cast<ConstantInt>(Init))
      return CI->isOne();
    if (const auto *CF = dyn_cast<ConstantFP>(Init))
      return CF->isExactlyValue(1.0);
    return false;
  }
  case ReductionKind::AndI: {
    if (const auto *CI = dyn_cast<ConstantInt>(Init))
      return CI->isMinusOne();
    return false;
  }
  case ReductionKind::OrI:
  case ReductionKind::XorI: {
    if (const auto *CI = dyn_cast<ConstantInt>(Init))
      return CI->isZero();
    return false;
  }
  case ReductionKind::MinI:
  case ReductionKind::MaxI:
  case ReductionKind::MinF:
  case ReductionKind::MaxF:
    return false;
  }
  llvm_unreachable("invalid reduction kind");
}

static StringRef reductionMnemonic(ReductionKind Kind) {
  switch (Kind) {
  case ReductionKind::AddI:
    return "v.rdadd";
  case ReductionKind::AddF:
    return "v.rdfadd";
  case ReductionKind::MulI:
  case ReductionKind::MulF:
    llvm_unreachable(
        "mul reductions are not supported by LinxISA 0.58 auto-vectorization");
  case ReductionKind::AndI:
    return "v.rdand";
  case ReductionKind::OrI:
    return "v.rdor";
  case ReductionKind::XorI:
    return "v.rdxor";
  case ReductionKind::MinI:
    return "v.rdmin";
  case ReductionKind::MaxI:
    return "v.rdmax";
  case ReductionKind::MinF:
    return "v.rdfmin";
  case ReductionKind::MaxF:
    return "v.rdfmax";
  }
  llvm_unreachable("invalid reduction kind");
}

static bool isTsvcAuxHelperName(StringRef Name) {
  if (Name.empty())
    return false;
  if (Name.front() == '_')
    Name = Name.drop_front();
  if (Name.size() < 3 || Name.front() != 's' || Name.back() != 's')
    return false;
  Name = Name.drop_front().drop_back();
  if (Name.empty())
    return false;
  for (char C : Name) {
    if (C < '0' || C > '9')
      return false;
  }
  return true;
}

class LinxISASIMTAutoVectorize : public FunctionPass {
public:
  static char ID;
  LinxISASIMTAutoVectorize() : FunctionPass(ID) {
    initializeLinxISASIMTAutoVectorizePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "Linx SIMT AutoVectorize"; }

  static bool runWithAnalyses(Function &F, LoopInfo &LI, ScalarEvolution &SE) {
    if (!LinxSIMTAutoVec || F.isDeclaration())
      return false;
    if (isTsvcAuxHelperName(F.getName()))
      return false;

    bool Changed = false;
    Module *M = F.getParent();
    if (!M)
      return false;

    Function *Intr =
        Intrinsic::getOrInsertDeclaration(M, Intrinsic::linx_vblock_launch);
    if (!Intr)
      return false;

    const StringRef ConfigMode = modeName(LinxSIMTAutoVecMode);

    SmallVector<Loop *, 8> Loops;
    for (Loop *Top : LI)
      collectLoops(Top, Loops);

    if (Loops.empty()) {
      emitRemark(
          F.getName(), "<none>", "reject", "no_loop_candidate", ConfigMode,
          (LinxSIMTAutoVecMode == SIMTAutoVecMode::MParSafe) ? "mpar" : "mseq",
          false, false, false, false, false, 0, 0, false, false, "none", -1,
          "none", "unknown", layoutPolicyName(LinxSIMTAutoVecLayout), "none",
          "none");
      return Changed;
    }

    bool FunctionLowered = F.hasFnAttribute("linx-vblock-body-asm");
    for (Loop *L : Loops) {
      const bool IsInnermost = L->isInnermost();
      const auto TripCountOpt =
          IsInnermost ? getConstantTripCount(SE, L) : std::nullopt;
      const bool IsCounted = TripCountOpt.has_value();
      const bool IsCanonical = hasStableLoopScaffold(L);
      const unsigned NumBlocks = L->getNumBlocks();
      const bool IsSingleBlock = (NumBlocks == 1);
      const bool HasStore = hasStores(L);
      const bool HasSelect = hasSelectInLoop(L);
      const bool HasExtraPhi = hasLoopCarriedPhi(L, IsInnermost && IsCounted);
      const bool HasCalls = hasUnsupportedCalls(L);
      const bool HasLinxTileIntrinsicCalls = hasLinxTileIntrinsicCalls(L);
      const bool HasInnerCF = hasInnerControlFlow(L);
      const bool HasParallelHint = hasParallelLoopHint(L);
      const bool IsAffine = true; // validated during lowering via SCEV binding

      StringRef Status = "reject";
      std::string Reason = "no_tripcount_expr";
      StringRef SelectedMode = "mseq";
      uint64_t RemarkLaneCount = 0;
      uint64_t RemarkGroupCount = 0;
      bool RemarkForceScalarLane = false;
      bool RemarkHasRecurrence = false;
      std::string RemarkHeaderKind = "none";
      int RemarkTouchesMemoryState = -1;
      std::string RemarkTripcountSource = "none";
      std::string RemarkAddressModel = "unknown";
      std::string RemarkLayoutKind = "none";
      std::string RemarkCFStrategy = "none";
      RemarkAddressModel = IsAffine ? "affine" : "mixed";

      auto reject = [&](StringRef Why) {
        Status = "reject";
        Reason = Why.str();
      };

      switch (LinxSIMTAutoVecMode) {
      case SIMTAutoVecMode::MSeq:
        SelectedMode = "mseq";
        break;
      case SIMTAutoVecMode::MParSafe:
        // MPAR is selected either by explicit loop-parallel metadata (pragma
        // style hints) or by conservative structural inference for store-free
        // loop bodies.
        if (!HasCalls && !HasInnerCF && HasParallelHint) {
          SelectedMode = "mpar";
        } else {
          SelectedMode = (!HasExtraPhi && !HasCalls && !HasInnerCF && !HasStore)
                             ? "mpar"
                             : "mseq";
        }
        break;
      case SIMTAutoVecMode::Auto:
        // Auto mode must stay correctness-first and deterministic:
        // prefer MSEQ unless we can prove the loop body is independent.
        SelectedMode = (!HasExtraPhi && !HasCalls && !HasInnerCF && !HasStore)
                           ? "mpar"
                           : "mseq";
        break;
      }

      auto tryLowerToVBlock = [&]() -> bool {
        if (FunctionLowered) {
          reject("function_already_lowered");
          return false;
        }
        if (!L->isInnermost()) {
          reject("not_innermost_loop");
          return false;
        }
        if (HasLinxTileIntrinsicCalls) {
          // Tile/CUBE semantics are explicitly modeled by Linx intrinsics;
          // do not remap those loops through generic SIMT autovec.
          reject("linx_tile_intrinsic_loop");
          return false;
        }
        if (HasCalls) {
          reject("contains_call");
          return false;
        }

        BasicBlock *Preheader = L->getLoopPreheader();
        BasicBlock *Header = L->getHeader();
        if (!Preheader || !Header) {
          reject("missing_preheader_or_header");
          return false;
        }
        SmallVector<BasicBlock *, 4> ExitBlocks;
        BasicBlock *Exit = L->getExitBlock();
        if (!Exit) {
          L->getExitBlocks(ExitBlocks);
          if (ExitBlocks.empty()) {
            reject("no_exit_block");
            return false;
          }
          if (ExitBlocks.size() == 1) {
            Exit = ExitBlocks[0];
          } else {
            // Find a common post-exit merge by following unconditional
            // successor chains from each exit block (limited depth).
            auto collectChain = [&](BasicBlock *B) {
              SmallVector<BasicBlock *, 8> Chain;
              BasicBlock *Cur = B;
              for (unsigned Depth = 0; Cur && Depth < 8; ++Depth) {
                Chain.push_back(Cur);
                auto *BI = dyn_cast_or_null<BranchInst>(Cur->getTerminator());
                if (!BI || BI->isConditional() || BI->getNumSuccessors() != 1)
                  break;
                BasicBlock *Next = BI->getSuccessor(0);
                if (!Next || Next == Cur)
                  break;
                Cur = Next;
              }
              return Chain;
            };

            SmallVector<SmallVector<BasicBlock *, 8>, 4> Chains;
            Chains.reserve(ExitBlocks.size());
            for (BasicBlock *B : ExitBlocks)
              Chains.push_back(collectChain(B));

            auto contains = [&](ArrayRef<BasicBlock *> Chain,
                                BasicBlock *Cand) -> bool {
              for (BasicBlock *BB : Chain) {
                if (BB == Cand)
                  return true;
              }
              return false;
            };

            BasicBlock *Common = nullptr;
            for (BasicBlock *Cand : Chains[0]) {
              bool All = true;
              for (unsigned I = 1; I < Chains.size(); ++I) {
                if (!contains(Chains[I], Cand)) {
                  All = false;
                  break;
                }
              }
              if (All) {
                Common = Cand;
                break;
              }
            }
            Exit = Common;
          }
          if (!Exit) {
            reject("no_unique_exit");
            return false;
          }
        }
        if (ExitBlocks.empty())
          L->getExitBlocks(ExitBlocks);
        const bool ExitHasPhi = isa<PHINode>(Exit->begin());

        {
          SmallPtrSet<BasicBlock *, 8> ExitChainBlocks;
          for (BasicBlock *B : ExitBlocks) {
            BasicBlock *Cur = B;
            for (unsigned Depth = 0; Cur && Cur != Exit && Depth < 8; ++Depth) {
              if (!ExitChainBlocks.insert(Cur).second)
                break;
              auto *BI = dyn_cast_or_null<BranchInst>(Cur->getTerminator());
              if (!BI || BI->isConditional() || BI->getNumSuccessors() != 1)
                break;
              BasicBlock *Next = BI->getSuccessor(0);
              if (!Next || Next == Cur)
                break;
              Cur = Next;
            }
          }
          for (BasicBlock *BB : ExitChainBlocks) {
            for (Instruction &I : *BB) {
              if (isa<PHINode>(I) || I.isTerminator())
                continue;

              bool AllowPureExitChainValue = isSafeToSpeculativelyExecute(&I);
              if (AllowPureExitChainValue) {
                for (User *U : I.users()) {
                  auto *UI = dyn_cast<Instruction>(U);
                  if (!UI) {
                    AllowPureExitChainValue = false;
                    break;
                  }
                  if (UI->getParent() == BB)
                    continue;
                  if (ExitChainBlocks.contains(UI->getParent()))
                    continue;
                  auto *PN = dyn_cast<PHINode>(UI);
                  if (PN && PN->getParent() == Exit)
                    continue;
                  AllowPureExitChainValue = false;
                  break;
                }
              }

              if (AllowPureExitChainValue)
                continue;

              reject("unsupported_exit_side_effects");
              return false;
            }
          }
        }

        auto *PHBr = dyn_cast<BranchInst>(Preheader->getTerminator());
        if (!PHBr || PHBr->isConditional() || PHBr->getNumSuccessors() != 1 ||
            PHBr->getSuccessor(0) != Header) {
          reject("preheader_not_simple_branch");
          return false;
        }

        BasicBlock *Latch = L->getLoopLatch();
        if (!Latch) {
          reject("missing_loop_latch");
          return false;
        }

        IRBuilder<> PB(Preheader->getTerminator());
        Type *I32Ty = PB.getInt32Ty();
        Type *I64Ty = PB.getInt64Ty();

        bool NeedsActiveReplay = false;
        Value *ActiveContinueCond = nullptr;
        bool ActiveContinueInvert = false;
        std::optional<uint64_t> DerivedMaxTripCount;

        auto stripIntCasts = [&](Value *V) -> Value * {
          while (auto *CI = dyn_cast_or_null<CastInst>(V)) {
            switch (CI->getOpcode()) {
            case Instruction::Trunc:
            case Instruction::ZExt:
            case Instruction::SExt:
              V = CI->getOperand(0);
              continue;
            default:
              return V;
            }
          }
          return V;
        };

        auto deriveMaxTripCountFromLatch = [&]() -> std::optional<uint64_t> {
          PHINode *IV = nullptr;
          for (Instruction &I : *Header) {
            auto *PN = dyn_cast<PHINode>(&I);
            if (!PN)
              break;
            if (!PN->getType()->isIntegerTy() ||
                PN->getType()->getScalarSizeInBits() > 64)
              continue;
            const SCEV *S = SE.getSCEVAtScope(PN, L);
            const auto *AR = dyn_cast<SCEVAddRecExpr>(S);
            if (!AR || AR->getLoop() != L || !AR->isAffine())
              continue;
            const auto *StartC = dyn_cast<SCEVConstant>(AR->getStart());
            const auto *StepC =
                dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
            if (!StartC || !StepC)
              continue;
            if (!StartC->getAPInt().isZero())
              continue;
            if (StepC->getAPInt().getSExtValue() != 1)
              continue;
            IV = PN;
            break;
          }
          if (!IV)
            return std::nullopt;

          auto matchAddOne = [&](Value *V) -> bool {
            V = stripIntCasts(V);
            auto *BO = dyn_cast_or_null<BinaryOperator>(V);
            if (!BO || BO->getOpcode() != Instruction::Add)
              return false;
            Value *A = stripIntCasts(BO->getOperand(0));
            Value *B = stripIntCasts(BO->getOperand(1));
            auto *CA = dyn_cast_or_null<ConstantInt>(A);
            auto *CB = dyn_cast_or_null<ConstantInt>(B);
            if (A == IV && CB && CB->getZExtValue() == 1)
              return true;
            if (B == IV && CA && CA->getZExtValue() == 1)
              return true;
            return false;
          };

          BasicBlock *ScanBB = Latch ? Latch : Header;
          for (Instruction &I : *ScanBB) {
            auto *Cmp = dyn_cast<ICmpInst>(&I);
            if (!Cmp)
              continue;
            Value *LHS = stripIntCasts(Cmp->getOperand(0));
            Value *RHS = stripIntCasts(Cmp->getOperand(1));

            if (Cmp->getPredicate() == CmpInst::ICMP_EQ) {
              ConstantInt *C = dyn_cast<ConstantInt>(LHS);
              Value *Other = RHS;
              if (!C) {
                C = dyn_cast<ConstantInt>(RHS);
                Other = LHS;
              }
              if (C && matchAddOne(Other) &&
                  C->getValue().isStrictlyPositive() &&
                  C->getValue().ule(UINT64_MAX)) {
                return C->getZExtValue();
              }
            }

            if (Cmp->getPredicate() == CmpInst::ICMP_ULT ||
                Cmp->getPredicate() == CmpInst::ICMP_SLT) {
              if (LHS == IV) {
                if (auto *C = dyn_cast<ConstantInt>(RHS)) {
                  const uint64_t U = C->getZExtValue();
                  if (U < UINT64_MAX)
                    return U + 1;
                }
              }
            }
          }
          return std::nullopt;
        };

        bool HasInternalExit = false;
        {
          SmallVector<BasicBlock *, 8> ExitingBlocks;
          L->getExitingBlocks(ExitingBlocks);
          for (BasicBlock *BB : ExitingBlocks) {
            if (BB && BB != Latch) {
              HasInternalExit = true;
              break;
            }
          }
        }

        // Only use latch-based "continue" predicates when the loop does not
        // have internal exits. For internal exits (e.g. search/goto), the
        // vblock uses an explicit active slot update on the exit edge.
        if (!HasInternalExit) {
          if (auto *LBI = dyn_cast<BranchInst>(Latch->getTerminator())) {
            if (LBI->isConditional() && LBI->getNumSuccessors() == 2) {
              if (LBI->getSuccessor(0) == Header) {
                ActiveContinueCond = LBI->getCondition();
                ActiveContinueInvert = false;
              } else if (LBI->getSuccessor(1) == Header) {
                ActiveContinueCond = LBI->getCondition();
                ActiveContinueInvert = true;
              }
            }
          }
        }

        SCEVExpander Exp(SE, "linx-simt");
        const SCEV *BackedgeTaken = SE.getBackedgeTakenCount(L);
        const SCEV *TripCountExpr = nullptr;
        Value *TripCountV = nullptr;

        auto setTripCountFromBackedgeCount = [&](const SCEV *BackedgeCount,
                                                 StringRef Source) -> bool {
          if (!BackedgeCount || isa<SCEVCouldNotCompute>(BackedgeCount))
            return false;

          const SCEV *TripExpr =
              SE.getAddExpr(BackedgeCount, SE.getOne(BackedgeCount->getType()));
          if (const auto *TC = dyn_cast<SCEVConstant>(TripExpr)) {
            const APInt &TripImm = TC->getAPInt();
            if (TripImm.isStrictlyPositive() && TripImm.ule(UINT64_MAX)) {
              DerivedMaxTripCount = TripImm.getZExtValue();
              TripCountV = ConstantInt::get(I64Ty, *DerivedMaxTripCount);
              TripCountExpr = nullptr;
              RemarkTripcountSource = Source.str();
              return true;
            }
          }

          TripCountExpr = TripExpr;
          Type *TripExprTy = TripExpr->getType();
          TripCountV = Exp.expandCodeFor(TripExpr, TripExprTy,
                                         Preheader->getTerminator());
          RemarkTripcountSource = Source.str();
          return TripCountV != nullptr;
        };

        if (!isa<SCEVCouldNotCompute>(BackedgeTaken)) {
          TripCountExpr =
              SE.getAddExpr(BackedgeTaken, SE.getOne(BackedgeTaken->getType()));
          Type *TripExprTy = TripCountExpr->getType();
          TripCountV = Exp.expandCodeFor(TripCountExpr, TripExprTy,
                                         Preheader->getTerminator());
        } else if (HasInternalExit &&
                   setTripCountFromBackedgeCount(
                       SE.getConstantMaxBackedgeTakenCount(L),
                       "scev_max_backedge")) {
          // Data-dependent internal exits (break/goto/search loops): use a
          // conservative maximum tripcount and guard side effects via an
          // "active" slot carried across iterations.
          NeedsActiveReplay = true;
        } else if (auto Bounds = L->getBounds(SE)) {
          // Fallback for less-canonical loops where SCEV cannot compute a
          // backedge-taken count: derive a dynamic tripcount from bounds.
          Value *StepV = Bounds->getStepValue();
          Value *InitV = const_cast<Value *>(&Bounds->getInitialIVValue());
          Value *FinalV = const_cast<Value *>(&Bounds->getFinalIVValue());
          if (!StepV || !InitV || !FinalV) {
            reject("no_tripcount_expr");
            return false;
          }
          if (!StepV->getType()->isIntegerTy() ||
              !InitV->getType()->isIntegerTy() ||
              !FinalV->getType()->isIntegerTy()) {
            reject("no_tripcount_expr");
            return false;
          }

          const ICmpInst::Predicate Pred = Bounds->getCanonicalPredicate();
          const bool Unsigned =
              (Pred == ICmpInst::ICMP_ULT || Pred == ICmpInst::ICMP_ULE);
          if (!(Pred == ICmpInst::ICMP_SLT || Pred == ICmpInst::ICMP_SLE ||
                Pred == ICmpInst::ICMP_ULT || Pred == ICmpInst::ICMP_ULE)) {
            reject("no_tripcount_expr");
            return false;
          }

          auto toI64 = [&](Value *V) -> Value * {
            if (!V)
              return nullptr;
            if (V->getType() == I64Ty)
              return V;
            return Unsigned ? PB.CreateZExtOrTrunc(V, I64Ty)
                            : PB.CreateSExtOrTrunc(V, I64Ty);
          };
          Value *Init64 = toI64(InitV);
          Value *Final64 = toI64(FinalV);
          Value *Step64 = toI64(StepV);
          if (!Init64 || !Final64 || !Step64) {
            reject("no_tripcount_expr");
            return false;
          }

          // diff = final - init (+1 for <=).
          Value *Diff = PB.CreateSub(Final64, Init64);
          if (Pred == ICmpInst::ICMP_SLE || Pred == ICmpInst::ICMP_ULE) {
            Diff = PB.CreateAdd(Diff, ConstantInt::get(I64Ty, 1));
          }

          // trip = (diff + step - 1) / step for positive step, else 0.
          Value *Zero = ConstantInt::get(I64Ty, 0);
          Value *One = ConstantInt::get(I64Ty, 1);
          Value *DiffPos = Unsigned ? PB.CreateICmpUGT(Diff, Zero)
                                    : PB.CreateICmpSGT(Diff, Zero);
          Value *StepPos = Unsigned ? PB.CreateICmpUGT(Step64, Zero)
                                    : PB.CreateICmpSGT(Step64, Zero);
          Value *Ok = PB.CreateAnd(DiffPos, StepPos);
          Value *StepM1 = PB.CreateSub(Step64, One);
          Value *Num = PB.CreateAdd(Diff, StepM1);
          Value *Quot = PB.CreateUDiv(Num, Step64);
          TripCountV = PB.CreateSelect(Ok, Quot, Zero);
          RemarkTripcountSource = "loop_bounds_fallback";
        } else {
          // Data-dependent exit (e.g. TSVC break/search loops): use a derived
          // maximum tripcount from the latch compare, and guard side effects
          // via an "active" slot carried across iterations.
          DerivedMaxTripCount = deriveMaxTripCountFromLatch();
          if (!DerivedMaxTripCount) {
            reject("no_tripcount_expr");
            return false;
          }
          TripCountV = ConstantInt::get(I64Ty, *DerivedMaxTripCount);
          NeedsActiveReplay = true;
          RemarkTripcountSource = "latch_max";
        }
        if (!TripCountV) {
          reject("tripcount_expand_failed");
          return false;
        }
        if (!TripCountV->getType()->isIntegerTy()) {
          reject("tripcount_non_integer");
          return false;
        }
        if (TripCountV->getType() != I64Ty) {
          TripCountV = PB.CreateZExtOrTrunc(TripCountV, I64Ty);
          if (!TripCountV) {
            reject("tripcount_expand_failed");
            return false;
          }
        }
        uint64_t ConstTripCount = 0;
        bool HasConstTripCount = false;
        if (TripCountExpr) {
          if (const auto *TC = dyn_cast<SCEVConstant>(TripCountExpr)) {
            const APInt &TripImm = TC->getAPInt();
            if (TripImm.isStrictlyPositive() && TripImm.ule(UINT64_MAX)) {
              ConstTripCount = TripImm.getZExtValue();
              HasConstTripCount = true;
              RemarkTripcountSource = "scev_constant";
            }
          }
        }

        bool NeedsExecMaskSaveRestore = false;
        struct IfConvertibleSplitInfo {
          BasicBlock *BranchBB = nullptr;
          BasicBlock *TrueEntryBB = nullptr;
          BasicBlock *TrueExitBB = nullptr;
          BasicBlock *FalseEntryBB = nullptr;
          BasicBlock *FalseExitBB = nullptr;
          BasicBlock *MergeBB = nullptr;
        };
        struct IfConvertibleStoreMergePlan {
          BasicBlock *BranchBB = nullptr;
          BasicBlock *TrueStoreBB = nullptr;
          StoreInst *TrueStore = nullptr;
          BasicBlock *FalseStoreBB = nullptr;
          StoreInst *FalseStore = nullptr;
          BasicBlock *MergeBB = nullptr;
        };
        struct ReplayMaskSplitInfo {
          BasicBlock *BranchBB = nullptr;
          BasicBlock *TrueEntryBB = nullptr;
          BasicBlock *FalseEntryBB = nullptr;
          BasicBlock *MergeBB = nullptr;
        };
        DenseMap<BasicBlock *, IfConvertibleSplitInfo> IfConvertibleSplits;
        DenseMap<BasicBlock *, IfConvertibleStoreMergePlan>
            IfConvertibleStoreMerges;
        DenseMap<BasicBlock *, ReplayMaskSplitInfo> ReplayMaskSplits;
        SmallPtrSet<BasicBlock *, 16> IfConvertibleRegionBlocks;
        struct IfConvertibleRegionInfo {
          BasicBlock *ExitBB = nullptr;
          SmallVector<BasicBlock *, 8> Blocks;
        };
        auto isSpeculativeValueBlock = [&](BasicBlock *SideBB,
                                           BasicBlock *MergeBB) -> bool {
          if (!SideBB || !MergeBB || SideBB == MergeBB)
            return false;
          auto *BI = dyn_cast<BranchInst>(SideBB->getTerminator());
          if (!BI || BI->isConditional() || BI->getNumSuccessors() != 1 ||
              BI->getSuccessor(0) != MergeBB)
            return false;

          for (Instruction &I : *SideBB) {
            if (&I == BI)
              continue;
            if (isa<PHINode>(I))
              return false;
            if (!isSafeToSpeculativelyExecute(&I))
              return false;
            for (User *U : I.users()) {
              auto *UI = dyn_cast<Instruction>(U);
              if (!UI)
                return false;
              if (UI->getParent() == SideBB)
                continue;
              auto *PN = dyn_cast<PHINode>(UI);
              if (!PN || PN->getParent() != MergeBB)
                return false;
            }
          }

          return true;
        };
        auto isPhiBridgeBlock = [&](BasicBlock *BB,
                                    BasicBlock *MergeBB) -> bool {
          if (!BB || !MergeBB || BB == MergeBB)
            return false;
          auto *BI = dyn_cast<BranchInst>(BB->getTerminator());
          if (!BI || BI->isConditional() || BI->getNumSuccessors() != 1 ||
              BI->getSuccessor(0) != MergeBB)
            return false;
          for (Instruction &I : *BB) {
            if (&I == BI)
              continue;
            if (!isa<PHINode>(I))
              return false;
          }
          return true;
        };
        auto analyzeStoreMergeSideBlock =
            [&](BasicBlock *SideBB,
                BasicBlock *MergeBB) -> std::optional<StoreInst *> {
          if (!SideBB || !MergeBB || SideBB == MergeBB)
            return std::nullopt;
          auto *BI = dyn_cast<BranchInst>(SideBB->getTerminator());
          if (!BI || BI->isConditional() || BI->getNumSuccessors() != 1 ||
              BI->getSuccessor(0) != MergeBB)
            return std::nullopt;

          StoreInst *OnlyStore = nullptr;
          for (Instruction &I : *SideBB) {
            if (&I == BI)
              continue;
            if (isa<PHINode>(I))
              return std::nullopt;
            if (auto *SI = dyn_cast<StoreInst>(&I)) {
              if (OnlyStore || SI->isVolatile() || SI->isAtomic())
                return std::nullopt;
              OnlyStore = SI;
              continue;
            }
            if (!isSafeToSpeculativelyExecute(&I))
              return std::nullopt;
            for (User *U : I.users()) {
              auto *UI = dyn_cast<Instruction>(U);
              if (!UI)
                return std::nullopt;
              if (UI->getParent() == SideBB)
                continue;
              if (UI == OnlyStore)
                continue;
              auto *SI = dyn_cast<StoreInst>(UI);
              if (!SI || SI->getParent() != SideBB)
                return std::nullopt;
            }
          }
          if (!OnlyStore)
            return std::nullopt;
          return OnlyStore;
        };
        auto isPureBranchHeader = [&](BasicBlock *BB) -> bool {
          if (!BB)
            return false;
          auto *BI = dyn_cast<BranchInst>(BB->getTerminator());
          if (!BI || !BI->isConditional() || BI->getNumSuccessors() != 2)
            return false;
          for (Instruction &I : *BB) {
            if (&I == BI)
              continue;
            if (isa<PHINode>(I))
              return false;
            if (!isSafeToSpeculativelyExecute(&I))
              return false;
            for (User *U : I.users()) {
              auto *UI = dyn_cast<Instruction>(U);
              if (!UI)
                return false;
              if (UI->getParent() == BB)
                continue;
              if (UI == BI)
                continue;
              return false;
            }
          }
          return true;
        };
        std::function<std::optional<IfConvertibleRegionInfo>(
            BasicBlock *, BasicBlock *, SmallPtrSetImpl<BasicBlock *> &)>
            analyzeIfConvertibleRegion;
        analyzeIfConvertibleRegion = [&](BasicBlock *EntryBB,
                                         BasicBlock *FinalMergeBB,
                                         SmallPtrSetImpl<BasicBlock *> &Visited)
            -> std::optional<IfConvertibleRegionInfo> {
          if (!EntryBB || !FinalMergeBB || EntryBB == FinalMergeBB ||
              !L->contains(EntryBB) || !L->contains(FinalMergeBB))
            return std::nullopt;
          if (!Visited.insert(EntryBB).second)
            return std::nullopt;

          if (isSpeculativeValueBlock(EntryBB, FinalMergeBB)) {
            IfConvertibleRegionInfo Info;
            Info.ExitBB = EntryBB;
            Info.Blocks.push_back(EntryBB);
            return Info;
          }

          if (!isPureBranchHeader(EntryBB))
            return std::nullopt;
          auto *BI = cast<BranchInst>(EntryBB->getTerminator());
          BasicBlock *S0 = BI->getSuccessor(0);
          BasicBlock *S1 = BI->getSuccessor(1);
          if (!S0 || !S1 || !L->contains(S0) || !L->contains(S1))
            return std::nullopt;

          for (BasicBlock *CandidateMerge : L->blocks()) {
            if (!isPhiBridgeBlock(CandidateMerge, FinalMergeBB))
              continue;
            SmallPtrSet<BasicBlock *, 16> LeftVisited(Visited.begin(),
                                                      Visited.end());
            auto Left =
                analyzeIfConvertibleRegion(S0, CandidateMerge, LeftVisited);
            if (!Left)
              continue;
            SmallPtrSet<BasicBlock *, 16> RightVisited(Visited.begin(),
                                                       Visited.end());
            auto Right =
                analyzeIfConvertibleRegion(S1, CandidateMerge, RightVisited);
            if (!Right)
              continue;

            SmallPtrSet<BasicBlock *, 16> Unique;
            IfConvertibleRegionInfo Info;
            Info.ExitBB = CandidateMerge;
            auto AddBlock = [&](BasicBlock *BB) {
              if (BB && Unique.insert(BB).second)
                Info.Blocks.push_back(BB);
            };
            AddBlock(EntryBB);
            for (BasicBlock *BB : Left->Blocks)
              AddBlock(BB);
            for (BasicBlock *BB : Right->Blocks)
              AddBlock(BB);
            AddBlock(CandidateMerge);
            return Info;
          }

          return std::nullopt;
        };
        if (HasInnerCF) {
          for (BasicBlock *BB : L->blocks()) {
            auto *BI = dyn_cast_or_null<BranchInst>(BB->getTerminator());
            if (!BI || !BI->isConditional())
              continue;
            if (BB == Latch)
              continue;
            BasicBlock *S0 = BI->getSuccessor(0);
            BasicBlock *S1 = BI->getSuccessor(1);
            if (S0 && S1 && L->contains(S0) && L->contains(S1)) {
              auto *S0BI = dyn_cast_or_null<BranchInst>(S0->getTerminator());
              auto *S1BI = dyn_cast_or_null<BranchInst>(S1->getTerminator());
              BasicBlock *StoreMerge0 = (S0BI && !S0BI->isConditional() &&
                                         S0BI->getNumSuccessors() == 1)
                                            ? S0BI->getSuccessor(0)
                                            : nullptr;
              BasicBlock *StoreMerge1 = (S1BI && !S1BI->isConditional() &&
                                         S1BI->getNumSuccessors() == 1)
                                            ? S1BI->getSuccessor(0)
                                            : nullptr;
              if (StoreMerge0 && StoreMerge0 == StoreMerge1 &&
                  L->contains(StoreMerge0)) {
                auto TrueStore = analyzeStoreMergeSideBlock(S0, StoreMerge0);
                auto FalseStore = analyzeStoreMergeSideBlock(S1, StoreMerge0);
                if (TrueStore && FalseStore) {
                  const SCEV *TruePtrS =
                      SE.getSCEVAtScope((*TrueStore)->getPointerOperand(), L);
                  const SCEV *FalsePtrS =
                      SE.getSCEVAtScope((*FalseStore)->getPointerOperand(), L);
                  if (TruePtrS == FalsePtrS) {
                    IfConvertibleSplits[BB] = {BB, S0, S0, S1, S1, StoreMerge0};
                    IfConvertibleStoreMerges[StoreMerge0] = {
                        BB, S0, *TrueStore, S1, *FalseStore, StoreMerge0};
                    IfConvertibleRegionBlocks.insert(S0);
                    IfConvertibleRegionBlocks.insert(S1);
                    goto found_ifconvertible_split;
                  }
                }
              }
              for (BasicBlock *MergeBB : L->blocks()) {
                SmallPtrSet<BasicBlock *, 16> LeftVisited;
                auto TrueRegion =
                    analyzeIfConvertibleRegion(S0, MergeBB, LeftVisited);
                if (!TrueRegion)
                  continue;
                SmallPtrSet<BasicBlock *, 16> RightVisited;
                auto FalseRegion =
                    analyzeIfConvertibleRegion(S1, MergeBB, RightVisited);
                if (!FalseRegion)
                  continue;

                IfConvertibleSplits[BB] = {
                    BB,     S0, TrueRegion->ExitBB, S1, FalseRegion->ExitBB,
                    MergeBB};
                for (BasicBlock *RB : TrueRegion->Blocks)
                  IfConvertibleRegionBlocks.insert(RB);
                for (BasicBlock *RB : FalseRegion->Blocks)
                  IfConvertibleRegionBlocks.insert(RB);
                goto found_ifconvertible_split;
              }
              if (NeedsActiveReplay && StoreMerge0 &&
                  StoreMerge0 == StoreMerge1 && L->contains(StoreMerge0)) {
                ReplayMaskSplits[BB] = {BB, S0, S1, StoreMerge0};
                goto found_ifconvertible_split;
              }
              NeedsExecMaskSaveRestore = true;
              break;
            }
          found_ifconvertible_split:
            continue;
          }
        }
        if (!HasConstTripCount && TripCountOpt.has_value() &&
            TripCountOpt.value_or(0) > 0 &&
            isUInt<63>(TripCountOpt.value_or(0))) {
          ConstTripCount = *TripCountOpt;
          HasConstTripCount = true;
          RemarkTripcountSource = "loop_bounds";
        }
        if (!HasConstTripCount && DerivedMaxTripCount) {
          ConstTripCount = *DerivedMaxTripCount;
          HasConstTripCount = true;
        }
        if (!HasConstTripCount && RemarkTripcountSource == "none")
          RemarkTripcountSource = "scev_dynamic";

        SmallVector<StoreInst *, 8> Stores;
        SmallVector<LoadInst *, 16> Loads;
        for (BasicBlock *BB : L->blocks()) {
          for (Instruction &I : *BB) {
            if (auto *SI = dyn_cast<StoreInst>(&I)) {
              if (SI->isVolatile() || SI->isAtomic()) {
                reject("volatile_or_atomic_store");
                return false;
              }
              Stores.push_back(SI);
            } else if (auto *LI = dyn_cast<LoadInst>(&I)) {
              if (LI->isVolatile() || LI->isAtomic()) {
                reject("volatile_or_atomic_load");
                return false;
              }
              Loads.push_back(LI);
            }
          }
        }
        RemarkTouchesMemoryState =
            ((!Stores.empty() || !Loads.empty()) ? 1 : 0);

        struct ReductionPlan {
          PHINode *Phi = nullptr;
          Instruction *Update = nullptr;
          Value *LaneValue = nullptr;
          Value *LaneMulL = nullptr;
          Value *LaneMulR = nullptr;
          Value *InitValue = nullptr;
          ReductionKind Kind = ReductionKind::AddF;
          AllocaInst *Slot = nullptr;
          uint32_t SlotElems = 1;
          std::string DstName;
          unsigned SlotBind = 0;
          std::optional<uint64_t> LocalWordBase;
        };

        struct RecurrencePlan {
          PHINode *Phi = nullptr;
          Instruction *Update = nullptr;
          Value *InitValue = nullptr;
          Type *SlotTy =
              nullptr; // storage type for v.lw/v.sw (must be 32-bit or f32)
          AllocaInst *Slot = nullptr;
          unsigned SlotBind = 0;
          std::optional<uint64_t> LocalWordBase;
        };

        struct F32InductionPlan {
          CastInst *Cast = nullptr;
          int64_t Start = 0;
          int64_t Step = 0;
          AllocaInst *Slot = nullptr;
          unsigned SlotBind = 0;
        };

        struct ExitPhiPlan {
          PHINode *Phi = nullptr;
          AllocaInst *Slot = nullptr;
          unsigned SlotBind = 0;
          std::optional<uint64_t> LocalWordBase;
        };

        LLVMContext &Ctx = F.getContext();
        SmallVector<ReductionPlan, 4> ReductionPlans;
        SmallVector<RecurrencePlan, 8> RecurrencePlans;
        SmallVector<F32InductionPlan, 4> F32InductionPlans;
        SmallVector<ExitPhiPlan, 8> ExitPhiPlans;
        DenseMap<const CastInst *, unsigned> F32InductionPlanByCast;
        auto hasExternalUse = [&](Value *V) -> bool {
          if (!V)
            return false;
          for (User *U : V->users()) {
            auto *UI = dyn_cast<Instruction>(U);
            if (!UI)
              return true;
            if (!L->contains(UI))
              return true;
          }
          return false;
        };
        auto tryAddReductionPlan = [&](PHINode *Phi) -> bool {
          if (!Phi || Phi->getNumIncomingValues() != 2)
            return false;
          if (!Phi->getType()->isFloatTy())
            return false;

          auto hasInLoopStoreUse = [&](Value *V) {
            if (!V)
              return false;
            for (User *U : V->users()) {
              auto *UI = dyn_cast<Instruction>(U);
              if (!UI || !L->contains(UI))
                continue;
              if (isa<StoreInst>(UI))
                return true;
            }
            return false;
          };

          int InitIdx = -1;
          int LoopIdx = -1;
          for (int I = 0; I < 2; I++) {
            BasicBlock *IB = Phi->getIncomingBlock(I);
            if (L->contains(IB)) {
              LoopIdx = I;
            } else {
              InitIdx = I;
            }
          }
          if (InitIdx < 0 || LoopIdx < 0)
            return false;
          if (Phi->getIncomingBlock(InitIdx) != Preheader)
            return false;

          Value *InitV = Phi->getIncomingValue(InitIdx);
          auto *UpdateI = dyn_cast<Instruction>(Phi->getIncomingValue(LoopIdx));
          if (!UpdateI || !L->contains(UpdateI))
            return false;

          Value *LaneValue = nullptr;
          Value *LaneMulL = nullptr;
          Value *LaneMulR = nullptr;
          std::optional<ReductionKind> KindOpt;

          if (auto DirectKind = classifyReductionOp(UpdateI)) {
            KindOpt = DirectKind;
            Value *Op0 = UpdateI->getOperand(0);
            Value *Op1 = UpdateI->getOperand(1);
            if (Op0 == Phi)
              LaneValue = Op1;
            else if (Op1 == Phi)
              LaneValue = Op0;
            else
              KindOpt.reset();
          }

          if (!KindOpt) {
            if (auto *CB = dyn_cast<CallBase>(UpdateI)) {
              Function *Callee = CB->getCalledFunction();
              if (Callee && Callee->isIntrinsic() && CB->arg_size() == 3 &&
                  CB->getType() == Type::getFloatTy(Ctx) &&
                  (Callee->getIntrinsicID() == Intrinsic::fmuladd ||
                   Callee->getIntrinsicID() == Intrinsic::fma)) {
                Value *A = CB->getArgOperand(0);
                Value *B = CB->getArgOperand(1);
                Value *C = CB->getArgOperand(2);
                if (C == Phi) {
                  KindOpt = ReductionKind::AddF;
                  LaneMulL = A;
                  LaneMulR = B;
                } else if (B == Phi) {
                  KindOpt = ReductionKind::AddF;
                  LaneMulL = A;
                  LaneMulR = C;
                } else if (A == Phi) {
                  KindOpt = ReductionKind::AddF;
                  LaneMulL = B;
                  LaneMulR = C;
                }
              }
            }
          }

          if (!KindOpt) {
            auto *Sel = dyn_cast<SelectInst>(UpdateI);
            auto *Cmp = Sel ? dyn_cast<CmpInst>(Sel->getCondition()) : nullptr;
            if (!Sel || !Cmp)
              return false;

            Value *TV = Sel->getTrueValue();
            Value *FV = Sel->getFalseValue();
            Value *Other = nullptr;
            bool SelectOtherOnTrue = false;
            if (TV == Phi && FV != Phi) {
              Other = FV;
              SelectOtherOnTrue = false;
            } else if (FV == Phi && TV != Phi) {
              Other = TV;
              SelectOtherOnTrue = true;
            } else {
              return false;
            }

            Value *LHS = Cmp->getOperand(0);
            Value *RHS = Cmp->getOperand(1);
            CmpInst::Predicate Pred = Cmp->getPredicate();
            if (LHS == Other && RHS == Phi) {
              // Already canonical.
            } else if (LHS == Phi && RHS == Other) {
              Pred = Cmp->getSwappedPredicate();
            } else {
              return false;
            }

            bool OtherGreater = false;
            bool OtherLess = false;
            switch (Pred) {
            case CmpInst::ICMP_SGT:
            case CmpInst::ICMP_UGT:
            case CmpInst::ICMP_SGE:
            case CmpInst::ICMP_UGE:
            case CmpInst::FCMP_OGT:
            case CmpInst::FCMP_OGE:
              OtherGreater = true;
              break;
            case CmpInst::ICMP_SLT:
            case CmpInst::ICMP_ULT:
            case CmpInst::ICMP_SLE:
            case CmpInst::ICMP_ULE:
            case CmpInst::FCMP_OLT:
            case CmpInst::FCMP_OLE:
              OtherLess = true;
              break;
            default:
              return false;
            }

            const bool IsInt = Phi->getType()->isIntegerTy();
            const bool IsFloat = Phi->getType()->isFloatTy();
            if (!IsInt && !IsFloat)
              return false;

            const bool IsMax = SelectOtherOnTrue ? OtherGreater : OtherLess;
            if (IsFloat)
              KindOpt = IsMax ? ReductionKind::MaxF : ReductionKind::MinF;
            else
              KindOpt = IsMax ? ReductionKind::MaxI : ReductionKind::MinI;
            LaneValue = Other;
          }

          if (!KindOpt)
            return false;
          if (!LaneValue && !(LaneMulL && LaneMulR))
            return false;
          if (hasInLoopStoreUse(Phi) || hasInLoopStoreUse(UpdateI))
            return false;

          if (!hasExternalUse(Phi) && !hasExternalUse(UpdateI))
            return false;

          ReductionPlan Plan;
          Plan.Phi = Phi;
          Plan.Update = UpdateI;
          Plan.LaneValue = LaneValue;
          Plan.LaneMulL = LaneMulL;
          Plan.LaneMulR = LaneMulR;
          Plan.InitValue = InitV;
          Plan.Kind = *KindOpt;
          ReductionPlans.push_back(std::move(Plan));
          return true;
        };

        for (Instruction &I : *Header) {
          auto *Phi = dyn_cast<PHINode>(&I);
          if (!Phi)
            break;
          (void)tryAddReductionPlan(Phi);
        }

        auto isSupportedReductionKind = [](ReductionKind Kind) -> bool {
          switch (Kind) {
          case ReductionKind::AddI:
          case ReductionKind::AddF:
          case ReductionKind::AndI:
          case ReductionKind::OrI:
          case ReductionKind::XorI:
          case ReductionKind::MinI:
          case ReductionKind::MaxI:
          case ReductionKind::MinF:
          case ReductionKind::MaxF:
            return true;
          case ReductionKind::MulI:
          case ReductionKind::MulF:
            return false;
          }
          llvm_unreachable("invalid reduction kind");
        };

        // Only keep reductions that LinxISA 0.58 auto-vectorization can lower.
        // Unsupported patterns are handled via recurrence slots instead.
        SmallVector<ReductionPlan, 4> SupportedReductionPlans;
        SmallPtrSet<const PHINode *, 8> SupportedReductionPhis;
        for (ReductionPlan &Plan : ReductionPlans) {
          if (!isSupportedReductionKind(Plan.Kind))
            continue;
          if (!isReductionIdentityValue(Plan.Kind, Plan.InitValue))
            continue;
          SupportedReductionPhis.insert(Plan.Phi);
          SupportedReductionPlans.push_back(std::move(Plan));
        }
        ReductionPlans.swap(SupportedReductionPlans);

        auto tryAddRecurrencePlan = [&](PHINode *Phi) -> bool {
          if (!Phi || Phi->getNumIncomingValues() != 2)
            return false;
          if (SupportedReductionPhis.contains(Phi))
            return false;

          // Avoid treating canonical affine IVs as recurrence slots: we can
          // emit them directly from (lc0/lc1) via SCEV AddRec lowering.
          if (Phi->getType()->isIntegerTy()) {
            const SCEV *PS = SE.getSCEVAtScope(Phi, L);
            if (const auto *AR = dyn_cast<SCEVAddRecExpr>(PS)) {
              if (AR->getLoop() == L && AR->isAffine())
                return false;
            }
          }

          Type *PhiTy = Phi->getType();
          Type *SlotTy = nullptr;
          if (PhiTy->isFloatTy()) {
            SlotTy = PhiTy;
          } else if (PhiTy->isIntegerTy()) {
            const unsigned Bits = PhiTy->getScalarSizeInBits();
            if (Bits <= 32) {
              SlotTy = PhiTy;
            } else if (Bits <= 64) {
              // Bring-up support: vblock only has 32-bit lane-wide
              // loads/stores. For widened index-like recurrences (common in
              // TSVC), store the low 32 bits and treat the value as unsigned.
              SlotTy = I32Ty;
            } else {
              return false;
            }
          } else {
            return false;
          }

          int InitIdx = -1;
          int LoopIdx = -1;
          for (int I = 0; I < 2; I++) {
            BasicBlock *IB = Phi->getIncomingBlock(I);
            if (L->contains(IB)) {
              LoopIdx = I;
            } else {
              InitIdx = I;
            }
          }
          if (InitIdx < 0 || LoopIdx < 0)
            return false;
          if (Phi->getIncomingBlock(InitIdx) != Preheader)
            return false;

          auto *UpdateI = dyn_cast<Instruction>(Phi->getIncomingValue(LoopIdx));
          if (!UpdateI || !L->contains(UpdateI))
            return false;

          RecurrencePlan Plan;
          Plan.Phi = Phi;
          Plan.Update = UpdateI;
          Plan.InitValue = Phi->getIncomingValue(InitIdx);
          Plan.SlotTy = SlotTy;
          RecurrencePlans.push_back(std::move(Plan));
          return true;
        };

        for (Instruction &I : *Header) {
          auto *Phi = dyn_cast<PHINode>(&I);
          if (!Phi)
            break;
          (void)tryAddRecurrencePlan(Phi);
        }

        SmallPtrSet<const Value *, 16> AllowedLiveOutValues;
        for (const ReductionPlan &Plan : ReductionPlans) {
          AllowedLiveOutValues.insert(Plan.Phi);
          AllowedLiveOutValues.insert(Plan.Update);
        }
        for (const RecurrencePlan &Plan : RecurrencePlans) {
          AllowedLiveOutValues.insert(Plan.Phi);
          AllowedLiveOutValues.insert(Plan.Update);
        }
        RemarkHasRecurrence = !RecurrencePlans.empty();

        // Scalar-lane replay preserves recurrence order only under sequential
        // group execution. A parallel-safe loop hint must not retain MPAR once
        // a generic recurrence plan has been discovered.
        if (!RecurrencePlans.empty())
          SelectedMode = "mseq";

        SmallVector<Instruction *, 8> LiveOutInsts;
        SmallPtrSet<const Instruction *, 8> LiveOutInstSet;

        if (Stores.empty() && ReductionPlans.empty() &&
            RecurrencePlans.empty() && !NeedsActiveReplay && !ExitHasPhi) {
          reject("no_store_in_loop");
          return false;
        }

        auto underlyingObj = [](Value *Ptr) -> const Value * {
          return getUnderlyingObject(Ptr->stripPointerCasts());
        };

        struct SelectBaseGEPInfo {
          GEPOperator *GEP = nullptr;
          SelectInst *BaseSel = nullptr;
          Value *Index = nullptr;
        };

        struct SelectStorePtrInfo {
          Value *Cond = nullptr;
          GEPOperator *TrueGEP = nullptr;
          Value *TrueBase = nullptr;
          GEPOperator *FalseGEP = nullptr;
          Value *FalseBase = nullptr;
        };

        auto matchGEPIndexForElemBytes =
            [&](Value *Ptr,
                uint64_t ElemBytes) -> std::optional<std::pair<Value *, bool>> {
          auto *GEP = dyn_cast_or_null<GEPOperator>(Ptr);
          if (!GEP && Ptr)
            GEP = dyn_cast<GEPOperator>(Ptr->stripPointerCasts());
          if (!GEP || !isPowerOf2_64(ElemBytes) || ElemBytes == 0 ||
              ElemBytes > 8)
            return std::nullopt;

          Value *Index = nullptr;
          const unsigned NumIdx = GEP->getNumIndices();
          if (NumIdx == 1) {
            Index = GEP->getOperand(1);
          } else if (NumIdx == 2) {
            auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
            if (!Z || !Z->isZero())
              return std::nullopt;
            Index = GEP->getOperand(2);
          } else {
            return std::nullopt;
          }

          Type *ElemTy = GEP->getResultElementType();
          if (!ElemTy)
            return std::nullopt;
          const DataLayout &DL = F.getParent()->getDataLayout();
          const uint64_t GEPBytes = DL.getTypeStoreSize(ElemTy);
          if (GEPBytes == ElemBytes)
            return std::make_pair(Index, false);
          if (GEPBytes == 1 && ElemBytes > 1)
            return std::make_pair(Index, true);
          return std::nullopt;
        };

        auto matchDirectSelectBaseGEP =
            [&](Value *Ptr,
                uint64_t ElemBytes) -> std::optional<SelectStorePtrInfo> {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          auto *GEP = dyn_cast_or_null<GetElementPtrInst>(Ptr);
          if (!GEP)
            return std::nullopt;
          if (!matchGEPIndexForElemBytes(GEP, ElemBytes))
            return std::nullopt;
          auto *BaseSel = dyn_cast<SelectInst>(GEP->getPointerOperand());
          if (!BaseSel || !BaseSel->getType()->isPointerTy())
            return std::nullopt;
          Value *TrueBase = BaseSel->getTrueValue()->stripPointerCasts();
          Value *FalseBase = BaseSel->getFalseValue()->stripPointerCasts();
          if (!L->isLoopInvariant(TrueBase) || !L->isLoopInvariant(FalseBase))
            return std::nullopt;
          return SelectStorePtrInfo{
              BaseSel->getCondition(),
              cast<GEPOperator>(GEP),
              TrueBase,
              cast<GEPOperator>(GEP),
              FalseBase,
          };
        };

        auto matchInvariantBaseGEP = [&](Value *Ptr, uint64_t ElemBytes)
            -> std::optional<std::pair<GEPOperator *, Value *>> {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          auto *GEP = dyn_cast_or_null<GEPOperator>(Ptr);
          if (!GEP && Ptr)
            GEP = dyn_cast<GEPOperator>(Ptr->stripPointerCasts());
          if (!GEP)
            return std::nullopt;
          if (!matchGEPIndexForElemBytes(GEP, ElemBytes))
            return std::nullopt;
          Value *BasePtr = GEP->getPointerOperand()->stripPointerCasts();
          if (!L->isLoopInvariant(BasePtr))
            return std::nullopt;
          return std::make_pair(GEP, BasePtr);
        };

        auto matchSelectBaseGEP =
            [&](Value *Ptr,
                uint64_t ElemBytes) -> std::optional<SelectBaseGEPInfo> {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          auto *GEP = dyn_cast_or_null<GEPOperator>(Ptr);
          if (!GEP && Ptr)
            GEP = dyn_cast<GEPOperator>(Ptr->stripPointerCasts());
          if (!GEP)
            return std::nullopt;

          auto IndexInfo = matchGEPIndexForElemBytes(GEP, ElemBytes);
          if (!IndexInfo)
            return std::nullopt;
          Value *Index = IndexInfo->first;

          Value *BasePtr = GEP->getPointerOperand();
          auto *BaseSel = dyn_cast<SelectInst>(BasePtr);
          if (!BaseSel)
            BaseSel = dyn_cast<SelectInst>(BasePtr->stripPointerCasts());
          if (!BaseSel || !BaseSel->getType()->isPointerTy())
            return std::nullopt;
          Value *TrueBase = BaseSel->getTrueValue()->stripPointerCasts();
          Value *FalseBase = BaseSel->getFalseValue()->stripPointerCasts();
          if (!L->isLoopInvariant(TrueBase) || !L->isLoopInvariant(FalseBase))
            return std::nullopt;
          if (!Index || !Index->getType()->isIntegerTy() ||
              Index->getType()->getScalarSizeInBits() > 64)
            return std::nullopt;
          return SelectBaseGEPInfo{GEP, BaseSel, Index};
        };

        auto matchSelectStorePtr =
            [&](Value *Ptr,
                uint64_t ElemBytes) -> std::optional<SelectStorePtrInfo> {
          if (auto Direct = matchDirectSelectBaseGEP(Ptr, ElemBytes))
            return Direct;
          if (auto BaseSelect = matchSelectBaseGEP(Ptr, ElemBytes)) {
            return SelectStorePtrInfo{
                BaseSelect->BaseSel->getCondition(),
                BaseSelect->GEP,
                BaseSelect->BaseSel->getTrueValue()->stripPointerCasts(),
                BaseSelect->GEP,
                BaseSelect->BaseSel->getFalseValue()->stripPointerCasts(),
            };
          }

          auto *Sel = dyn_cast_or_null<SelectInst>(Ptr);
          if (!Sel && Ptr)
            Sel = dyn_cast<SelectInst>(Ptr->stripPointerCasts());
          if (!Sel || !Sel->getType()->isPointerTy())
            return std::nullopt;
          auto TrueMatch = matchInvariantBaseGEP(
              Sel->getTrueValue()->stripPointerCasts(), ElemBytes);
          auto FalseMatch = matchInvariantBaseGEP(
              Sel->getFalseValue()->stripPointerCasts(), ElemBytes);
          if (!TrueMatch || !FalseMatch)
            return std::nullopt;
          return SelectStorePtrInfo{
              Sel->getCondition(), TrueMatch->first,   TrueMatch->second,
              FalseMatch->first,   FalseMatch->second,
          };
        };

        DenseMap<const StoreInst *, const Value *> StoreObjByInst;
        SmallPtrSet<const Value *, 8> StoreObjects;
        for (StoreInst *SI : Stores) {
          const Value *Obj = underlyingObj(SI->getPointerOperand());
          const Value *Key = Obj ? Obj : SI->getPointerOperand();
          StoreObjByInst[SI] = Key;
          StoreObjects.insert(Key);
        }

        const uint64_t RequestedLaneCount =
            std::max<uint64_t>(1, static_cast<uint64_t>(LinxSIMTAutoVecLanes));
        uint64_t LaneCount = HasConstTripCount ? ConstTripCount : 1;
        uint64_t GroupCount = 1;
        bool UseGroupedDims = false;
        const SIMTLayoutPolicy LayoutPolicy = LinxSIMTAutoVecLayout;

        auto isUnitStridePtr = [&](Value *Ptr, uint64_t ElemBytes) -> bool {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return false;
          if (matchSelectStorePtr(Ptr, ElemBytes))
            return true;
          Ptr = Ptr->stripPointerCasts();
          const SCEV *PointerExpr = SE.getSCEVAtScope(Ptr, L);
          const auto *AddRec = dyn_cast<SCEVAddRecExpr>(PointerExpr);
          if (!AddRec || AddRec->getLoop() != L || !AddRec->isAffine())
            return false;
          const auto *StepConst =
              dyn_cast<SCEVConstant>(AddRec->getStepRecurrence(SE));
          if (!StepConst)
            return false;
          return StepConst->getAPInt().getSExtValue() ==
                 static_cast<int64_t>(ElemBytes);
        };

        auto getSIMTMemElemBytes = [&](Type *Ty) -> uint64_t {
          if (!Ty)
            return 0;
          if (Ty->isIntegerTy(1) || Ty->isIntegerTy(8))
            return 1;
          if (Ty->isIntegerTy(16))
            return 2;
          if (Ty->isIntegerTy(32) || Ty->isFloatTy())
            return 4;
          if (Ty->isIntegerTy(64) || Ty->isDoubleTy())
            return 8;
          return 0;
        };

        auto isCanonicalUnitStrideIV = [&](Value *V) -> bool {
          V = stripIntCasts(V);
          const SCEV *S = SE.getSCEVAtScope(V, L);
          const auto *AR = dyn_cast<SCEVAddRecExpr>(S);
          if (!AR || AR->getLoop() != L || !AR->isAffine())
            return false;
          const auto *StartC = dyn_cast<SCEVConstant>(AR->getStart());
          const auto *StepC = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
          if (!StartC || !StepC)
            return false;
          if (!StartC->getAPInt().isZero())
            return false;
          return StepC->getAPInt().getSExtValue() == 1;
        };

        auto matchesMaskedShiftedIV = [&](Value *V, uint64_t ShiftImm) -> bool {
          V = stripIntCasts(V);
          if (auto *BO = dyn_cast_or_null<BinaryOperator>(V)) {
            if (BO->getOpcode() == Instruction::And) {
              ConstantInt *MaskC = dyn_cast<ConstantInt>(BO->getOperand(0));
              Value *Other = BO->getOperand(1);
              if (!MaskC) {
                MaskC = dyn_cast<ConstantInt>(BO->getOperand(1));
                Other = BO->getOperand(0);
              }
              if (!MaskC)
                return false;
              const uint64_t Mask = MaskC->getZExtValue();
              if (Mask != 0xffffffffffffffffULL && Mask != 0xffffffffULL &&
                  Mask != 0x7fffffffULL)
                return false;
              V = stripIntCasts(Other);
            }
          }

          auto *BO = dyn_cast_or_null<BinaryOperator>(V);
          if (!BO || BO->getOpcode() != Instruction::LShr)
            return false;
          auto *Sh = dyn_cast<ConstantInt>(BO->getOperand(1));
          if (!Sh || Sh->getZExtValue() != ShiftImm)
            return false;
          return isCanonicalUnitStrideIV(BO->getOperand(0));
        };

        auto hasIVShiftByConst = [&](uint64_t ShiftImm) -> bool {
          for (BasicBlock *BB : L->blocks()) {
            for (Instruction &I : *BB) {
              if (!matchesMaskedShiftedIV(&I, ShiftImm))
                continue;
              return true;
            }
          }
          return false;
        };

        // Recurrence-carrying loops are executed in scalar-lane replay mode
        // (LB1), so we do not require unit-stride memory for correctness.
        bool MemoryIsUnitStride = true;
        for (StoreInst *SI : Stores) {
          Type *StoreTy = SI->getValueOperand()->getType();
          uint64_t ElemBytes = getSIMTMemElemBytes(StoreTy);
          bool StoreIsUnitStride =
              isUnitStridePtr(SI->getPointerOperand(), ElemBytes);
          if (!StoreIsUnitStride) {
            MemoryIsUnitStride = false;
            break;
          }
        }
        if (MemoryIsUnitStride) {
          for (LoadInst *LI : Loads) {
            const uint64_t ElemBytes = getSIMTMemElemBytes(LI->getType());
            bool LoadIsUnitStride =
                isUnitStridePtr(LI->getPointerOperand(), ElemBytes);
            if (!LoadIsUnitStride) {
              MemoryIsUnitStride = false;
              break;
            }
          }
        }

        bool ForceScalarLane = (LayoutPolicy != SIMTLayoutPolicy::Grouped);
        std::optional<uint64_t> ForcedLaneCount;

        // If the loop index is explicitly shifted right (e.g. i >> 1),
        // prefer a small grouped-lane mapping so the shift can be expressed
        // as the group index (lc1). This is needed by TSVC kernels that use
        // patterns like c[i/2].
        if (HasConstTripCount && ConstTripCount > 2 &&
            (ConstTripCount % 2) == 0 && hasIVShiftByConst(1)) {
          ForcedLaneCount = 2;
        }

        auto selectGroupedLayout = [&](uint64_t ChosenLaneCount) {
          LaneCount = ChosenLaneCount;
          GroupCount =
              (ChosenLaneCount == 0) ? 1 : (ConstTripCount / ChosenLaneCount);
          UseGroupedDims = (GroupCount > 1);
          ForceScalarLane = false;
          RemarkLayoutKind =
              UseGroupedDims ? "grouped-strip-mined" : "grouped-single-group";
        };

        auto isShiftFriendlyGroupedPtr = [&](Value *Ptr, uint64_t ElemBytes,
                                             uint64_t CandidateLaneCount) {
          if (!Ptr || CandidateLaneCount <= 1 ||
              !isPowerOf2_64(CandidateLaneCount))
            return false;
          Ptr = Ptr->stripPointerCasts();
          auto *GEP = dyn_cast<GEPOperator>(Ptr);
          if (!GEP)
            return false;

          Value *Index = nullptr;
          const unsigned NumIdx = GEP->getNumIndices();
          if (NumIdx == 1) {
            Index = GEP->getOperand(1);
          } else if (NumIdx == 2) {
            auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
            if (!Z || !Z->isZero())
              return false;
            Index = GEP->getOperand(2);
          } else {
            return false;
          }

          Type *ElemTy = GEP->getResultElementType();
          if (!ElemTy)
            return false;
          const DataLayout &DL = F.getParent()->getDataLayout();
          if (DL.getTypeStoreSize(ElemTy) != ElemBytes)
            return false;
          if (!L->isLoopInvariant(
                  GEP->getPointerOperand()->stripPointerCasts()))
            return false;

          const uint64_t ShiftImm = Log2_64(CandidateLaneCount);
          return matchesMaskedShiftedIV(Index, ShiftImm);
        };

        auto memorySupportsGroupedLaneCount = [&](uint64_t CandidateLaneCount) {
          if (MemoryIsUnitStride)
            return true;
          if (CandidateLaneCount <= 1 || !isPowerOf2_64(CandidateLaneCount))
            return false;

          for (StoreInst *SI : Stores) {
            const uint64_t ElemBytes =
                getSIMTMemElemBytes(SI->getValueOperand()->getType());
            if (isUnitStridePtr(SI->getPointerOperand(), ElemBytes))
              continue;
            if (isShiftFriendlyGroupedPtr(SI->getPointerOperand(), ElemBytes,
                                          CandidateLaneCount))
              continue;
            return false;
          }

          for (LoadInst *LI : Loads) {
            const uint64_t ElemBytes = getSIMTMemElemBytes(LI->getType());
            if (isUnitStridePtr(LI->getPointerOperand(), ElemBytes))
              continue;
            if (isShiftFriendlyGroupedPtr(LI->getPointerOperand(), ElemBytes,
                                          CandidateLaneCount))
              continue;
            return false;
          }

          return true;
        };

        auto canUseGroupedLaneCount = [&](uint64_t CandidateLaneCount) {
          // Generic loop-carried recurrences are order-dependent. Unlike the
          // supported reduction plans above, they cannot be split into
          // independent per-lane states and combined after grouped execution.
          if (!RecurrencePlans.empty())
            return false;
          if (!HasConstTripCount || ConstTripCount == 0 ||
              CandidateLaneCount <= 1)
            return false;
          if (!isPowerOf2_64(CandidateLaneCount))
            return false;
          if ((ConstTripCount % CandidateLaneCount) != 0)
            return false;
          if (!Stores.empty() || !Loads.empty())
            return memorySupportsGroupedLaneCount(CandidateLaneCount);
          return true;
        };

        auto groupedRejectReason = [&]() -> const char * {
          if (!RecurrencePlans.empty())
            return "grouped_layout_unsupported_recurrence";
          if (!HasConstTripCount)
            return "grouped_layout_requires_static_tripcount";
          if (NeedsExecMaskSaveRestore)
            return "grouped_layout_requires_exec_mask_save_restore";
          if ((!Stores.empty() || !Loads.empty()) && !MemoryIsUnitStride)
            return "grouped_layout_requires_unit_stride_memory";
          return "grouped_layout_unavailable";
        };

        uint64_t PreferredGroupedLaneCount = 0;
        if (ForcedLaneCount && canUseGroupedLaneCount(*ForcedLaneCount)) {
          PreferredGroupedLaneCount = *ForcedLaneCount;
        } else if (canUseGroupedLaneCount(RequestedLaneCount) &&
                   ConstTripCount >= RequestedLaneCount) {
          PreferredGroupedLaneCount = RequestedLaneCount;
        } else if (canUseGroupedLaneCount(ConstTripCount)) {
          PreferredGroupedLaneCount = ConstTripCount;
        }

        if (LayoutPolicy == SIMTLayoutPolicy::Grouped &&
            NeedsExecMaskSaveRestore) {
          RemarkCFStrategy = "exec-mask-save-restore-required";
          reject(groupedRejectReason());
          return false;
        }

        if (LayoutPolicy == SIMTLayoutPolicy::Grouped &&
            PreferredGroupedLaneCount == 0) {
          if (NeedsActiveReplay)
            RemarkCFStrategy = "active-replay";
          reject(groupedRejectReason());
          return false;
        }

        if (PreferredGroupedLaneCount > 0 && !NeedsExecMaskSaveRestore &&
            LayoutPolicy != SIMTLayoutPolicy::ScalarReplay) {
          selectGroupedLayout(PreferredGroupedLaneCount);
        } else {
          LaneCount = 1;
          GroupCount = HasConstTripCount ? ConstTripCount : 1;
          // When scalarizing to a single lane, iteration replay is driven by
          // the group dimension (LB1). Treat this as a grouped layout even
          // when the tripcount is only known dynamically, so indexing uses LC1.
          UseGroupedDims = true;
          ForceScalarLane = true;
          RemarkLayoutKind = "scalar-replay";
        }
        RemarkForceScalarLane = ForceScalarLane;
        RemarkLaneCount = LaneCount;
        RemarkGroupCount = GroupCount;
        // Local scratch slots keyed by lc1 are only safe when the replay/group
        // extent is statically bounded. Dynamic scalar-replay still uses lc1
        // for addressing, so route those cases through invariant slots instead
        // of allocating undersized TS-backed scratch.
        const bool HasBoundedGroupedScratch =
            UseGroupedDims && HasConstTripCount;
        if (NeedsExecMaskSaveRestore) {
          RemarkCFStrategy = "exec-mask-save-restore-required";
        } else if (NeedsActiveReplay) {
          RemarkCFStrategy = "active-replay";
        } else if (!IfConvertibleSplits.empty()) {
          RemarkCFStrategy = "if-converted-diamond";
        } else if (IsSingleBlock && HasSelect) {
          RemarkCFStrategy = "if-converted-single-block";
        } else if (IsSingleBlock) {
          RemarkCFStrategy = "straight-line-single-block";
        } else {
          RemarkCFStrategy = "body-cfg";
        }

        // Recurrences are supported for both single-block and multi-block
        // loops. Bring-up still carries grouped recurrence state through the
        // scalar mirror path until the TS-backed carrier is runtime-stable.

        (void)StoreObjByInst;
        (void)StoreObjects;

        // Reject loop bodies that compute values used outside the loop,
        // except for recognized reduction values.
        for (BasicBlock *BB : L->blocks()) {
          for (Instruction &I : *BB) {
            if (isa<BranchInst>(I) || isa<ICmpInst>(I) || isa<PHINode>(I))
              continue;
            if (isa<StoreInst>(I))
              continue;
            for (User *U : I.users()) {
              auto *UI = dyn_cast<Instruction>(U);
              if (!UI)
                continue;
              if (!L->contains(UI)) {
                // Values that only flow to exit PHIs are handled via the
                // exit-phi lowering (stored on the exit edge + loaded in the
                // launch block). Do not treat them as generic live-outs.
                if (auto *PN = dyn_cast<PHINode>(UI)) {
                  if (PN->getParent() == Exit)
                    continue;
                }
                if (AllowedLiveOutValues.contains(&I))
                  continue;
                Type *Ty = I.getType();
                if (!Ty->isFloatTy() &&
                    !(Ty->isIntegerTy() && Ty->getScalarSizeInBits() <= 32)) {
                  reject("value_live_out_unsupported_type");
                  return false;
                }
                if (LiveOutInstSet.insert(&I).second)
                  LiveOutInsts.push_back(&I);
                break;
              }
            }
          }
        }

        DenseMap<const SCEV *, Value *> ExpandedStarts;

        static constexpr unsigned kMaxVBlockBinds = 12;
        SmallVector<Value *, 6> BindVals;
        DenseMap<Value *, unsigned> BindIndex;
        auto bindI64 = [&](Value *V) -> std::optional<unsigned> {
          if (!V)
            return std::nullopt;
          if (V->getType() != I64Ty)
            return std::nullopt;
          auto It = BindIndex.find(V);
          if (It != BindIndex.end())
            return It->second;
          if (BindVals.size() >= kMaxVBlockBinds)
            return std::nullopt;
          unsigned Idx = BindVals.size();
          BindVals.push_back(V);
          BindIndex[V] = Idx;
          return Idx;
        };

        static constexpr uint64_t kMaxSIMTLocalWords = 1024u;
        uint64_t LocalScratchWordCount = 0;
        auto reserveLocalWords =
            [&](uint64_t Words) -> std::optional<uint64_t> {
          if (Words == 0)
            return std::nullopt;
          if (Words > kMaxSIMTLocalWords - LocalScratchWordCount)
            return std::nullopt;
          const uint64_t Base = LocalScratchWordCount;
          LocalScratchWordCount += Words;
          return Base;
        };

        const bool ActiveSlotPerLane = NeedsActiveReplay && LaneCount > 1;
        const uint64_t ActiveSlotElems =
            ActiveSlotPerLane ? (LaneCount * GroupCount) : 1u;
        std::optional<unsigned> ActiveSlotBind;
        std::optional<uint64_t> ActiveSlotLocalWordBase;
        if (NeedsActiveReplay)
          ActiveSlotLocalWordBase = reserveLocalWords(ActiveSlotElems);
        if (NeedsActiveReplay) {
          if (!ActiveSlotLocalWordBase) {
            BasicBlock &EntryBB = F.getEntryBlock();
            Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
            IRBuilder<> EB(EntryIP);
            auto *ActiveSlot =
                EB.CreateAlloca(I32Ty, ConstantInt::get(I32Ty, ActiveSlotElems),
                                "linx.simt.active");
            for (uint64_t Elem = 0; Elem < ActiveSlotElems; ++Elem) {
              Value *ElemPtr = EB.CreateInBoundsGEP(
                  I32Ty, ActiveSlot, ConstantInt::get(I32Ty, Elem));
              EB.CreateStore(ConstantInt::get(I32Ty, 1), ElemPtr);
            }
            Value *SlotI64 = PB.CreatePtrToInt(ActiveSlot, I64Ty);
            auto Bind = bindI64(SlotI64);
            if (!Bind) {
              reject("active_bind_exhausted");
              return false;
            }
            ActiveSlotBind = *Bind;
          }
        }

        DenseMap<BasicBlock *, SmallVector<std::pair<unsigned, Value *>, 4>>
            ExitPhiStoresByBlock;
        if (ExitHasPhi) {
          BasicBlock &EntryBB = F.getEntryBlock();
          Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
          IRBuilder<> EB(EntryIP);

          for (Instruction &I : *Exit) {
            auto *Phi = dyn_cast<PHINode>(&I);
            if (!Phi)
              break;

            Type *Ty = Phi->getType();
            if (!Ty->isFloatTy() &&
                !(Ty->isIntegerTy() && Ty->getScalarSizeInBits() <= 32)) {
              reject("exit_phi_unsupported_type");
              return false;
            }

            // Exit PHIs in a post-exit merge block typically have one incoming
            // from the latch's loopexit edge (normal loop completion) and one
            // or more from internal exits (break/search/goto). For internal
            // exits, the incoming block may not be part of the natural loop
            // (it may sit just outside the loop), so attribute those incoming
            // values to the unique in-loop predecessor of that block.

            auto findUniqueInLoopPred = [&](BasicBlock *BB) -> BasicBlock * {
              if (!BB)
                return nullptr;
              BasicBlock *Unique = nullptr;
              for (BasicBlock *P : predecessors(BB)) {
                if (!P || !L->contains(P))
                  continue;
                if (Unique && Unique != P)
                  return nullptr;
                Unique = P;
              }
              return Unique;
            };

            Value *InitV = nullptr;
            SmallVector<std::pair<BasicBlock *, Value *>, 8> PendingStores;
            for (unsigned In = 0; In < Phi->getNumIncomingValues(); ++In) {
              BasicBlock *PredBB = Phi->getIncomingBlock(In);
              if (!PredBB)
                continue;
              Value *VIn = Phi->getIncomingValue(In);

              BasicBlock *KeyBB = nullptr;
              if (L->contains(PredBB)) {
                KeyBB = PredBB;
              } else {
                KeyBB = findUniqueInLoopPred(PredBB);
              }

              if (!KeyBB)
                continue;

              if (KeyBB == Latch && !InitV) {
                InitV = VIn;
                continue;
              }

              if (KeyBB != Latch) {
                PendingStores.push_back(std::make_pair(KeyBB, VIn));
              }
            }

            if (!InitV) {
              // Fallback: use any non-loop incoming as the initial value.
              for (unsigned In = 0; In < Phi->getNumIncomingValues(); ++In) {
                BasicBlock *PredBB = Phi->getIncomingBlock(In);
                if (!PredBB || L->contains(PredBB))
                  continue;
                InitV = Phi->getIncomingValue(In);
                break;
              }
            }
            if (!InitV) {
              reject("exit_phi_no_init_incoming");
              return false;
            }

            if (!isa<Constant>(InitV)) {
              auto *II = dyn_cast<Instruction>(InitV);
              if (!II || (II->getParent() != Preheader &&
                          II->getParent() != &F.getEntryBlock())) {
                reject("exit_phi_init_not_dominating");
                return false;
              }
            }
            auto *Slot = EB.CreateAlloca(Ty, nullptr, "linx.simt.exitphi");
            PB.CreateStore(InitV, Slot);

            Value *SlotI64 = PB.CreatePtrToInt(Slot, I64Ty);
            auto Bind = bindI64(SlotI64);
            if (!Bind) {
              reject("exit_phi_bind_exhausted");
              return false;
            }

            for (auto &Pair : PendingStores) {
              BasicBlock *KeyBB = Pair.first;
              Value *VIn = Pair.second;
              if (!KeyBB || KeyBB == Latch)
                continue;
              ExitPhiStoresByBlock[KeyBB].push_back(std::make_pair(*Bind, VIn));
            }

            ExitPhiPlan Plan;
            Plan.Phi = Phi;
            Plan.Slot = Slot;
            Plan.SlotBind = *Bind;
            if (HasBoundedGroupedScratch && !NeedsActiveReplay)
              Plan.LocalWordBase = reserveLocalWords(LaneCount * GroupCount);
            ExitPhiPlans.push_back(std::move(Plan));

            // Note: incoming values for blocks keyed above cover all
            // non-latch internal exits. We intentionally do not attempt to
            // emit latch-completion stores (those values are represented by
            // InitV above).
          }
        }

        auto getOrCreateF32InductionPlan =
            [&](CastInst *Cast) -> std::optional<unsigned> {
          if (!Cast)
            return std::nullopt;
          auto It = F32InductionPlanByCast.find(Cast);
          if (It != F32InductionPlanByCast.end())
            return It->second;

          // Scalar-lane replay only: the plan carries a single scalar value
          // across "iterations" (groups). In grouped-lane mode, per-lane
          // values would diverge (we would need an int->float conversion op).
          if (LaneCount != 1)
            return std::nullopt;

          if (Cast->getType() != Type::getFloatTy(Ctx))
            return std::nullopt;
          const unsigned Opc = Cast->getOpcode();
          if (Opc != Instruction::SIToFP && Opc != Instruction::UIToFP)
            return std::nullopt;

          Value *Src = Cast->getOperand(0);
          while (auto *CI = dyn_cast_or_null<CastInst>(Src)) {
            switch (CI->getOpcode()) {
            case Instruction::Trunc:
            case Instruction::ZExt:
            case Instruction::SExt:
              Src = CI->getOperand(0);
              continue;
            default:
              break;
            }
            break;
          }
          if (!Src || !Src->getType()->isIntegerTy())
            return std::nullopt;

          const SCEV *S = SE.getSCEVAtScope(Src, L);
          const auto *AR = dyn_cast<SCEVAddRecExpr>(S);
          if (!AR || AR->getLoop() != L || !AR->isAffine())
            return std::nullopt;
          const auto *StartC = dyn_cast<SCEVConstant>(AR->getStart());
          const auto *StepC = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
          if (!StartC || !StepC)
            return std::nullopt;
          const int64_t StartI = StartC->getAPInt().getSExtValue();
          const int64_t StepI = StepC->getAPInt().getSExtValue();
          if (StepI == 0)
            return std::nullopt;

          BasicBlock &EntryBB = F.getEntryBlock();
          Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
          IRBuilder<> EB(EntryIP);

          F32InductionPlan Plan;
          Plan.Cast = Cast;
          Plan.Start = StartI;
          Plan.Step = StepI;
          Plan.Slot =
              EB.CreateAlloca(Type::getFloatTy(Ctx), nullptr, "linx.simt.fiv");
          PB.CreateStore(ConstantFP::get(Type::getFloatTy(Ctx), (double)StartI),
                         Plan.Slot);
          Value *SlotI64 = PB.CreatePtrToInt(Plan.Slot, I64Ty);
          auto Bind = bindI64(SlotI64);
          if (!Bind)
            return std::nullopt;
          Plan.SlotBind = *Bind;

          const unsigned Idx = F32InductionPlans.size();
          F32InductionPlans.push_back(std::move(Plan));
          F32InductionPlanByCast[Cast] = Idx;
          return Idx;
        };

        DenseMap<const PHINode *, unsigned> RecurrencePlanByPhi;
        DenseMap<const Instruction *, SmallVector<unsigned, 2>>
            RecurrencePlansByUpdate;
        if (!RecurrencePlans.empty()) {
          BasicBlock &EntryBB = F.getEntryBlock();
          Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
          IRBuilder<> EB(EntryIP);
          for (unsigned RI = 0; RI < RecurrencePlans.size(); RI++) {
            RecurrencePlan &Plan = RecurrencePlans[RI];
            if (!Plan.SlotTy) {
              reject("invalid_recurrence_slot_type");
              return false;
            }
            const uint64_t RecurrenceSlotElems =
                (LaneCount > 1) ? LaneCount
                                : ((GroupCount != 0) ? (1u + GroupCount) : 1u);
            Plan.Slot = EB.CreateAlloca(
                Plan.SlotTy, ConstantInt::get(I32Ty, RecurrenceSlotElems),
                "linx.simt.rec");
            Value *InitStored = Plan.InitValue;
            if (!InitStored) {
              reject("invalid_recurrence_init");
              return false;
            }
            if (InitStored->getType() != Plan.SlotTy) {
              if (InitStored->getType()->isIntegerTy() &&
                  Plan.SlotTy->isIntegerTy()) {
                InitStored = PB.CreateZExtOrTrunc(InitStored, Plan.SlotTy);
              } else if (InitStored->getType()->isFloatTy() &&
                         Plan.SlotTy->isFloatTy()) {
                InitStored = PB.CreateFPCast(InitStored, Plan.SlotTy);
              } else {
                reject("invalid_recurrence_init_cast");
                return false;
              }
            }
            PB.CreateStore(InitStored, Plan.Slot);
            Value *SlotI64 = PB.CreatePtrToInt(Plan.Slot, I64Ty);
            auto Bind = bindI64(SlotI64);
            if (!Bind) {
              reject("recurrence_bind_exhausted");
              return false;
            }
            Plan.SlotBind = *Bind;
            RecurrencePlanByPhi[Plan.Phi] = RI;
            RecurrencePlansByUpdate[Plan.Update].push_back(RI);
          }
        }

        struct LiveOutPlan {
          Instruction *Inst = nullptr;
          AllocaInst *Slot = nullptr;
          unsigned SlotBind = 0;
          std::optional<uint64_t> LocalWordBase;
        };
        SmallVector<LiveOutPlan, 8> LiveOutPlans;
        if (!LiveOutInsts.empty()) {
          BasicBlock &EntryBB = F.getEntryBlock();
          Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
          IRBuilder<> EB(EntryIP);
          for (Instruction *I : LiveOutInsts) {
            if (!I)
              continue;
            LiveOutPlan Plan;
            Plan.Inst = I;
            Plan.Slot =
                EB.CreateAlloca(I->getType(), nullptr, "linx.simt.liveout");
            Value *SlotI64 = PB.CreatePtrToInt(Plan.Slot, I64Ty);
            auto Bind = bindI64(SlotI64);
            if (!Bind) {
              reject("liveout_bind_exhausted");
              return false;
            }
            Plan.SlotBind = *Bind;
            LiveOutPlans.push_back(std::move(Plan));
          }
        }

        if (NeedsActiveReplay) {
          for (ExitPhiPlan &Plan : ExitPhiPlans)
            Plan.LocalWordBase.reset();
        }

        struct AddressBinding {
          unsigned BaseRi;
          int64_t IndexFactor;
          unsigned Shift;
          int64_t StepElems;
        };

        auto bindPtrStartForElem =
            [&](Value *Ptr,
                uint64_t ElemBytes) -> std::optional<AddressBinding> {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          Ptr = Ptr->stripPointerCasts();
          const SCEV *PointerExpr = SE.getSCEVAtScope(Ptr, L);
          const auto *AddRec = dyn_cast<SCEVAddRecExpr>(PointerExpr);
          if (!AddRec || AddRec->getLoop() != L || !AddRec->isAffine())
            return std::nullopt;
          const auto *StepConst =
              dyn_cast<SCEVConstant>(AddRec->getStepRecurrence(SE));
          if (!StepConst)
            return std::nullopt;
          int64_t StepBytes = StepConst->getAPInt().getSExtValue();
          if ((StepBytes % static_cast<int64_t>(ElemBytes)) != 0 ||
              StepBytes == 0)
            return std::nullopt;
          const int64_t StepElems = StepBytes / static_cast<int64_t>(ElemBytes);

          const SCEV *Start = AddRec->getStart();
          Value *StartV = ExpandedStarts.lookup(Start);
          if (!StartV) {
            StartV = Exp.expandCodeFor(Start, Start->getType(),
                                       Preheader->getTerminator());
            if (!StartV)
              return std::nullopt;
            ExpandedStarts[Start] = StartV;
          }
          Value *BaseI64 = nullptr;
          if (StartV->getType()->isPointerTy()) {
            BaseI64 = PB.CreatePtrToInt(StartV, I64Ty);
          } else if (StartV->getType()->isIntegerTy()) {
            BaseI64 = PB.CreateZExtOrTrunc(StartV, I64Ty);
          } else {
            return std::nullopt;
          }
          auto BaseOpt = bindI64(BaseI64);
          if (!BaseOpt)
            return std::nullopt;

          AddressBinding Binding = {/*BaseRi=*/*BaseOpt,
                                    /*IndexFactor=*/0,
                                    /*Shift=*/0,
                                    /*StepElems=*/StepElems};
          const int64_t Delta = StepBytes - static_cast<int64_t>(ElemBytes);
          if (Delta == 0)
            return Binding;

          const uint64_t AbsDelta = Delta < 0 ? static_cast<uint64_t>(-Delta)
                                              : static_cast<uint64_t>(Delta);
          const unsigned Shift = countr_zero(AbsDelta);
          if (Shift > 31)
            return std::nullopt;

          const int64_t Factor = Delta >> Shift;
          if (Factor == 0 || Factor > 4096 || Factor < -4096)
            return std::nullopt;
          Binding.IndexFactor = Factor;
          Binding.Shift = Shift;
          return Binding;
        };

        struct VecPipeToken {
          unsigned Class = 0;
          unsigned Index = 0;
        };

        auto parseVecPipeToken =
            [&](StringRef Tok) -> std::optional<VecPipeToken> {
          Tok = Tok.trim();
          if (Tok.size() < 4)
            return std::nullopt;
          unsigned Class = 0;
          if (Tok.starts_with("vt#"))
            Class = 0;
          else if (Tok.starts_with("vu#"))
            Class = 1;
          else if (Tok.starts_with("vm#"))
            Class = 2;
          else if (Tok.starts_with("vn#"))
            Class = 3;
          else
            return std::nullopt;
          StringRef Tail = Tok.drop_front(3);
          unsigned Index = 0;
          if (Tail.getAsInteger(10, Index) || Index == 0)
            return std::nullopt;
          return VecPipeToken{Class, Index};
        };

        auto formatVecPipeToken = [&](const VecPipeToken &Tok) {
          static constexpr const char *kClassPrefix[] = {"vt#", "vu#", "vm#",
                                                         "vn#"};
          return std::string(kClassPrefix[Tok.Class]) +
                 std::to_string(Tok.Index);
        };

        auto formatVecPipeHead = [&](const VecPipeToken &Tok,
                                     StringRef Suffix) {
          static constexpr const char *kHeadPrefix[] = {"vt", "vu", "vm", "vn"};
          return std::string(kHeadPrefix[Tok.Class]) + Suffix.str();
        };

        unsigned NextVecReg = 0;
        auto allocVec = [&]() -> std::optional<std::string> {
          static constexpr unsigned kMaxIndex = 31;
          static constexpr unsigned kNumClasses = 4;
          if (NextVecReg >= (kMaxIndex * kNumClasses))
            return std::nullopt;
          const unsigned Class = NextVecReg / kMaxIndex;
          const unsigned Index = (NextVecReg % kMaxIndex) + 1u;
          ++NextVecReg;
          return formatVecPipeToken(VecPipeToken{Class, Index});
        };

        unsigned NextAsmLabel = 0;
        auto freshAsmLabel = [&](StringRef Prefix) -> std::string {
          std::string S;
          raw_string_ostream SS(S);
          SS << Prefix << NextAsmLabel++;
          return SS.str();
        };

        auto isLaneCounterToken = [](StringRef Tok) {
          Tok = Tok.trim();
          return Tok == "lc0" || Tok == "lc1";
        };

        auto formatPipeDest = [&](StringRef Tok, StringRef Suffix) {
          Tok = Tok.trim();
          auto PipeTok = parseVecPipeToken(Tok);
          if (!PipeTok)
            return Tok.str();
          return formatVecPipeHead(*PipeTok, Suffix);
        };

        auto formatIntSrc = [&](StringRef Tok) {
          Tok = Tok.trim();
          if (parseVecPipeToken(Tok))
            return (Tok + ".sw").str();
          if (isLaneCounterToken(Tok))
            return (Tok + ".uh").str();
          return Tok.str();
        };

        auto formatFloatSrc = [&](StringRef Tok) {
          Tok = Tok.trim();
          if (parseVecPipeToken(Tok))
            return (Tok + ".fs").str();
          if (isLaneCounterToken(Tok))
            return (Tok + ".uh").str();
          return Tok.str();
        };

        auto formatMaskSrc = [&](StringRef Tok) {
          Tok = Tok.trim();
          if (parseVecPipeToken(Tok))
            return (Tok + ".ud").str();
          if (isLaneCounterToken(Tok))
            return (Tok + ".uh").str();
          return Tok.str();
        };

        auto formatWordDest = [&](StringRef Tok) {
          return formatPipeDest(Tok, ".w");
        };

        auto formatMaskDest = [&](StringRef Tok) {
          return formatPipeDest(Tok, ".d");
        };

        auto formatAssignedDest = [&](StringRef Tok, StringRef Suffix) {
          Tok = Tok.trim();
          if (parseVecPipeToken(Tok))
            return (Tok + Suffix).str();
          return Tok.str();
        };

        auto formatAssignedWordDest = [&](StringRef Tok) {
          return formatAssignedDest(Tok, ".w");
        };

        DenseMap<BasicBlock *, std::string> ReplayMaskSaveRegByBranch;
        DenseMap<BasicBlock *, SmallVector<std::string, 2>>
            ReplayMaskRestoreRegsByMerge;
        std::optional<unsigned> ExecMaskSaveOneBind;
        if (!ReplayMaskSplits.empty()) {
          ExecMaskSaveOneBind = bindI64(ConstantInt::get(I64Ty, 1));
          if (!ExecMaskSaveOneBind) {
            reject("exec_mask_bind_exhausted");
            return false;
          }
          for (const auto &It : ReplayMaskSplits) {
            auto SaveReg = allocVec();
            if (!SaveReg) {
              reject("vector_reg_exhausted");
              return false;
            }
            ReplayMaskSaveRegByBranch[It.first] = *SaveReg;
            ReplayMaskRestoreRegsByMerge[It.second.MergeBB].push_back(*SaveReg);
          }
        }

        auto formatAddrExpr = [&](StringRef Expr) {
          Expr = Expr.trim();
          const size_t ShiftPos = Expr.find("<<");
          StringRef Base =
              ShiftPos == StringRef::npos ? Expr : Expr.substr(0, ShiftPos);
          StringRef Shift =
              ShiftPos == StringRef::npos ? StringRef() : Expr.substr(ShiftPos);
          Base = Base.trim();
          std::string Out;
          if (parseVecPipeToken(Base))
            Out = (Base + ".sw").str();
          else if (isLaneCounterToken(Base))
            Out = (Base + ".uh").str();
          else
            Out = Base.str();
          if (!Shift.empty())
            Out += Shift.str();
          return Out;
        };

        auto formatShiftedAddr = [&](StringRef Tok, unsigned Shift) {
          std::string Expr = Tok.trim().str();
          if (Shift)
            Expr += "<<" + std::to_string(Shift);
          return formatAddrExpr(Expr);
        };

        struct PtrPhiPlan {
          std::string SelReg; // Small integer selector in a vector register.
          SmallVector<unsigned, 4> BaseRis; // sel_id -> base RI bind
          DenseMap<const BasicBlock *, unsigned> SelByPred; // pred -> sel_id
        };
        DenseMap<const PHINode *, PtrPhiPlan> PtrPhiPlans;
        DenseMap<unsigned, std::string> PendingRecurrenceValues;

        DenseMap<Value *, std::string> ValOp;
        SmallString<512> Body;
        raw_svector_ostream OS(Body);

        std::string LinearIndexReg = "lc0";
        const unsigned GroupShift =
            UseGroupedDims ? static_cast<unsigned>(Log2_64(LaneCount)) : 0u;
        if (UseGroupedDims) {
          auto Lin = allocVec();
          if (!Lin) {
            reject("vector_reg_exhausted");
            return false;
          }
          OS << "  v.add " << formatIntSrc("lc0") << ", "
             << formatAddrExpr(("lc1<<" + std::to_string(GroupShift)).c_str())
             << ", ->" << formatWordDest(*Lin) << "\n";
          LinearIndexReg = *Lin;
        }

        DenseMap<int64_t, std::string> IndexRegByFactor;
        IndexRegByFactor[0] = "zero";
        IndexRegByFactor[1] = LinearIndexReg;
        const bool CacheVecIndexRegs =
            !NeedsActiveReplay && !HasInnerCF && IsSingleBlock;
        std::optional<std::string> NegLc0Reg;
        DenseMap<int64_t, std::string> GroupedIndexRegByStepElems;
        DenseMap<uint64_t, std::string> LocalSlotMemOffsetByWordBase;
        std::optional<std::string> GroupWordIndexReg;

        std::function<std::optional<std::string>(int64_t)> emitScaledLc0 =
            [&](int64_t Factor) -> std::optional<std::string> {
          if (CacheVecIndexRegs) {
            auto Cached = IndexRegByFactor.find(Factor);
            if (Cached != IndexRegByFactor.end())
              return Cached->second;
          }

          if (Factor == -1) {
            auto NegReg = allocVec();
            if (!NegReg)
              return std::nullopt;
            OS << "  v.sub zero, " << formatIntSrc(LinearIndexReg) << ", ->"
               << formatWordDest(*NegReg) << "\n";
            if (CacheVecIndexRegs) {
              IndexRegByFactor[Factor] = *NegReg;
            }
            return *NegReg;
          }

          const bool IsNegative = Factor < 0;
          const uint64_t AbsFactor = IsNegative ? static_cast<uint64_t>(-Factor)
                                                : static_cast<uint64_t>(Factor);
          if (AbsFactor == 0)
            return std::string("zero");
          if (AbsFactor > 4096)
            return std::nullopt;

          DenseMap<unsigned, std::string> Pow2Regs;
          Pow2Regs[0] = LinearIndexReg;
          const unsigned HighestBit = Log2_64(AbsFactor);
          for (unsigned Bit = 1; Bit <= HighestBit; ++Bit) {
            auto Prev = Pow2Regs.find(Bit - 1);
            if (Prev == Pow2Regs.end())
              return std::nullopt;
            auto Next = allocVec();
            if (!Next)
              return std::nullopt;
            OS << "  v.add " << formatIntSrc(Prev->second) << ", "
               << formatIntSrc(Prev->second) << ", ->" << formatWordDest(*Next)
               << "\n";
            Pow2Regs[Bit] = *Next;
          }

          std::optional<std::string> AccumReg;
          for (unsigned Bit = 0; Bit <= HighestBit; ++Bit) {
            if (((AbsFactor >> Bit) & 1u) == 0)
              continue;
            auto Part = Pow2Regs.find(Bit);
            if (Part == Pow2Regs.end())
              return std::nullopt;
            if (!AccumReg) {
              AccumReg = Part->second;
              continue;
            }
            auto Sum = allocVec();
            if (!Sum)
              return std::nullopt;
            OS << "  v.add " << formatIntSrc(*AccumReg) << ", "
               << formatIntSrc(Part->second) << ", ->" << formatWordDest(*Sum)
               << "\n";
            AccumReg = *Sum;
          }

          if (!AccumReg)
            return std::nullopt;

          if (IsNegative) {
            auto NegReg = allocVec();
            if (!NegReg)
              return std::nullopt;
            OS << "  v.sub zero, " << formatIntSrc(*AccumReg) << ", ->"
               << formatWordDest(*NegReg) << "\n";
            AccumReg = *NegReg;
          }

          if (CacheVecIndexRegs)
            IndexRegByFactor[Factor] = *AccumReg;
          return *AccumReg;
        };

        auto emitGroupedIndexReg =
            [&](int64_t StepElems) -> std::optional<std::string> {
          if (CacheVecIndexRegs) {
            auto Cached = GroupedIndexRegByStepElems.find(StepElems);
            if (Cached != GroupedIndexRegByStepElems.end())
              return Cached->second;
          }

          auto StepScaled = emitScaledLc0(StepElems);
          if (!StepScaled)
            return std::nullopt;
          auto Idx = allocVec();
          if (!Idx)
            return std::nullopt;
          OS << "  v.sub " << formatIntSrc(*StepScaled) << ", "
             << formatIntSrc("lc0") << ", ->" << formatWordDest(*Idx) << "\n";
          if (CacheVecIndexRegs) {
            GroupedIndexRegByStepElems[StepElems] = *Idx;
          }
          return *Idx;
        };

        auto emitGroupWordIndexReg = [&]() -> std::optional<std::string> {
          if (!UseGroupedDims)
            return std::nullopt;
          if (CacheVecIndexRegs && GroupWordIndexReg)
            return GroupWordIndexReg;
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          if (GroupShift == 0) {
            OS << "  v.add zero, " << formatIntSrc("lc1") << ", ->"
               << formatWordDest(*Dst) << "\n";
          } else {
            OS << "  v.add zero, "
               << formatAddrExpr(("lc1<<" + std::to_string(GroupShift)).c_str())
               << ", ->" << formatWordDest(*Dst) << "\n";
          }
          if (CacheVecIndexRegs) {
            GroupWordIndexReg = *Dst;
          }
          return *Dst;
        };

        auto emitLocalSlotMemOffset =
            [&](uint64_t WordBase) -> std::optional<std::string> {
          if (CacheVecIndexRegs) {
            auto Cached = LocalSlotMemOffsetByWordBase.find(WordBase);
            if (Cached != LocalSlotMemOffsetByWordBase.end())
              return Cached->second;
          }

          if (WordBase == 0) {
            std::string Tok = UseGroupedDims
                                  ? ("lc1<<" + std::to_string(GroupShift + 2u))
                                  : "zero<<2";
            if (CacheVecIndexRegs)
              LocalSlotMemOffsetByWordBase[WordBase] = Tok;
            return Tok;
          }

          auto ConstBind = bindI64(ConstantInt::get(I64Ty, WordBase));
          if (!ConstBind)
            return std::nullopt;

          if (!UseGroupedDims) {
            std::string Tok = "ri" + std::to_string(*ConstBind) + "<<2";
            if (CacheVecIndexRegs)
              LocalSlotMemOffsetByWordBase[WordBase] = Tok;
            return Tok;
          }

          auto GroupReg = emitGroupWordIndexReg();
          if (!GroupReg)
            return std::nullopt;
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  v.add " << formatIntSrc(*GroupReg) << ", ri" << *ConstBind
             << ", ->" << formatWordDest(*Dst) << "\n";
          std::string Tok = *Dst + "<<2";
          if (CacheVecIndexRegs) {
            LocalSlotMemOffsetByWordBase[WordBase] = Tok;
          }
          return Tok;
        };

        auto emitNegLc0 = [&]() -> std::optional<std::string> {
          if (CacheVecIndexRegs && NegLc0Reg)
            return NegLc0Reg;
          auto Neg = allocVec();
          if (!Neg)
            return std::nullopt;
          OS << "  v.sub zero, " << formatIntSrc("lc0") << ", ->"
             << formatWordDest(*Neg) << "\n";
          if (CacheVecIndexRegs) {
            NegLc0Reg = *Neg;
          }
          return *Neg;
        };

        auto emitIndexDeltaFromLc0 =
            [&](StringRef IndexExpr) -> std::optional<std::string> {
          StringRef Expr = IndexExpr.trim();
          if (Expr == "lc0")
            return std::string("zero");
          if (Expr == "zero")
            return emitNegLc0();
          auto Delta = allocVec();
          if (!Delta)
            return std::nullopt;
          OS << "  v.sub " << formatIntSrc(Expr) << ", " << formatIntSrc("lc0")
             << ", ->" << formatWordDest(*Delta) << "\n";
          return *Delta;
        };

        // Convert a byte-based induction/index expression (e.g. i8 GEP index)
        // into an element index suitable for v.l*/v.s* addressing.
        auto emitElemIndexFromByteIndex =
            [&](Value *ByteIndex,
                uint64_t ElemBytes) -> std::optional<std::string> {
          if (!ByteIndex)
            return std::nullopt;
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          if (!ByteIndex->getType()->isIntegerTy() ||
              ByteIndex->getType()->getScalarSizeInBits() > 64) {
            return std::nullopt;
          }

          const SCEV *IS = SE.getSCEVAtScope(ByteIndex, L);
          const auto *AR = dyn_cast<SCEVAddRecExpr>(IS);
          if (!AR || AR->getLoop() != L || !AR->isAffine())
            return std::nullopt;
          const auto *StartC = dyn_cast<SCEVConstant>(AR->getStart());
          const auto *StepC = dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
          if (!StartC || !StepC)
            return std::nullopt;

          const int64_t StartB = StartC->getAPInt().getSExtValue();
          const int64_t StepB = StepC->getAPInt().getSExtValue();
          const int64_t ElemStride = static_cast<int64_t>(ElemBytes);
          if ((StartB % ElemStride) != 0 || (StepB % ElemStride) != 0)
            return std::nullopt;

          const int64_t StartElems = StartB / ElemStride;
          const int64_t StepElems = StepB / ElemStride;
          if (StepElems == 0)
            return std::nullopt;
          if (StepElems > 4096 || StepElems < -4096)
            return std::nullopt;

          std::optional<std::string> ScaledIndex;
          if (StepElems == 1) {
            ScaledIndex = LinearIndexReg;
          } else {
            ScaledIndex = emitScaledLc0(StepElems);
          }
          if (!ScaledIndex)
            return std::nullopt;

          if (StartElems == 0)
            return *ScaledIndex;

          auto *C64 = ConstantInt::get(I64Ty, (uint64_t)StartElems);
          auto Bind = bindI64(C64);
          if (!Bind)
            return std::nullopt;
          std::string StartTok = "ri" + std::to_string(*Bind);

          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  v.add " << formatIntSrc(*ScaledIndex) << ", " << StartTok
             << ", ->" << formatWordDest(*Dst) << "\n";
          return *Dst;
        };

        std::function<std::optional<std::string>(Value *)> emitValue;
        std::function<std::optional<std::string>(Value *)> emitCondition;
        std::function<std::optional<std::string>(Value *)> emitF32;

        auto emitIntegerAffineAddRecValue =
            [&](Value *IV, bool EdgeFresh) -> std::optional<std::string> {
          if (!IV || !IV->getType()->isIntegerTy() ||
              IV->getType()->getScalarSizeInBits() > 64) {
            return std::nullopt;
          }

          if (!EdgeFresh) {
            auto It = ValOp.find(IV);
            if (It != ValOp.end())
              return It->second;
          }

          const SCEV *PS = SE.getSCEVAtScope(IV, L);
          const auto *AR = dyn_cast<SCEVAddRecExpr>(PS);
          if (!AR || AR->getLoop() != L || !AR->isAffine()) {
            return std::nullopt;
          }
          const SCEV *StartS = AR->getStart();
          const SCEV *StepS = AR->getStepRecurrence(SE);
          if (!StartS || !StepS)
            return std::nullopt;

          // Prefer constant-step lowering when available; fall back to a
          // vector multiply for dynamic step values.
          std::optional<int64_t> StepConst;
          if (auto *StepC = dyn_cast<SCEVConstant>(StepS)) {
            StepConst = StepC->getAPInt().getSExtValue();
            if (*StepConst == 0)
              return std::nullopt;
            if (*StepConst > 4096 || *StepConst < -4096)
              StepConst.reset();
          }

          std::optional<std::string> ScaledIndex;
          if (StepConst) {
            if (*StepConst == 1) {
              ScaledIndex = LinearIndexReg;
            } else {
              ScaledIndex = emitScaledLc0(*StepConst);
            }
          } else {
            Value *StepV = Exp.expandCodeFor(StepS, StepS->getType(),
                                             Preheader->getTerminator());
            if (!StepV)
              return std::nullopt;
            if (!StepV->getType()->isIntegerTy())
              return std::nullopt;
            if (StepV->getType()->getScalarSizeInBits() > 64)
              return std::nullopt;
            if (StepV->getType() != I64Ty)
              StepV = PB.CreateSExtOrTrunc(StepV, I64Ty);
            auto StepTok = emitValue(StepV);
            if (!StepTok)
              return std::nullopt;
            auto Mul = allocVec();
            if (!Mul)
              return std::nullopt;
            OS << "  v.mul " << formatIntSrc(LinearIndexReg) << ", "
               << formatIntSrc(*StepTok) << ", ->" << formatWordDest(*Mul)
               << "\n";
            ScaledIndex = *Mul;
          }
          if (!ScaledIndex)
            return std::nullopt;

          std::optional<int64_t> StartConst;
          if (auto *StartC = dyn_cast<SCEVConstant>(StartS))
            StartConst = StartC->getAPInt().getSExtValue();

          if (StartConst && *StartConst == 0) {
            if (!EdgeFresh)
              ValOp[IV] = *ScaledIndex;
            return *ScaledIndex;
          }

          std::optional<std::string> StartTok;
          if (StartConst) {
            auto *C64 = ConstantInt::get(I64Ty, (uint64_t)*StartConst);
            auto Bind = bindI64(C64);
            if (!Bind)
              return std::nullopt;
            StartTok = "ri" + std::to_string(*Bind);
          } else {
            Value *StartV = Exp.expandCodeFor(StartS, StartS->getType(),
                                              Preheader->getTerminator());
            if (!StartV)
              return std::nullopt;
            if (!StartV->getType()->isIntegerTy())
              return std::nullopt;
            if (StartV->getType()->getScalarSizeInBits() > 64)
              return std::nullopt;
            if (StartV->getType() != I64Ty)
              StartV = PB.CreateSExtOrTrunc(StartV, I64Ty);
            StartTok = emitValue(StartV);
          }
          if (!StartTok)
            return std::nullopt;

          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  v.add " << formatIntSrc(*ScaledIndex) << ", "
             << formatIntSrc(*StartTok) << ", ->" << formatWordDest(*Dst)
             << "\n";
          if (!EdgeFresh)
            ValOp[IV] = *Dst;
          return *Dst;
        };

        auto bindPtrGeneralForElem = [&](Value *Ptr, uint64_t ElemBytes)
            -> std::optional<std::pair<unsigned, std::string>> {
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return std::nullopt;
          Ptr = Ptr->stripPointerCasts();
          if (auto *GEP = dyn_cast<GEPOperator>(Ptr)) {
            auto Try =
                [&]() -> std::optional<std::pair<unsigned, std::string>> {
              // Accept both the canonical pointer GEP form:
              //   gep <elt>, <ptr>, <idx>
              // and the common global-array form:
              //   gep [N x <elt>], <ptr>, 0, <idx>
              // TSVC frequently uses the latter for global arrays.
              Value *Index = nullptr;
              const unsigned NumIdx = GEP->getNumIndices();
              if (NumIdx == 1) {
                Index = GEP->getOperand(1);
              } else if (NumIdx == 2) {
                auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
                if (!Z || !Z->isZero())
                  return std::nullopt;
                Index = GEP->getOperand(2);
              } else {
                return std::nullopt;
              }

              Type *ElemTy = GEP->getResultElementType();
              if (!ElemTy)
                return std::nullopt;
              const DataLayout &DL = F.getParent()->getDataLayout();
              if (DL.getTypeStoreSize(ElemTy) != ElemBytes)
                return std::nullopt;
              if (!(ElemTy->isIntegerTy() &&
                    ElemTy->getScalarSizeInBits() <= 32) &&
                  !(ElemTy->isFloatTy() && ElemBytes == 4))
                return std::nullopt;

              Value *BasePtr = GEP->getPointerOperand()->stripPointerCasts();
              if (!L->isLoopInvariant(BasePtr))
                return std::nullopt;
              Value *BaseI64 = PB.CreatePtrToInt(BasePtr, I64Ty);
              auto BaseOpt = bindI64(BaseI64);
              if (!BaseOpt)
                return std::nullopt;

              if (!Index || !Index->getType()->isIntegerTy())
                return std::nullopt;
              if (Index->getType()->getScalarSizeInBits() > 64)
                return std::nullopt;

              // Keep loop-variant casts inside the body emission rather than
              // inserting them in the IR preheader (which may not dominate the
              // value definition).
              auto IdxExpr = emitValue(Index);
              if (!IdxExpr)
                return std::nullopt;
              auto DeltaExpr = emitIndexDeltaFromLc0(*IdxExpr);
              if (!DeltaExpr)
                return std::nullopt;
              return std::make_pair(*BaseOpt, *DeltaExpr);
            };

            if (auto Res = Try())
              return Res;
          }

          // Pointer induction variable: accept affine AddRec pointers even
          // when the step is dynamic (e.g. i += inc).
          const SCEV *PtrS = SE.getSCEVAtScope(Ptr, L);
          const auto *AR = dyn_cast<SCEVAddRecExpr>(PtrS);
          if (!AR || AR->getLoop() != L || !AR->isAffine())
            return std::nullopt;

          const SCEV *Start = AR->getStart();
          Value *StartV = ExpandedStarts.lookup(Start);
          if (!StartV) {
            StartV = Exp.expandCodeFor(Start, Start->getType(),
                                       Preheader->getTerminator());
            if (!StartV)
              return std::nullopt;
            ExpandedStarts[Start] = StartV;
          }
          Value *BaseI64 = nullptr;
          if (StartV->getType()->isPointerTy()) {
            BaseI64 = PB.CreatePtrToInt(StartV, I64Ty);
          } else if (StartV->getType()->isIntegerTy()) {
            BaseI64 = PB.CreateZExtOrTrunc(StartV, I64Ty);
          } else {
            return std::nullopt;
          }
          auto BaseOpt = bindI64(BaseI64);
          if (!BaseOpt)
            return std::nullopt;

          const SCEV *StepS = AR->getStepRecurrence(SE);
          if (!StepS)
            return std::nullopt;
          Value *StepBytesV = Exp.expandCodeFor(StepS, StepS->getType(),
                                                Preheader->getTerminator());
          if (!StepBytesV || !StepBytesV->getType()->isIntegerTy())
            return std::nullopt;
          if (StepBytesV->getType() != I64Ty)
            StepBytesV = PB.CreateSExtOrTrunc(StepBytesV, I64Ty);
          Value *StepElemsV = PB.CreateAShr(
              StepBytesV, ConstantInt::get(I64Ty, Log2_64(ElemBytes)));
          auto StepTok = emitValue(StepElemsV);
          if (!StepTok)
            return std::nullopt;
          auto Mul = allocVec();
          auto Idx = allocVec();
          if (!Mul || !Idx)
            return std::nullopt;
          OS << "  v.mul " << formatIntSrc(LinearIndexReg) << ", "
             << formatIntSrc(*StepTok) << ", ->" << formatWordDest(*Mul)
             << "\n";
          OS << "  v.sub " << formatIntSrc(*Mul) << ", " << formatIntSrc("lc0")
             << ", ->" << formatWordDest(*Idx) << "\n";
          return std::make_pair(*BaseOpt, *Idx);
        };

        auto unsupportedValueReason = [&](Value *V) -> std::string {
          if (auto *I = dyn_cast<Instruction>(V)) {
            if (auto *CB = dyn_cast<CallBase>(I)) {
              if (Function *Callee = CB->getCalledFunction()) {
                if (Callee->isIntrinsic())
                  return ("unsupported_value_expr:intrinsic:" +
                          Callee->getName())
                      .str();
                return ("unsupported_value_expr:call:" + Callee->getName())
                    .str();
              }
              return "unsupported_value_expr:indirect_call";
            }
            return ("unsupported_value_expr:" + StringRef(I->getOpcodeName()))
                .str();
          }
          if (isa<Argument>(V))
            return "unsupported_value_expr:arg";
          return "unsupported_value_expr:unknown";
        };

        auto ensureExecMaskSaveOneBind = [&]() -> std::optional<unsigned> {
          if (!ExecMaskSaveOneBind)
            ExecMaskSaveOneBind = bindI64(ConstantInt::get(I64Ty, 1));
          return ExecMaskSaveOneBind;
        };

        auto prefersSignedSIMTLoad = [&](Value *SemanticV) -> bool {
          auto *LI = dyn_cast_or_null<LoadInst>(SemanticV);
          if (!LI)
            return false;
          Type *Ty = LI->getType();
          if (!Ty->isIntegerTy(8) && !Ty->isIntegerTy(16))
            return false;
          bool SawSignedExtUse = false;
          for (User *U : LI->users()) {
            auto *I = dyn_cast<Instruction>(U);
            if (!I)
              return false;
            switch (I->getOpcode()) {
            case Instruction::SExt:
              SawSignedExtUse = true;
              continue;
            case Instruction::ICmp: {
              auto *Cmp = cast<ICmpInst>(I);
              switch (Cmp->getPredicate()) {
              case CmpInst::ICMP_SLT:
              case CmpInst::ICMP_SLE:
              case CmpInst::ICMP_SGT:
              case CmpInst::ICMP_SGE:
                return true;
              case CmpInst::ICMP_EQ:
              case CmpInst::ICMP_NE:
              case CmpInst::ICMP_ULT:
              case CmpInst::ICMP_ULE:
              case CmpInst::ICMP_UGT:
              case CmpInst::ICMP_UGE:
                continue;
              default:
                return false;
              }
            }
            case Instruction::Store:
              if (cast<StoreInst>(I)->getValueOperand() == LI)
                continue;
              return false;
            default:
              return false;
            }
          }
          return SawSignedExtUse;
        };

        auto getSIMTLoadMnemonic = [&](Type *Ty,
                                       Value *SemanticV) -> const char * {
          const bool SignedLoad = prefersSignedSIMTLoad(SemanticV);
          switch (getSIMTMemElemBytes(Ty)) {
          case 1:
            return SignedLoad ? "v.lb.brg" : "v.lbu.brg";
          case 2:
            return SignedLoad ? "v.lh.brg" : "v.lhu.brg";
          case 4:
            return "v.lw.brg";
          default:
            return nullptr;
          }
        };

        std::function<std::optional<std::string>(Value *, Type *, Value *)>
            emitLoadFromPtr;
        emitLoadFromPtr = [&](Value *Ptr, Type *LoadTy,
                              Value *SemanticV) -> std::optional<std::string> {
          const uint64_t ElemBytes = getSIMTMemElemBytes(LoadTy);
          const char *LoadMnemonic = getSIMTLoadMnemonic(LoadTy, SemanticV);
          if (!LoadMnemonic)
            return std::nullopt;
          const unsigned ElemShift = Log2_64(ElemBytes);
          const std::string LaneExpr =
              ElemShift == 0 ? "lc0" : ("lc0<<" + std::to_string(ElemShift));

          auto Address = bindPtrStartForElem(Ptr, ElemBytes);
          unsigned BaseRi = 0;
          std::string IndexReg;
          unsigned IndexShift = 0;
          if (Address) {
            BaseRi = Address->BaseRi;
            if (UseGroupedDims) {
              // In grouped mode, emit the SrcR word-offset term consumed by the
              // relative vector memory form `[SrcL, lc0<<2, SrcR<<2]`.
              // The textual asm is not a scalar base+index syntax; it feeds the
              // block/vector address machinery through the clock-hand/register
              // pipe contract. `emitGroupedIndexReg()` materializes the word
              // offset that preserves the intended linear `(lane +
              // group*lanes)` iteration mapping for both unit and non-unit
              // stride patterns.
              auto Idx = emitGroupedIndexReg(Address->StepElems);
              if (!Idx)
                return std::nullopt;
              IndexReg = *Idx;
              IndexShift = ElemShift;
            } else {
              if (Address->StepElems < 0) {
                auto Idx = emitGroupedIndexReg(Address->StepElems);
                if (!Idx)
                  return std::nullopt;
                IndexReg = *Idx;
                IndexShift = ElemShift;
              } else {
                IndexShift = Address->Shift;
                auto IndexRegOpt = emitScaledLc0(Address->IndexFactor);
                if (!IndexRegOpt)
                  return std::nullopt;
                IndexReg = *IndexRegOpt;
              }
            }
          } else {
            auto General = bindPtrGeneralForElem(Ptr, ElemBytes);
            if (!General) {
              Value *Stripped = Ptr ? Ptr->stripPointerCasts() : nullptr;
              auto *GEP = dyn_cast_or_null<GEPOperator>(Stripped);
              if (GEP) {
                Value *Base = GEP->getPointerOperand()->stripPointerCasts();
                if (auto *Phi = dyn_cast<PHINode>(Base)) {
                  auto PlanIt = PtrPhiPlans.find(Phi);
                  if (PlanIt != PtrPhiPlans.end()) {
                    // Pointer sink PHI: dispatch load based on the selector
                    // written on the incoming edge.
                    Value *Index = nullptr;
                    const unsigned NumIdx = GEP->getNumIndices();
                    if (NumIdx == 1) {
                      Index = GEP->getOperand(1);
                    } else if (NumIdx == 2) {
                      auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
                      if (!Z || !Z->isZero())
                        return std::nullopt;
                      Index = GEP->getOperand(2);
                    } else {
                      return std::nullopt;
                    }
                    if (!Index || !Index->getType()->isIntegerTy() ||
                        Index->getType()->getScalarSizeInBits() > 64) {
                      return std::nullopt;
                    }

                    const DataLayout &DL = F.getParent()->getDataLayout();
                    Type *ElemTy = GEP->getResultElementType();
                    const uint64_t PtrElemBytes =
                        ElemTy ? DL.getTypeStoreSize(ElemTy) : 0;

                    std::optional<std::string> IdxExpr;
                    if (PtrElemBytes == ElemBytes &&
                        (ElemBytes == 1 || ElemBytes == 2 || ElemBytes == 4)) {
                      IdxExpr = emitValue(Index);
                    } else {
                      return std::nullopt;
                    }
                    if (!IdxExpr)
                      return std::nullopt;

                    auto DeltaExpr = emitIndexDeltaFromLc0(*IdxExpr);
                    if (!DeltaExpr)
                      return std::nullopt;
                    auto Dst = allocVec();
                    if (!Dst)
                      return std::nullopt;

                    PtrPhiPlan &Plan = PlanIt->second;
                    if (Plan.BaseRis.empty())
                      return std::nullopt;

                    const std::string EndLbl = freshAsmLabel("L_ptrphi_end");
                    SmallVector<std::pair<std::string, unsigned>, 4> CaseLabels;
                    CaseLabels.reserve(Plan.BaseRis.size());
                    for (unsigned SelId = 0; SelId < Plan.BaseRis.size();
                         ++SelId) {
                      std::string CaseLbl = freshAsmLabel("L_ptrphi_case");
                      CaseLabels.push_back(std::make_pair(CaseLbl, SelId));

                      std::string SelTok = "zero";
                      if (SelId != 0) {
                        auto Tok = emitValue(ConstantInt::get(I64Ty, SelId));
                        if (!Tok)
                          return std::nullopt;
                        SelTok = *Tok;
                      }

                      auto Pred = allocVec();
                      if (!Pred)
                        return std::nullopt;
                      OS << "  v.cmp.eq " << formatIntSrc(Plan.SelReg) << ", "
                         << formatIntSrc(SelTok) << ", ->"
                         << formatMaskDest(*Pred) << "\n";
                      // Reduce ops accumulate into the destination register;
                      // seed our scratch reduce destination before each use.
                      // NOTE: C.MOVR cannot write to a specific t#k entry; it
                      // can only push to `t`/`u` or write a global GPR.
                      OS << "  c.movr zero, ->t\n";
                      OS << "  v.rdor " << formatMaskSrc(*Pred) << ", ->t#1\n";
                      OS << "  b.ne t#1, zero, " << CaseLbl << "\n";
                    }

                    OS << "  j " << CaseLabels.front().first << "\n";
                    for (auto &C : CaseLabels) {
                      const unsigned SelId = C.second;
                      if (SelId >= Plan.BaseRis.size())
                        return std::nullopt;
                      const unsigned Ri = Plan.BaseRis[SelId];
                      OS << C.first << ":\n";
                      OS << "  " << LoadMnemonic << " [ri" << Ri << ", "
                         << formatAddrExpr(LaneExpr) << ", "
                         << formatShiftedAddr(*DeltaExpr, ElemShift) << "], ->"
                         << formatWordDest(*Dst) << "\n";
                      OS << "  j " << EndLbl << "\n";
                    }
                    OS << EndLbl << ":\n";
                    return *Dst;
                  }
                }
              }
              if (GEP && GEP->getNumIndices() == 1) {
                Value *Base = GEP->getPointerOperand()->stripPointerCasts();
                auto *Sel = dyn_cast<SelectInst>(Base);
                if (Sel && Sel->getType()->isPointerTy()) {
                  auto IdxIt = GEP->idx_begin();
                  Value *Index =
                      (IdxIt == GEP->idx_end()) ? nullptr : IdxIt->get();
                  if (Index) {
                    Value *TruePtr = PB.CreateGEP(GEP->getSourceElementType(),
                                                  Sel->getTrueValue(), Index);
                    Value *FalsePtr = PB.CreateGEP(GEP->getSourceElementType(),
                                                   Sel->getFalseValue(), Index);
                    auto Pred = emitCondition(Sel->getCondition());
                    auto TV = emitLoadFromPtr(TruePtr, LoadTy, SemanticV);
                    auto FV = emitLoadFromPtr(FalsePtr, LoadTy, SemanticV);
                    if (Pred && TV && FV) {
                      auto Dst = allocVec();
                      if (!Dst)
                        return std::nullopt;
                      OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                         << formatFloatSrc(*TV) << ", " << formatFloatSrc(*FV)
                         << ", ->" << formatWordDest(*Dst) << "\n";
                      return *Dst;
                    }
                  }
                }
              }
              return std::nullopt;
            }
            BaseRi = General->first;
            IndexReg = General->second;
            IndexShift = ElemShift;
          }
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  " << LoadMnemonic << " [ri" << BaseRi << ", "
             << formatAddrExpr(LaneExpr) << ", "
             << formatShiftedAddr(IndexReg, IndexShift) << "], ->"
             << formatWordDest(*Dst) << "\n";
          return *Dst;
        };

        auto emitLoadFromInvariantPtr =
            [&](Value *Ptr, Type *LoadTy,
                Value *SemanticV) -> std::optional<std::string> {
          if (!Ptr)
            return std::nullopt;
          const uint64_t ElemBytes = getSIMTMemElemBytes(LoadTy);
          const char *LoadMnemonic = getSIMTLoadMnemonic(LoadTy, SemanticV);
          if (!LoadMnemonic)
            return std::nullopt;
          const unsigned ElemShift = Log2_64(ElemBytes);
          const std::string LaneExpr =
              ElemShift == 0 ? "lc0" : ("lc0<<" + std::to_string(ElemShift));
          Value *PtrI64 = PB.CreatePtrToInt(Ptr->stripPointerCasts(), I64Ty);
          auto Base = bindI64(PtrI64);
          if (!Base)
            return std::nullopt;
          auto Neg = emitNegLc0();
          if (!Neg)
            return std::nullopt;
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  " << LoadMnemonic << " [ri" << *Base << ", "
             << formatAddrExpr(LaneExpr) << ", "
             << formatShiftedAddr(*Neg, ElemShift) << "], ->"
             << formatWordDest(*Dst) << "\n";
          return *Dst;
        };

        auto emitLoadFromInvariantBindInto = [&](unsigned BaseRi,
                                                 StringRef Dst) -> bool {
          if (LaneCount == 1u) {
            OS << "  v.lw.brg [ri" << BaseRi << ", " << formatAddrExpr("lc0<<2")
               << ", " << formatAddrExpr("zero<<2") << "], ->"
               << formatAssignedWordDest(Dst) << "\n";
            return true;
          }
          auto Neg = emitNegLc0();
          if (!Neg)
            return false;
          OS << "  v.lw.brg [ri" << BaseRi << ", " << formatAddrExpr("lc0<<2")
             << ", " << formatShiftedAddr(*Neg, 2) << "], ->"
             << formatAssignedWordDest(Dst) << "\n";
          return true;
        };

        auto emitLoadFromInvariantBind =
            [&](unsigned BaseRi) -> std::optional<std::string> {
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          if (!emitLoadFromInvariantBindInto(BaseRi, *Dst))
            return std::nullopt;
          return *Dst;
        };

        auto emitStoreToInvariantBind = [&](StringRef Src, unsigned BaseRi,
                                            bool IsFloat = false) {
          if (LaneCount == 1u) {
            OS << "  v.sw.brg "
               << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src)) << ", [ri"
               << BaseRi << ", " << formatAddrExpr("lc0<<2") << ", "
               << formatAddrExpr("zero<<2") << "]\n";
            return true;
          }
          auto Neg = emitNegLc0();
          if (!Neg)
            return false;
          OS << "  v.sw.brg "
             << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src)) << ", [ri"
             << BaseRi << ", " << formatAddrExpr("lc0<<2") << ", "
             << formatShiftedAddr(*Neg, 2) << "]\n";
          return true;
        };

        auto emitLoadFromLocalWordBaseInto = [&](uint64_t WordBase,
                                                 StringRef Dst) -> bool {
          constexpr uint64_t MaxSImm24Bytes = (1ull << 23) - 1ull;
          if (!UseGroupedDims && (WordBase * 4ull) <= MaxSImm24Bytes) {
            OS << "  v.lwi.u.local [ts, " << formatAddrExpr("lc0<<2") << ", "
               << (WordBase * 4ull) << "], ->" << formatAssignedWordDest(Dst)
               << "\n";
            return true;
          }
          auto Offset = emitLocalSlotMemOffset(WordBase);
          if (!Offset)
            return false;
          OS << "  v.lw.local [ts, " << formatAddrExpr("lc0<<2") << ", "
             << formatAddrExpr(*Offset) << "], ->"
             << formatAssignedWordDest(Dst) << "\n";
          return true;
        };

        auto emitLoadFromLocalWordBase =
            [&](uint64_t WordBase) -> std::optional<std::string> {
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          if (!emitLoadFromLocalWordBaseInto(WordBase, *Dst))
            return std::nullopt;
          return *Dst;
        };

        auto emitSharedLocalWordOffsetReg =
            [&](uint64_t WordBase) -> std::optional<std::string> {
          auto Neg = emitNegLc0();
          if (!Neg)
            return std::nullopt;

          std::optional<std::string> BaseTok;
          if (UseGroupedDims) {
            auto GroupReg = emitGroupWordIndexReg();
            if (!GroupReg)
              return std::nullopt;
            BaseTok = *GroupReg;
          }

          if (WordBase != 0) {
            auto ConstBind = bindI64(ConstantInt::get(I64Ty, WordBase));
            if (!ConstBind)
              return std::nullopt;
            if (!BaseTok) {
              BaseTok = ("ri" + std::to_string(*ConstBind));
            } else {
              auto Sum = allocVec();
              if (!Sum)
                return std::nullopt;
              OS << "  v.add " << formatIntSrc(*BaseTok) << ", ri" << *ConstBind
                 << ", ->" << formatWordDest(*Sum) << "\n";
              BaseTok = *Sum;
            }
          }

          if (!BaseTok)
            return *Neg;

          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          OS << "  v.add " << formatIntSrc(*BaseTok) << ", "
             << formatIntSrc(*Neg) << ", ->" << formatWordDest(*Dst) << "\n";
          return *Dst;
        };

        auto emitLoadFromSharedLocalWordBaseInto = [&](uint64_t WordBase,
                                                       StringRef Dst) -> bool {
          auto Offset = emitSharedLocalWordOffsetReg(WordBase);
          if (!Offset)
            return false;
          OS << "  v.lw.local [ts, " << formatAddrExpr("lc0<<2") << ", "
             << formatShiftedAddr(*Offset, 2) << "], ->"
             << formatAssignedWordDest(Dst) << "\n";
          return true;
        };

        auto emitLoadFromSharedLocalWordBase =
            [&](uint64_t WordBase) -> std::optional<std::string> {
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          if (!emitLoadFromSharedLocalWordBaseInto(WordBase, *Dst))
            return std::nullopt;
          return *Dst;
        };

        auto emitStoreToLocalWordBase = [&](StringRef Src, uint64_t WordBase,
                                            bool IsFloat = false) {
          constexpr uint64_t MaxSImm24Bytes = (1ull << 23) - 1ull;
          if (!UseGroupedDims && (WordBase * 4ull) <= MaxSImm24Bytes) {
            OS << "  v.swi.u.local "
               << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src))
               << ", [ts, " << formatAddrExpr("lc0<<2") << ", "
               << (WordBase * 4ull) << "]\n";
            return true;
          }
          auto Offset = emitLocalSlotMemOffset(WordBase);
          if (!Offset)
            return false;
          OS << "  v.sw.local "
             << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src)) << ", [ts, "
             << formatAddrExpr("lc0<<2") << ", " << formatAddrExpr(*Offset)
             << "]\n";
          return true;
        };

        auto emitStoreToSharedLocalWordBase =
            [&](StringRef Src, uint64_t WordBase, bool IsFloat = false) {
              auto Offset = emitSharedLocalWordOffsetReg(WordBase);
              if (!Offset)
                return false;
              OS << "  v.sw.local "
                 << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src))
                 << ", [ts, " << formatAddrExpr("lc0<<2") << ", "
                 << formatShiftedAddr(*Offset, 2) << "]\n";
              return true;
            };

        auto emitLoadFromActiveBind =
            [&](unsigned BaseRi) -> std::optional<std::string> {
          if (!ActiveSlotPerLane)
            return emitLoadFromInvariantBind(BaseRi);
          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;
          if (UseGroupedDims) {
            auto Idx = emitGroupedIndexReg(1);
            if (!Idx)
              return std::nullopt;
            OS << "  v.lw.brg [ri" << BaseRi << ", " << formatAddrExpr("lc0<<2")
               << ", " << formatShiftedAddr(*Idx, 2) << "], ->"
               << formatWordDest(*Dst) << "\n";
          } else {
            OS << "  v.lw.brg [ri" << BaseRi << ", " << formatAddrExpr("lc0<<2")
               << ", " << formatAddrExpr("zero<<2") << "], ->"
               << formatWordDest(*Dst) << "\n";
          }
          return *Dst;
        };

        auto emitStoreToActiveBind = [&](StringRef Src, unsigned BaseRi,
                                         bool IsFloat = false) {
          if (!ActiveSlotPerLane)
            return emitStoreToInvariantBind(Src, BaseRi, IsFloat);
          if (UseGroupedDims) {
            auto Idx = emitGroupedIndexReg(1);
            if (!Idx)
              return false;
            OS << "  v.sw.brg "
               << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src)) << ", [ri"
               << BaseRi << ", " << formatAddrExpr("lc0<<2") << ", "
               << formatShiftedAddr(*Idx, 2) << "]\n";
          } else {
            OS << "  v.sw.brg "
               << (IsFloat ? formatFloatSrc(Src) : formatIntSrc(Src)) << ", [ri"
               << BaseRi << ", " << formatAddrExpr("lc0<<2") << ", "
               << formatAddrExpr("zero<<2") << "]\n";
          }
          return true;
        };

        auto canScalarizeInvariantLoad = [&](const LoadInst *LI) -> bool {
          if (!L->isLoopInvariant(LI->getPointerOperand())) {
            return false;
          }
          const SCEV *LoadS = SE.getSCEVAtScope(
              SE.getSCEV(const_cast<Value *>(LI->getPointerOperand())), L);
          for (StoreInst *SI : Stores) {
            const SCEV *StoreS = SE.getSCEVAtScope(SI->getPointerOperand(), L);
            if (!SE.isKnownPredicate(CmpInst::ICMP_NE, LoadS, StoreS)) {
              return false;
            }
          }
          return true;
        };

        emitCondition = [&](Value *Cond) -> std::optional<std::string> {
          if (auto *CI = dyn_cast<ConstantInt>(Cond)) {
            if (CI->isZero()) {
              ValOp[Cond] = "zero";
              return "zero";
            }
            auto *C64 = ConstantInt::get(I64Ty, CI->getZExtValue());
            auto Bind = bindI64(C64);
            if (!Bind)
              return std::nullopt;
            std::string Name = "ri" + std::to_string(*Bind);
            ValOp[Cond] = Name;
            return Name;
          }

          auto It = ValOp.find(Cond);
          if (It != ValOp.end())
            return It->second;

          if (auto *SI = dyn_cast<SelectInst>(Cond)) {
            if (!SI->getType()->isIntegerTy(1))
              return std::nullopt;
            auto Pred = emitCondition(SI->getCondition());
            auto TV = emitCondition(SI->getTrueValue());
            auto FV = emitCondition(SI->getFalseValue());
            if (!Pred || !TV || !FV)
              return std::nullopt;
            auto Dst = allocVec();
            if (!Dst)
              return std::nullopt;
            OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
               << formatMaskSrc(*TV) << ", " << formatMaskSrc(*FV) << ", ->"
               << formatMaskDest(*Dst) << "\n";
            ValOp[Cond] = *Dst;
            return *Dst;
          }

          if (auto *Cmp = dyn_cast<ICmpInst>(Cond)) {
            auto Lhs = emitValue(Cmp->getOperand(0));
            auto Rhs = emitValue(Cmp->getOperand(1));
            if (!Lhs || !Rhs)
              return std::nullopt;

            auto Dst = allocVec();
            if (!Dst)
              return std::nullopt;

            StringRef Mn;
            std::string A = formatIntSrc(*Lhs);
            std::string B = formatIntSrc(*Rhs);
            switch (Cmp->getPredicate()) {
            case CmpInst::ICMP_EQ:
              Mn = "v.cmp.eq";
              break;
            case CmpInst::ICMP_NE:
              Mn = "v.cmp.ne";
              break;
            case CmpInst::ICMP_SLT:
              Mn = "v.cmp.lt";
              break;
            case CmpInst::ICMP_SLE:
              Mn = "v.cmp.ge";
              std::swap(A, B);
              break;
            case CmpInst::ICMP_SGT:
              Mn = "v.cmp.lt";
              std::swap(A, B);
              break;
            case CmpInst::ICMP_SGE:
              Mn = "v.cmp.ge";
              break;
            case CmpInst::ICMP_ULT:
              Mn = "v.cmp.ltu";
              break;
            case CmpInst::ICMP_ULE:
              Mn = "v.cmp.geu";
              std::swap(A, B);
              break;
            case CmpInst::ICMP_UGT:
              Mn = "v.cmp.ltu";
              std::swap(A, B);
              break;
            case CmpInst::ICMP_UGE:
              Mn = "v.cmp.geu";
              break;
            default:
              return std::nullopt;
            }

            OS << "  " << Mn << " " << A << ", " << B << ", ->"
               << formatMaskDest(*Dst) << "\n";
            ValOp[Cond] = *Dst;
            return *Dst;
          }

          auto *FCmp = dyn_cast<FCmpInst>(Cond);
          if (!FCmp)
            return std::nullopt;

          auto Lhs = emitValue(FCmp->getOperand(0));
          auto Rhs = emitValue(FCmp->getOperand(1));
          if (!Lhs || !Rhs)
            return std::nullopt;

          auto Dst = allocVec();
          if (!Dst)
            return std::nullopt;

          StringRef Mn;
          std::string A = formatFloatSrc(*Lhs);
          std::string B = formatFloatSrc(*Rhs);
          switch (FCmp->getPredicate()) {
          case CmpInst::FCMP_OEQ:
          case CmpInst::FCMP_UEQ:
            Mn = "v.feq";
            break;
          case CmpInst::FCMP_ONE:
          case CmpInst::FCMP_UNE:
            Mn = "v.fne";
            break;
          case CmpInst::FCMP_OLT:
          case CmpInst::FCMP_ULT:
            Mn = "v.flt";
            break;
          case CmpInst::FCMP_OLE:
          case CmpInst::FCMP_ULE:
            Mn = "v.fge";
            std::swap(A, B);
            break;
          case CmpInst::FCMP_OGT:
          case CmpInst::FCMP_UGT:
            Mn = "v.flt";
            std::swap(A, B);
            break;
          case CmpInst::FCMP_OGE:
          case CmpInst::FCMP_UGE:
            Mn = "v.fge";
            break;
          default:
            return std::nullopt;
          }

          OS << "  " << Mn << " " << A << ", " << B << ", ->"
             << formatMaskDest(*Dst) << "\n";
          ValOp[Cond] = *Dst;
          return *Dst;
        };

        emitF32 = [&](Value *V) -> std::optional<std::string> {
          if (!V)
            return std::nullopt;
          if (V->getType() == Type::getFloatTy(Ctx))
            return emitValue(V);
          if (!V->getType()->isDoubleTy())
            return std::nullopt;

          auto It = ValOp.find(V);
          if (It != ValOp.end())
            return It->second;

          if (auto *Cast = dyn_cast<CastInst>(V)) {
            if (Cast->getOpcode() == Instruction::FPExt &&
                Cast->getOperand(0)->getType() == Type::getFloatTy(Ctx)) {
              auto Tok = emitValue(Cast->getOperand(0));
              if (Tok)
                ValOp[V] = *Tok;
              return Tok;
            }
          }

          if (auto *CF = dyn_cast<ConstantFP>(V)) {
            APFloat F = CF->getValueAPF();
            bool LosesInfo = false;
            F.convert(APFloat::IEEEsingle(), APFloat::rmNearestTiesToEven,
                      &LosesInfo);
            APInt Bits = F.bitcastToAPInt();
            if (Bits.getBitWidth() != 32)
              return std::nullopt;
            const uint64_t U = Bits.getZExtValue();
            if (U == 0) {
              ValOp[V] = "zero";
              return "zero";
            }
            auto *CI = ConstantInt::get(I64Ty, U);
            auto Bind = bindI64(CI);
            if (!Bind)
              return std::nullopt;
            std::string Name = "ri" + std::to_string(*Bind);
            ValOp[V] = Name;
            return Name;
          }

          if (auto *BO = dyn_cast<BinaryOperator>(V)) {
            unsigned Opc = BO->getOpcode();
            if (Opc == Instruction::FAdd || Opc == Instruction::FSub ||
                Opc == Instruction::FMul || Opc == Instruction::FDiv) {
              auto Lhs = emitF32(BO->getOperand(0));
              auto Rhs = emitF32(BO->getOperand(1));
              if (!Lhs || !Rhs)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              StringRef Mn = (Opc == Instruction::FAdd)   ? "v.fadd"
                             : (Opc == Instruction::FSub) ? "v.fsub"
                             : (Opc == Instruction::FMul) ? "v.fmul"
                                                          : "v.fdiv";
              OS << "  " << Mn << " " << formatFloatSrc(*Lhs) << ", "
                 << formatFloatSrc(*Rhs) << ", ->" << formatWordDest(*Dst)
                 << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }
          }

          return std::nullopt;
        };

        emitValue = [&](Value *V) -> std::optional<std::string> {
          if (!V)
            return std::nullopt;

          auto It = ValOp.find(V);
          if (It != ValOp.end())
            return It->second;

          if (auto *PN = dyn_cast<PHINode>(V)) {
            auto RecIt = RecurrencePlanByPhi.find(PN);
            if (RecIt != RecurrencePlanByPhi.end()) {
              const unsigned RecIdx = RecIt->second;
              const RecurrencePlan &Plan = RecurrencePlans[RecIdx];
              if (!Plan.LocalWordBase) {
                auto Dst = emitLoadFromInvariantBind(Plan.SlotBind);
                if (!Dst)
                  return std::nullopt;
                ValOp[V] = *Dst;
                return *Dst;
              }

              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              const std::string BindLabel = freshAsmLabel("L_recurrence_bind");
              const std::string DoneLabel = freshAsmLabel("L_recurrence_done");
              OS << "  v.cmp.eq " << formatIntSrc(LinearIndexReg)
                 << ", zero, ->p\n";
              OS << "  b.nz " << BindLabel << "\n";
              if (!emitLoadFromLocalWordBaseInto(*Plan.LocalWordBase, *Dst))
                return std::nullopt;
              OS << "  j " << DoneLabel << "\n";
              OS << BindLabel << ":\n";
              auto Neg = allocVec();
              if (!Neg)
                return std::nullopt;
              OS << "  v.sub zero, " << formatIntSrc("lc0") << ", ->"
                 << formatWordDest(*Neg) << "\n";
              OS << "  v.lw.brg [ri" << Plan.SlotBind << ", "
                 << formatAddrExpr("lc0<<2") << ", "
                 << formatShiftedAddr(*Neg, 2) << "], ->"
                 << formatAssignedWordDest(*Dst) << "\n";
              OS << DoneLabel << ":\n";
              ValOp[V] = *Dst;
              return *Dst;
            }

            auto tryEmitPhiSelectCsel = [&]() -> std::optional<std::string> {
              if (PN->getNumIncomingValues() != 2)
                return std::nullopt;

              auto tryEmitPhiSelect =
                  [&](BasicBlock *BranchBB,
                      BasicBlock *OtherBB) -> std::optional<std::string> {
                if (!BranchBB || !OtherBB || BranchBB == OtherBB)
                  return std::nullopt;
                auto *BI = dyn_cast<BranchInst>(BranchBB->getTerminator());
                auto *OBI = dyn_cast<BranchInst>(OtherBB->getTerminator());
                if (!BI || !BI->isConditional() || BI->getNumSuccessors() != 2)
                  return std::nullopt;
                if (!OBI || OBI->isConditional() ||
                    OBI->getNumSuccessors() != 1)
                  return std::nullopt;
                if (OBI->getSuccessor(0) != PN->getParent())
                  return std::nullopt;

                const bool TrueToMerge =
                    (BI->getSuccessor(0) == PN->getParent());
                const bool FalseToMerge =
                    (BI->getSuccessor(1) == PN->getParent());
                if (!(TrueToMerge || FalseToMerge))
                  return std::nullopt;

                if (TrueToMerge) {
                  if (BI->getSuccessor(1) != OtherBB)
                    return std::nullopt;
                } else {
                  if (BI->getSuccessor(0) != OtherBB)
                    return std::nullopt;
                }

                Value *VTrue = PN->getIncomingValueForBlock(
                    TrueToMerge ? BranchBB : OtherBB);
                Value *VFalse = PN->getIncomingValueForBlock(
                    TrueToMerge ? OtherBB : BranchBB);
                if (!VTrue || !VFalse || VTrue == PN || VFalse == PN)
                  return std::nullopt;

                auto Pred = emitCondition(BI->getCondition());
                auto TV = emitValue(VTrue);
                auto FV = emitValue(VFalse);
                if (!Pred || !TV || !FV)
                  return std::nullopt;

                auto Dst = allocVec();
                if (!Dst)
                  return std::nullopt;
                const bool IsFloat = PN->getType() == Type::getFloatTy(Ctx);
                OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                   << (IsFloat ? formatFloatSrc(*TV) : formatIntSrc(*TV))
                   << ", "
                   << (IsFloat ? formatFloatSrc(*FV) : formatIntSrc(*FV))
                   << ", ->" << formatWordDest(*Dst) << "\n";
                return *Dst;
              };

              auto tryEmitPhiSelectViaSplit =
                  [&](BasicBlock *TruePred,
                      BasicBlock *FalsePred) -> std::optional<std::string> {
                if (!TruePred || !FalsePred || TruePred == FalsePred)
                  return std::nullopt;
                auto *TPredBI = dyn_cast<BranchInst>(TruePred->getTerminator());
                auto *FPredBI =
                    dyn_cast<BranchInst>(FalsePred->getTerminator());
                if (!TPredBI || !FPredBI)
                  return std::nullopt;
                if (TPredBI->isConditional() || FPredBI->isConditional())
                  return std::nullopt;
                if (TPredBI->getNumSuccessors() != 1 ||
                    FPredBI->getNumSuccessors() != 1)
                  return std::nullopt;
                if (TPredBI->getSuccessor(0) != PN->getParent() ||
                    FPredBI->getSuccessor(0) != PN->getParent())
                  return std::nullopt;

                BasicBlock *BranchBB = TruePred->getSinglePredecessor();
                if (!BranchBB || BranchBB != FalsePred->getSinglePredecessor())
                  return std::nullopt;
                auto *BI = dyn_cast<BranchInst>(BranchBB->getTerminator());
                if (!BI || !BI->isConditional() || BI->getNumSuccessors() != 2)
                  return std::nullopt;
                if (BI->getSuccessor(0) != TruePred ||
                    BI->getSuccessor(1) != FalsePred)
                  return std::nullopt;

                Value *VTrue = PN->getIncomingValueForBlock(TruePred);
                Value *VFalse = PN->getIncomingValueForBlock(FalsePred);
                if (!VTrue || !VFalse || VTrue == PN || VFalse == PN)
                  return std::nullopt;

                auto Pred = emitCondition(BI->getCondition());
                auto TV = emitValue(VTrue);
                auto FV = emitValue(VFalse);
                if (!Pred || !TV || !FV)
                  return std::nullopt;

                auto Dst = allocVec();
                if (!Dst)
                  return std::nullopt;
                const bool IsFloat = PN->getType() == Type::getFloatTy(Ctx);
                OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                   << (IsFloat ? formatFloatSrc(*TV) : formatIntSrc(*TV))
                   << ", "
                   << (IsFloat ? formatFloatSrc(*FV) : formatIntSrc(*FV))
                   << ", ->" << formatWordDest(*Dst) << "\n";
                return *Dst;
              };

              auto tryEmitPhiSelectViaRegion =
                  [&]() -> std::optional<std::string> {
                for (auto &KV : IfConvertibleSplits) {
                  const IfConvertibleSplitInfo &Split = KV.second;
                  if (Split.MergeBB != PN->getParent())
                    continue;
                  if (!Split.BranchBB || !Split.TrueExitBB ||
                      !Split.FalseExitBB)
                    continue;

                  Value *VTrue = PN->getIncomingValueForBlock(Split.TrueExitBB);
                  Value *VFalse =
                      PN->getIncomingValueForBlock(Split.FalseExitBB);
                  if (!VTrue || !VFalse || VTrue == PN || VFalse == PN)
                    continue;

                  auto *BI =
                      dyn_cast<BranchInst>(Split.BranchBB->getTerminator());
                  if (!BI || !BI->isConditional() ||
                      BI->getNumSuccessors() != 2)
                    continue;

                  auto Pred = emitCondition(BI->getCondition());
                  auto TV = emitValue(VTrue);
                  auto FV = emitValue(VFalse);
                  if (!Pred || !TV || !FV)
                    return std::nullopt;

                  auto Dst = allocVec();
                  if (!Dst)
                    return std::nullopt;
                  const bool IsFloat = PN->getType() == Type::getFloatTy(Ctx);
                  OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                     << (IsFloat ? formatFloatSrc(*TV) : formatIntSrc(*TV))
                     << ", "
                     << (IsFloat ? formatFloatSrc(*FV) : formatIntSrc(*FV))
                     << ", ->" << formatWordDest(*Dst) << "\n";
                  return *Dst;
                }
                return std::nullopt;
              };

              BasicBlock *Pred0 = PN->getIncomingBlock(0);
              BasicBlock *Pred1 = PN->getIncomingBlock(1);
              if (!L->contains(Pred0) || !L->contains(Pred1))
                return std::nullopt;

              if (auto Dst = tryEmitPhiSelect(Pred0, Pred1))
                return Dst;
              if (auto Dst = tryEmitPhiSelect(Pred1, Pred0))
                return Dst;
              if (auto Dst = tryEmitPhiSelectViaSplit(Pred0, Pred1))
                return Dst;
              if (auto Dst = tryEmitPhiSelectViaSplit(Pred1, Pred0))
                return Dst;
              if (auto Dst = tryEmitPhiSelectViaRegion())
                return Dst;

              return std::nullopt;
            };

            if (PN->getType() == Type::getFloatTy(Ctx) ||
                (PN->getType()->isIntegerTy() &&
                 PN->getType()->getScalarSizeInBits() <= 64)) {
              if (auto Dst = tryEmitPhiSelectCsel()) {
                ValOp[V] = *Dst;
                return *Dst;
              }
            }

            auto It = ValOp.find(V);
            if (It != ValOp.end())
              return It->second;

            if (PN->getType() == Type::getFloatTy(Ctx)) {
              Value *LoopIncoming = nullptr;
              Value *PreIncoming = nullptr;
              for (unsigned I = 0; I < 2; I++) {
                BasicBlock *IncomingBB = PN->getIncomingBlock(I);
                if (L->contains(IncomingBB)) {
                  if (LoopIncoming)
                    return std::nullopt;
                  LoopIncoming = PN->getIncomingValue(I);
                } else {
                  if (PreIncoming)
                    return std::nullopt;
                  PreIncoming = PN->getIncomingValue(I);
                }
              }
              if (!LoopIncoming || !PreIncoming) {
                return std::nullopt;
              }

              auto *LoopLI = dyn_cast_or_null<LoadInst>(LoopIncoming);
              auto *PreLI = dyn_cast_or_null<LoadInst>(PreIncoming);
              if (!LoopLI || !PreLI || LoopLI->isVolatile() ||
                  LoopLI->isAtomic() || PreLI->isVolatile() ||
                  PreLI->isAtomic()) {
                return std::nullopt;
              }

              const SCEV *PS =
                  SE.getSCEVAtScope(LoopLI->getPointerOperand(), L);
              const auto *AR = dyn_cast<SCEVAddRecExpr>(PS);
              if (!AR || AR->getLoop() != L || !AR->isAffine()) {
                return std::nullopt;
              }
              const auto *StepC =
                  dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
              if (!StepC) {
                return std::nullopt;
              }
              const int64_t StepBytes = StepC->getAPInt().getSExtValue();
              if ((StepBytes % 4) != 0 || StepBytes <= 0) {
                return std::nullopt;
              }
              const SCEV *ExpectedPre =
                  SE.getMinusSCEV(AR->getStart(), AR->getStepRecurrence(SE));
              const SCEV *PreS =
                  SE.getSCEVAtScope(PreLI->getPointerOperand(), L);
              if (!SE.isKnownPredicate(CmpInst::ICMP_EQ, PreS, ExpectedPre)) {
                return std::nullopt;
              }
              Value *AdjPtr =
                  PB.CreateGEP(PB.getInt8Ty(), LoopLI->getPointerOperand(),
                               ConstantInt::get(I64Ty, -StepBytes));
              auto Dst = emitLoadFromPtr(AdjPtr, LoopLI->getType(), LoopLI);
              if (!Dst) {
                return std::nullopt;
              }
              ValOp[V] = *Dst;
              return *Dst;
            }

            if (PN->getType()->isIntegerTy() &&
                PN->getType()->getScalarSizeInBits() <= 64) {
              if (auto Dst =
                      emitIntegerAffineAddRecValue(PN, /*EdgeFresh=*/false))
                return Dst;
            }

            // Loop-invariant PHIs can appear in nested loops (outer IVs). Treat
            // them like any other loop-invariant value and bind them.
            if (L->isLoopInvariant(PN)) {
              if (PN->getType()->isPointerTy()) {
                Value *I64V = PB.CreatePtrToInt(const_cast<Value *>(V), I64Ty);
                auto Bind = bindI64(I64V);
                if (!Bind)
                  return std::nullopt;
                std::string Name = "ri" + std::to_string(*Bind);
                ValOp[V] = Name;
                return Name;
              }
              if (PN->getType()->isIntegerTy() &&
                  PN->getType()->getScalarSizeInBits() <= 64) {
                Value *I64V =
                    PB.CreateZExtOrTrunc(const_cast<Value *>(V), I64Ty);
                auto Bind = bindI64(I64V);
                if (!Bind)
                  return std::nullopt;
                std::string Name = "ri" + std::to_string(*Bind);
                ValOp[V] = Name;
                return Name;
              }
              if (PN->getType() == Type::getFloatTy(Ctx)) {
                auto *Bits32 = PB.CreateBitCast(const_cast<Value *>(V), I32Ty);
                auto *I64V = PB.CreateZExt(Bits32, I64Ty);
                auto Bind = bindI64(I64V);
                if (!Bind)
                  return std::nullopt;
                std::string Name = "ri" + std::to_string(*Bind);
                ValOp[V] = Name;
                return Name;
              }
            }

            return std::nullopt;
          }

          if (auto *CF = dyn_cast<ConstantFP>(V)) {
            APInt Bits = CF->getValueAPF().bitcastToAPInt();
            if (Bits.getBitWidth() != 32)
              return std::nullopt;
            const uint64_t U = Bits.getZExtValue();
            if (U == 0) {
              ValOp[V] = "zero";
              return "zero";
            }
            auto *CI = ConstantInt::get(I64Ty, U);
            auto Bind = bindI64(CI);
            if (!Bind)
              return std::nullopt;
            std::string Name = "ri" + std::to_string(*Bind);
            ValOp[V] = Name;
            return Name;
          }

          if (auto *CI = dyn_cast<ConstantInt>(V)) {
            if (CI->isZero()) {
              ValOp[V] = "zero";
              return "zero";
            }
            auto *C64 = ConstantInt::get(I64Ty, CI->getZExtValue());
            auto Bind = bindI64(C64);
            if (!Bind)
              return std::nullopt;
            std::string Name = "ri" + std::to_string(*Bind);
            ValOp[V] = Name;
            return Name;
          }

          if (auto *CB = dyn_cast<CallBase>(V)) {
            Function *Callee = CB->getCalledFunction();
            if (!Callee)
              return std::nullopt;

            if (Callee->isIntrinsic()) {
              if (CB->getType() != Type::getFloatTy(Ctx))
                return std::nullopt;

              switch (Callee->getIntrinsicID()) {
              case Intrinsic::fmuladd:
              case Intrinsic::fma: {
                auto A = emitValue(CB->getArgOperand(0));
                auto B = emitValue(CB->getArgOperand(1));
                auto C = emitValue(CB->getArgOperand(2));
                if (!A || !B || !C)
                  return std::nullopt;
                auto Mul = allocVec();
                auto Dst = allocVec();
                if (!Mul || !Dst)
                  return std::nullopt;
                OS << "  v.fmul " << formatFloatSrc(*A) << ", "
                   << formatFloatSrc(*B) << ", ->" << formatWordDest(*Mul)
                   << "\n";
                OS << "  v.fadd " << formatFloatSrc(*Mul) << ", "
                   << formatFloatSrc(*C) << ", ->" << formatWordDest(*Dst)
                   << "\n";
                ValOp[V] = *Dst;
                return *Dst;
              }
              default:
                break;
              }
              return std::nullopt;
            }

            // Leaf helper calls supported in bring-up mode.
            if (CB->getType() != Type::getFloatTy(Ctx))
              return std::nullopt;
            StringRef Name = Callee->getName();
            if (Name == "fabsf" && CB->arg_size() == 1) {
              auto Src = emitValue(CB->getArgOperand(0));
              if (!Src)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              OS << "  v.fabs " << formatFloatSrc(*Src) << ", ->"
                 << formatWordDest(*Dst) << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }
            if (Name == "sqrtf" && CB->arg_size() == 1) {
              auto Src = emitValue(CB->getArgOperand(0));
              if (!Src)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              OS << "  v.fsqrt " << formatFloatSrc(*Src) << ", ->"
                 << formatWordDest(*Dst) << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }
            return std::nullopt;
          }

          if (auto *LI = dyn_cast<LoadInst>(V)) {
            if (LI->isVolatile() || LI->isAtomic()) {
              return std::nullopt;
            }

            if (L->isLoopInvariant(LI->getPointerOperand())) {
              if (!LI->getType()->isFloatTy() &&
                  !(LI->getType()->isIntegerTy() &&
                    LI->getType()->getScalarSizeInBits() <= 32)) {
                return std::nullopt;
              }
              if (canScalarizeInvariantLoad(LI)) {
                Value *ScalarLoad =
                    PB.CreateLoad(LI->getType(), LI->getPointerOperand());
                Value *I64V = nullptr;
                if (LI->getType()->isFloatTy()) {
                  auto *Bits32 = PB.CreateBitCast(ScalarLoad, I32Ty);
                  I64V = PB.CreateZExt(Bits32, I64Ty);
                } else {
                  I64V = PB.CreateZExtOrTrunc(ScalarLoad, I64Ty);
                }
                auto Slot = bindI64(I64V);
                if (!Slot) {
                  return std::nullopt;
                }
                std::string Name = "ri" + std::to_string(*Slot);
                ValOp[V] = Name;
                return Name;
              }
              auto Dst = emitLoadFromInvariantPtr(LI->getPointerOperand(),
                                                  LI->getType(), LI);
              if (!Dst) {
                return std::nullopt;
              }
              ValOp[V] = *Dst;
              return *Dst;
            }

            if (!LI->getType()->isFloatTy() &&
                !(LI->getType()->isIntegerTy() &&
                  LI->getType()->getScalarSizeInBits() <= 32))
              return std::nullopt;
            if (auto *SelPtr = dyn_cast<SelectInst>(LI->getPointerOperand())) {
              auto Pred = emitCondition(SelPtr->getCondition());
              auto TV =
                  emitLoadFromPtr(SelPtr->getTrueValue(), LI->getType(), LI);
              auto FV =
                  emitLoadFromPtr(SelPtr->getFalseValue(), LI->getType(), LI);
              if (!Pred || !TV || !FV)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                 << formatFloatSrc(*TV) << ", " << formatFloatSrc(*FV) << ", ->"
                 << formatWordDest(*Dst) << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }

            auto Dst =
                emitLoadFromPtr(LI->getPointerOperand(), LI->getType(), LI);
            if (!Dst)
              return std::nullopt;
            ValOp[V] = *Dst;
            return *Dst;
          }

          if (auto *SI = dyn_cast<SelectInst>(V)) {
            auto Pred = emitCondition(SI->getCondition());
            auto TV = emitValue(SI->getTrueValue());
            auto FV = emitValue(SI->getFalseValue());
            if (!Pred || !TV || !FV)
              return std::nullopt;
            auto Dst = allocVec();
            if (!Dst)
              return std::nullopt;
            const bool IsFloat = SI->getType() == Type::getFloatTy(Ctx);
            OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
               << (IsFloat ? formatFloatSrc(*TV) : formatIntSrc(*TV)) << ", "
               << (IsFloat ? formatFloatSrc(*FV) : formatIntSrc(*FV)) << ", ->"
               << formatWordDest(*Dst) << "\n";
            ValOp[V] = *Dst;
            return *Dst;
          }

          if (auto *Cast = dyn_cast<CastInst>(V)) {
            switch (Cast->getOpcode()) {
            case Instruction::Trunc:
            case Instruction::ZExt:
            case Instruction::SExt:
              return emitValue(Cast->getOperand(0));
            case Instruction::SIToFP:
            case Instruction::UIToFP:
            case Instruction::FPExt:
            case Instruction::FPTrunc: {
              if (Cast->getOpcode() == Instruction::FPExt &&
                  Cast->getOperand(0)->getType() == Type::getFloatTy(Ctx) &&
                  Cast->getType()->isDoubleTy()) {
                // TSVC frequently promotes floats to double due to literal
                // constants (e.g. "/1.9") and truncates back to float.
                // We model the computation in float32 and treat these casts
                // as no-ops in bring-up mode.
                return emitValue(Cast->getOperand(0));
              }
              if (Cast->getOpcode() == Instruction::FPTrunc &&
                  Cast->getType() == Type::getFloatTy(Ctx) &&
                  Cast->getOperand(0)->getType()->isDoubleTy()) {
                auto Tok = emitF32(Cast->getOperand(0));
                if (Tok)
                  ValOp[V] = *Tok;
                return Tok;
              }

              if (Cast->getType() != Type::getFloatTy(Ctx))
                return std::nullopt;
              Value *Src = Cast->getOperand(0);
              if (UseGroupedDims && LaneCount > 1 &&
                  (Cast->getOpcode() == Instruction::SIToFP ||
                   Cast->getOpcode() == Instruction::UIToFP)) {
                if (!Src->getType()->isIntegerTy() ||
                    Src->getType()->getScalarSizeInBits() > 32) {
                  return std::nullopt;
                }
                if (Cast->getOpcode() == Instruction::UIToFP) {
                  const SCEV *SrcS = SE.getSCEVAtScope(Src, L);
                  if (!SE.isKnownNonNegative(SrcS)) {
                    return std::nullopt;
                  }
                }
                auto IntTok = emitValue(Src);
                if (!IntTok)
                  return std::nullopt;
                auto Dst = allocVec();
                if (!Dst)
                  return std::nullopt;
                // The vector convert surface only exposes signed integer
                // lanes. Reuse it for unsigned casts when SCEV proves the
                // source non-negative, which holds for TSVC induction paths.
                OS << "  v.icvtf.sw2fs " << formatIntSrc(*IntTok) << ", ->"
                   << formatWordDest(*Dst) << "\n";
                ValOp[V] = *Dst;
                return *Dst;
              }
              if (!L->isLoopInvariant(Src)) {
                // Support affine int induction to float in scalar-lane replay
                // mode by synthesizing a float induction slot.
                auto PlanIdx = getOrCreateF32InductionPlan(Cast);
                if (!PlanIdx)
                  return std::nullopt;
                const F32InductionPlan &Plan = F32InductionPlans[*PlanIdx];
                auto Tok = emitLoadFromInvariantBind(Plan.SlotBind);
                if (Tok)
                  ValOp[V] = *Tok;
                return Tok;
              }
              Value *Scalar =
                  PB.CreateCast(Cast->getOpcode(), Src, Cast->getType());
              auto *Bits32 = PB.CreateBitCast(Scalar, I32Ty);
              auto *I64V = PB.CreateZExt(Bits32, I64Ty);
              auto Bind = bindI64(I64V);
              if (!Bind)
                return std::nullopt;
              std::string Name = "ri" + std::to_string(*Bind);
              ValOp[V] = Name;
              return Name;
            }
            default:
              break;
            }
          }

          if (auto *UO = dyn_cast<UnaryOperator>(V)) {
            if (UO->getOpcode() == Instruction::FNeg) {
              auto Src = emitValue(UO->getOperand(0));
              if (!Src)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              OS << "  v.fsub zero, " << formatFloatSrc(*Src) << ", ->"
                 << formatWordDest(*Dst) << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }
          }

          if (auto *BO = dyn_cast<BinaryOperator>(V)) {
            unsigned Opc = BO->getOpcode();
            if (Opc == Instruction::FAdd || Opc == Instruction::FSub ||
                Opc == Instruction::FMul || Opc == Instruction::FDiv) {
              if (BO->getType() != Type::getFloatTy(Ctx))
                return std::nullopt;
              auto Lhs = emitValue(BO->getOperand(0));
              auto Rhs = emitValue(BO->getOperand(1));
              if (!Lhs || !Rhs)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              StringRef Mn = (Opc == Instruction::FAdd)   ? "v.fadd"
                             : (Opc == Instruction::FSub) ? "v.fsub"
                             : (Opc == Instruction::FMul) ? "v.fmul"
                                                          : "v.fdiv";
              OS << "  " << Mn << " " << formatFloatSrc(*Lhs) << ", "
                 << formatFloatSrc(*Rhs) << ", ->" << formatWordDest(*Dst)
                 << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }

            if (Opc == Instruction::And) {
              if (!BO->getType()->isIntegerTy() ||
                  BO->getType()->getScalarSizeInBits() > 64)
                return std::nullopt;

              // TSVC frequently masks indices with 0xffffffff/0x7fffffff
              // after phi widening to i64. Treat those masks as no-ops in
              // bring-up mode (they only discard high bits that are known to
              // be zero for in-bounds loop indices).
              ConstantInt *MaskC = dyn_cast<ConstantInt>(BO->getOperand(0));
              Value *Other = BO->getOperand(1);
              if (!MaskC) {
                MaskC = dyn_cast<ConstantInt>(BO->getOperand(1));
                Other = BO->getOperand(0);
              }
              if (!MaskC)
                return std::nullopt;

              const uint64_t Mask = MaskC->getZExtValue();
              if (Mask == 0xffffffffffffffffULL || Mask == 0xffffffffULL ||
                  Mask == 0x7fffffffULL) {
                auto Tok = emitValue(Other);
                if (Tok)
                  ValOp[V] = *Tok;
                return Tok;
              }
              return std::nullopt;
            }

            if (Opc == Instruction::LShr) {
              if (!BO->getType()->isIntegerTy() ||
                  BO->getType()->getScalarSizeInBits() > 64)
                return std::nullopt;
              auto *Sh = dyn_cast<ConstantInt>(BO->getOperand(1));
              if (!Sh)
                return std::nullopt;
              const uint64_t ShiftImm = Sh->getZExtValue();

              // In grouped-lane mode, shifting the canonical IV right by the
              // group shift yields the group index (lc1). This allows us to
              // represent patterns like c[i/2] without needing a vblock shift
              // op.
              if (UseGroupedDims && LaneCount > 1 &&
                  isPowerOf2_64(static_cast<uint64_t>(LaneCount)) &&
                  ShiftImm == GroupShift) {
                const SCEV *XS = SE.getSCEVAtScope(BO->getOperand(0), L);
                const auto *AR = dyn_cast<SCEVAddRecExpr>(XS);
                if (AR && AR->getLoop() == L && AR->isAffine()) {
                  const auto *StartC = dyn_cast<SCEVConstant>(AR->getStart());
                  const auto *StepC =
                      dyn_cast<SCEVConstant>(AR->getStepRecurrence(SE));
                  if (StartC && StepC && StartC->getAPInt().isZero() &&
                      StepC->getAPInt().getSExtValue() == 1) {
                    ValOp[V] = "lc1";
                    return std::string("lc1");
                  }
                }
              }
              return std::nullopt;
            }

            if (Opc == Instruction::Add || Opc == Instruction::Sub ||
                Opc == Instruction::Mul) {
              if (!BO->getType()->isIntegerTy() ||
                  BO->getType()->getScalarSizeInBits() > 64)
                return std::nullopt;
              auto Lhs = emitValue(BO->getOperand(0));
              auto Rhs = emitValue(BO->getOperand(1));
              if (!Lhs || !Rhs)
                return std::nullopt;
              auto Dst = allocVec();
              if (!Dst)
                return std::nullopt;
              StringRef Mn = (Opc == Instruction::Add)   ? "v.add"
                             : (Opc == Instruction::Sub) ? "v.sub"
                                                         : "v.mul";
              OS << "  " << Mn << " " << formatIntSrc(*Lhs) << ", "
                 << formatIntSrc(*Rhs) << ", ->" << formatWordDest(*Dst)
                 << "\n";
              ValOp[V] = *Dst;
              return *Dst;
            }
          }

          if (L->isLoopInvariant(V)) {
            if (V->getType()->isPointerTy()) {
              Value *I64V = PB.CreatePtrToInt(const_cast<Value *>(V), I64Ty);
              auto Bind = bindI64(I64V);
              if (!Bind)
                return std::nullopt;
              std::string Name = "ri" + std::to_string(*Bind);
              ValOp[V] = Name;
              return Name;
            }
            if (V->getType()->isIntegerTy() &&
                V->getType()->getScalarSizeInBits() <= 64) {
              Value *I64V = PB.CreateZExtOrTrunc(const_cast<Value *>(V), I64Ty);
              auto Bind = bindI64(I64V);
              if (!Bind)
                return std::nullopt;
              std::string Name = "ri" + std::to_string(*Bind);
              ValOp[V] = Name;
              return Name;
            }
            if (V->getType() == Type::getFloatTy(Ctx)) {
              auto *Bits32 = PB.CreateBitCast(const_cast<Value *>(V), I32Ty);
              auto *I64V = PB.CreateZExt(Bits32, I64Ty);
              auto Bind = bindI64(I64V);
              if (!Bind)
                return std::nullopt;
              std::string Name = "ri" + std::to_string(*Bind);
              ValOp[V] = Name;
              return Name;
            }
          }

          return std::nullopt;
        };

        auto emitStoreValueToPtr = [&](Value *Ptr, StringRef ValTok,
                                       bool IsFloat,
                                       uint64_t ElemBytes) -> bool {
          if (!Ptr)
            return false;
          if (!isPowerOf2_64(ElemBytes) || ElemBytes == 0 || ElemBytes > 8)
            return false;

          const unsigned ElemShift = Log2_64(ElemBytes);
          const std::string LaneExpr =
              ElemShift == 0 ? "lc0" : ("lc0<<" + std::to_string(ElemShift));
          const char *StoreMnemonic = (ElemBytes == 1)   ? "v.sb.brg"
                                      : (ElemBytes == 2) ? "v.sh.brg"
                                      : (ElemBytes == 4) ? "v.sw.brg"
                                                         : "v.sd.brg";

          auto isVectorValueToken = [&](StringRef Tok) {
            Tok = Tok.trim();
            return parseVecPipeToken(Tok).has_value() ||
                   isLaneCounterToken(Tok);
          };

          std::string StoreTok = ValTok.trim().str();
          if (!isVectorValueToken(StoreTok)) {
            auto Dst = allocVec();
            if (!Dst) {
              reject("vector_reg_exhausted");
              return false;
            }
            OS << "  v.add zero, " << formatIntSrc(StoreTok) << ", ->"
               << formatWordDest(*Dst) << "\n";
            StoreTok = *Dst;
          }

          // Pointer sink PHI store: dispatch by selector written on the
          // incoming edge (classic if/switch sinks in TSVC).
          if (auto *GEP =
                  dyn_cast_or_null<GEPOperator>(Ptr->stripPointerCasts())) {
            Value *Base = GEP->getPointerOperand()->stripPointerCasts();
            if (auto *Phi = dyn_cast<PHINode>(Base)) {
              auto PlanIt = PtrPhiPlans.find(Phi);
              if (PlanIt != PtrPhiPlans.end()) {
                Value *Index = nullptr;
                const unsigned NumIdx = GEP->getNumIndices();
                if (NumIdx == 1) {
                  Index = GEP->getOperand(1);
                } else if (NumIdx == 2) {
                  auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
                  if (!Z || !Z->isZero()) {
                    reject("unsupported_ptr_phi_store_gep");
                    return false;
                  }
                  Index = GEP->getOperand(2);
                } else {
                  reject("unsupported_ptr_phi_store_gep");
                  return false;
                }
                if (!Index || !Index->getType()->isIntegerTy() ||
                    Index->getType()->getScalarSizeInBits() > 64) {
                  reject("unsupported_ptr_phi_store_gep");
                  return false;
                }

                const DataLayout &DL = F.getParent()->getDataLayout();
                Type *ElemTy = GEP->getResultElementType();
                const uint64_t ElemBytes =
                    ElemTy ? DL.getTypeStoreSize(ElemTy) : 0;
                std::optional<std::string> IdxExpr;
                if (ElemBytes == 4) {
                  IdxExpr = emitValue(Index);
                } else if (ElemBytes == 1) {
                  IdxExpr = emitElemIndexFromByteIndex(Index, /*ElemBytes=*/4);
                } else {
                  reject("unsupported_ptr_phi_store_gep");
                  return false;
                }

                if (!IdxExpr) {
                  reject("unsupported_ptr_phi_store_index");
                  return false;
                }
                auto DeltaExpr = emitIndexDeltaFromLc0(*IdxExpr);
                if (!DeltaExpr) {
                  reject("unsupported_ptr_phi_store_index");
                  return false;
                }

                PtrPhiPlan &Plan = PlanIt->second;
                if (Plan.BaseRis.empty()) {
                  reject("invalid_ptr_phi_plan");
                  return false;
                }

                const std::string EndLbl = freshAsmLabel("L_ptrphi_st_end");
                SmallVector<std::pair<std::string, unsigned>, 4> CaseLabels;
                CaseLabels.reserve(Plan.BaseRis.size());
                for (unsigned SelId = 0; SelId < Plan.BaseRis.size(); ++SelId) {
                  std::string CaseLbl = freshAsmLabel("L_ptrphi_st_case");
                  CaseLabels.push_back(std::make_pair(CaseLbl, SelId));

                  std::string SelTok = "zero";
                  if (SelId != 0) {
                    auto Tok = emitValue(ConstantInt::get(I64Ty, SelId));
                    if (!Tok) {
                      reject("ptr_phi_sel_emit_failed");
                      return false;
                    }
                    SelTok = *Tok;
                  }

                  OS << "  v.cmp.eq " << formatIntSrc(Plan.SelReg) << ", "
                     << formatIntSrc(SelTok) << ", ->p\n";
                  OS << "  b.nz " << CaseLbl << "\n";
                }

                OS << "  j " << CaseLabels.front().first << "\n";
                for (auto &C : CaseLabels) {
                  const unsigned SelId = C.second;
                  if (SelId >= Plan.BaseRis.size()) {
                    reject("invalid_ptr_phi_plan");
                    return false;
                  }
                  const unsigned Ri = Plan.BaseRis[SelId];
                  OS << C.first << ":\n";
                  OS << "  " << StoreMnemonic << " "
                     << (IsFloat ? formatFloatSrc(StoreTok)
                                 : formatIntSrc(StoreTok))
                     << ", [ri" << Ri << ", " << formatAddrExpr(LaneExpr)
                     << ", " << formatShiftedAddr(*DeltaExpr, ElemShift)
                     << "]\n";
                  OS << "  j " << EndLbl << "\n";
                }
                OS << EndLbl << ":\n";
                return true;
              }
            }
          }

          auto Address = bindPtrStartForElem(Ptr, ElemBytes);
          unsigned BaseRi = 0;
          std::string IndexReg;
          unsigned StoreShift = 0;
          if (Address) {
            BaseRi = Address->BaseRi;
            if (UseGroupedDims) {
              auto Idx = emitGroupedIndexReg(Address->StepElems);
              if (!Idx) {
                reject("unsupported_store_stride");
                return false;
              }
              IndexReg = *Idx;
              StoreShift = ElemShift;
            } else {
              if (Address->StepElems < 0) {
                auto Idx = emitGroupedIndexReg(Address->StepElems);
                if (!Idx) {
                  reject("unsupported_store_stride");
                  return false;
                }
                IndexReg = *Idx;
                StoreShift = ElemShift;
              } else {
                StoreShift = Address->Shift;
                auto IndexRegOpt = emitScaledLc0(Address->IndexFactor);
                if (!IndexRegOpt) {
                  reject("unsupported_store_stride");
                  return false;
                }
                IndexReg = *IndexRegOpt;
              }
            }
          } else {
            auto General = bindPtrGeneralForElem(Ptr, ElemBytes);
            if (!General) {
              reject("non_affine_store_address");
              return false;
            }
            BaseRi = General->first;
            IndexReg = General->second;
            StoreShift = ElemShift;
          }

          OS << "  " << StoreMnemonic << " "
             << (IsFloat ? formatFloatSrc(StoreTok) : formatIntSrc(StoreTok))
             << ", [ri" << BaseRi << ", " << formatAddrExpr(LaneExpr) << ", "
             << formatShiftedAddr(IndexReg, StoreShift) << "]\n";
          return true;
        };

        auto emitStoreValueToInvariantBaseGEP =
            [&](GEPOperator *GEP, Value *BasePtr, StringRef StoreTok,
                bool IsFloat, uint64_t ElemBytes) -> bool {
          if (!GEP || !BasePtr || !isPowerOf2_64(ElemBytes) || ElemBytes == 0 ||
              ElemBytes > 8)
            return false;

          Value *Index = nullptr;
          bool ByteIndexed = false;
          const unsigned NumIdx = GEP->getNumIndices();
          if (NumIdx == 1) {
            Index = GEP->getOperand(1);
          } else if (NumIdx == 2) {
            auto *Z = dyn_cast<ConstantInt>(GEP->getOperand(1));
            if (!Z || !Z->isZero())
              return false;
            Index = GEP->getOperand(2);
          } else {
            return false;
          }

          auto IndexInfo = matchGEPIndexForElemBytes(GEP, ElemBytes);
          if (!IndexInfo)
            return false;
          Index = IndexInfo->first;
          ByteIndexed = IndexInfo->second;

          BasePtr = BasePtr->stripPointerCasts();
          if (!L->isLoopInvariant(BasePtr))
            return false;

          Value *BaseI64 = PB.CreatePtrToInt(BasePtr, I64Ty);
          auto BaseOpt = bindI64(BaseI64);
          if (!BaseOpt)
            return false;
          std::optional<std::string> IdxExpr =
              ByteIndexed ? emitElemIndexFromByteIndex(Index, ElemBytes)
                          : emitValue(Index);
          if (!IdxExpr)
            return false;
          auto DeltaExpr = emitIndexDeltaFromLc0(*IdxExpr);
          if (!DeltaExpr)
            return false;

          const unsigned ElemShift = Log2_64(ElemBytes);
          const std::string LaneExpr =
              ElemShift == 0 ? "lc0" : ("lc0<<" + std::to_string(ElemShift));
          const char *StoreMnemonic = (ElemBytes == 1)   ? "v.sb.brg"
                                      : (ElemBytes == 2) ? "v.sh.brg"
                                      : (ElemBytes == 4) ? "v.sw.brg"
                                                         : "v.sd.brg";
          OS << "  " << StoreMnemonic << " "
             << (IsFloat ? formatFloatSrc(StoreTok) : formatIntSrc(StoreTok))
             << ", [ri" << *BaseOpt << ", " << formatAddrExpr(LaneExpr) << ", "
             << formatShiftedAddr(*DeltaExpr, ElemShift) << "]\n";
          return true;
        };

        auto emitSelectBaseStoreSplit =
            [&](StoreInst *SI, bool IsFloat,
                uint64_t ElemBytes) -> std::optional<bool> {
          auto SelectGEP =
              matchSelectStorePtr(SI->getPointerOperand(), ElemBytes);
          if (!SelectGEP)
            return std::nullopt;

          Value *Cond = SelectGEP->Cond;
          Value *TrueV = SI->getValueOperand();
          Value *FalseV = SI->getValueOperand();
          if (auto *ValSel = dyn_cast<SelectInst>(SI->getValueOperand())) {
            if (ValSel->getCondition() != Cond)
              return std::nullopt;
            TrueV = ValSel->getTrueValue();
            FalseV = ValSel->getFalseValue();
          }

          auto SaveOneBind = ensureExecMaskSaveOneBind();
          if (!SaveOneBind) {
            reject("exec_mask_bind_exhausted");
            return false;
          }

          auto SaveReg = allocVec();
          if (!SaveReg) {
            reject("vector_reg_exhausted");
            return false;
          }

          auto TrueTok = emitValue(TrueV);
          auto FalseTok = emitValue(FalseV);
          if (!TrueTok || !FalseTok) {
            reject(unsupportedValueReason(SI->getValueOperand()));
            return false;
          }

          std::string TrueLabel = freshAsmLabel("L_selptr_true");
          std::string FalseLabel = freshAsmLabel("L_selptr_false");
          std::string EndLabel = freshAsmLabel("L_selptr_end");

          auto emitExecMaskCompareLocal = [&](StringRef Mnemonic, StringRef Lhs,
                                              StringRef Rhs) {
            OS << "  " << Mnemonic << " " << Lhs << ", " << Rhs << ", ->p\n";
          };
          auto emitBranchOnExecMaskLocal = [&](StringRef T, StringRef F) {
            OS << "  b.nz " << T << "\n";
            if (T != F)
              OS << "  j " << F << "\n";
          };
          auto emitPredicateToExecMaskLocal =
              [&](Value *PredicateExpr) -> bool {
            if (auto *Cmp = dyn_cast<ICmpInst>(PredicateExpr)) {
              auto L = emitValue(Cmp->getOperand(0));
              auto R = emitValue(Cmp->getOperand(1));
              if (!L || !R)
                return false;
              if (Cmp->getOperand(0)->getType()->isIntegerTy(1) ||
                  Cmp->getOperand(1)->getType()->isIntegerTy(1))
                return false;

              StringRef Mn;
              std::string A = formatIntSrc(*L);
              std::string B = formatIntSrc(*R);
              switch (Cmp->getPredicate()) {
              case CmpInst::ICMP_EQ:
                Mn = "v.cmp.eq";
                break;
              case CmpInst::ICMP_NE:
                Mn = "v.cmp.ne";
                break;
              case CmpInst::ICMP_SLT:
                Mn = "v.cmp.lt";
                break;
              case CmpInst::ICMP_SLE:
                Mn = "v.cmp.ge";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_SGT:
                Mn = "v.cmp.lt";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_SGE:
                Mn = "v.cmp.ge";
                break;
              case CmpInst::ICMP_ULT:
                Mn = "v.cmp.ltu";
                break;
              case CmpInst::ICMP_ULE:
                Mn = "v.cmp.geu";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_UGT:
                Mn = "v.cmp.ltu";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_UGE:
                Mn = "v.cmp.geu";
                break;
              default:
                return false;
              }

              emitExecMaskCompareLocal(Mn, A, B);
              return true;
            }

            if (auto *FCmp = dyn_cast<FCmpInst>(PredicateExpr)) {
              auto L = emitValue(FCmp->getOperand(0));
              auto R = emitValue(FCmp->getOperand(1));
              if (!L || !R)
                return false;

              StringRef Mn;
              std::string A = formatFloatSrc(*L);
              std::string B = formatFloatSrc(*R);
              switch (FCmp->getPredicate()) {
              case CmpInst::FCMP_OEQ:
              case CmpInst::FCMP_UEQ:
                Mn = "v.feq";
                break;
              case CmpInst::FCMP_ONE:
              case CmpInst::FCMP_UNE:
                Mn = "v.fne";
                break;
              case CmpInst::FCMP_OLT:
              case CmpInst::FCMP_ULT:
                Mn = "v.flt";
                break;
              case CmpInst::FCMP_OLE:
              case CmpInst::FCMP_ULE:
                Mn = "v.fge";
                std::swap(A, B);
                break;
              case CmpInst::FCMP_OGT:
              case CmpInst::FCMP_UGT:
                Mn = "v.flt";
                std::swap(A, B);
                break;
              case CmpInst::FCMP_OGE:
              case CmpInst::FCMP_UGE:
                Mn = "v.fge";
                break;
              default:
                return false;
              }

              emitExecMaskCompareLocal(Mn, A, B);
              return true;
            }

            auto Pred = emitCondition(PredicateExpr);
            if (!Pred)
              return false;
            emitExecMaskCompareLocal("v.cmp.ne", formatMaskSrc(*Pred), "zero");
            return true;
          };

          OS << "  v.psel p, ri" << *SaveOneBind << ", ->"
             << formatWordDest(*SaveReg) << "\n";
          if (!emitPredicateToExecMaskLocal(Cond)) {
            reject("unsupported_branch_condition");
            return false;
          }
          emitBranchOnExecMaskLocal(TrueLabel, FalseLabel);

          OS << TrueLabel << ":\n";
          if (!emitStoreValueToInvariantBaseGEP(SelectGEP->TrueGEP,
                                                SelectGEP->TrueBase, *TrueTok,
                                                IsFloat, ElemBytes)) {
            reject("non_affine_store_address");
            return false;
          }
          OS << "  j " << EndLabel << "\n";

          OS << FalseLabel << ":\n";
          if (!emitStoreValueToInvariantBaseGEP(SelectGEP->FalseGEP,
                                                SelectGEP->FalseBase, *FalseTok,
                                                IsFloat, ElemBytes)) {
            reject("non_affine_store_address");
            return false;
          }

          OS << EndLabel << ":\n";
          OS << "  v.cmp.ne " << formatIntSrc(*SaveReg) << ", zero, ->p\n";
          return true;
        };

        auto emitStoreInst = [&](StoreInst *SI) -> bool {
          Type *StoreTy = SI->getValueOperand()->getType();
          const bool IsFloat = StoreTy == Type::getFloatTy(Ctx);
          uint64_t ElemBytes = 0;
          if (StoreTy->isIntegerTy(1) || StoreTy->isIntegerTy(8))
            ElemBytes = 1;
          else if (StoreTy->isIntegerTy(16))
            ElemBytes = 2;
          else if (IsFloat || StoreTy->isIntegerTy(32))
            ElemBytes = 4;
          else if (StoreTy->isIntegerTy(64))
            ElemBytes = 8;
          if (ElemBytes == 0) {
            reject("non_float_store_value");
            return false;
          }

          if (auto SelectStore =
                  emitSelectBaseStoreSplit(SI, IsFloat, ElemBytes))
            return *SelectStore;

          auto Val = emitValue(SI->getValueOperand());
          if (!Val) {
            reject(unsupportedValueReason(SI->getValueOperand()));
            return false;
          }
          if (auto *UpdateI = dyn_cast<Instruction>(SI->getValueOperand())) {
            auto RecIt = RecurrencePlansByUpdate.find(UpdateI);
            if (RecIt != RecurrencePlansByUpdate.end()) {
              for (unsigned RecIdx : RecIt->second) {
                if (RecIdx >= RecurrencePlans.size()) {
                  reject("invalid_recurrence_plan");
                  return false;
                }
                PendingRecurrenceValues[RecIdx] = *Val;
              }
            }
          }
          return emitStoreValueToPtr(SI->getPointerOperand(), *Val, IsFloat,
                                     ElemBytes);
        };

        auto emitBodyInstructions = [&](BasicBlock *BB) -> bool {
          for (Instruction &I : *BB) {
            if (isa<PHINode>(I) || isa<BranchInst>(I) || isa<ICmpInst>(I) ||
                isa<FCmpInst>(I))
              continue;

            /*
             * Preserve per-iteration program order for values that feed later
             * stores. Our emitValue() is otherwise demand-driven (triggered by
             * stores), which can accidentally move loads across earlier stores
             * and change semantics for "read-before-write" patterns (e.g. TSVC
             * scalar/array expansion kernels like s1251).
             *
             * Emitting in-order and caching by SSA value keeps the body
             * deterministic without relying on alias analysis.
             */
            if (!isa<StoreInst>(I) && !I.use_empty()) {
              // Preserve program order for side-effecting value computations
              // that can affect memory semantics (loads and float ops feeding
              // stores). Avoid emitting pointer/i64 induction plumbing that is
              // not required for address formation and can generate illegal
              // vector+scalar ops on some loop-rotated forms (e.g. TSVC s1111).
              Type *Ty = I.getType();
              if (Ty->isFloatTy() ||
                  (Ty->isIntegerTy() && Ty->getScalarSizeInBits() <= 32)) {
                (void)emitValue(&I);
              }
            }
            if (auto *SI = dyn_cast<StoreInst>(&I)) {
              if (!emitStoreInst(SI))
                return false;
              continue;
            }
            auto RecIt = RecurrencePlansByUpdate.find(&I);
            if (RecIt == RecurrencePlansByUpdate.end())
              continue;
            auto EmittedVal = emitValue(&I);
            if (!EmittedVal) {
              reject(unsupportedValueReason(&I));
              return false;
            }
            for (unsigned RecIdx : RecIt->second) {
              if (RecIdx >= RecurrencePlans.size()) {
                reject("invalid_recurrence_plan");
                return false;
              }
              PendingRecurrenceValues[RecIdx] = *EmittedVal;
            }
          }
          return true;
        };

        auto emitInnerControlFlowBody = [&]() -> bool {
          // Linearize the loop's inner CFG for one "iteration" of the vblock
          // body. The vblock launch (B.DIM replay) provides loop iteration,
          // so edges to Header are treated as "end of iteration".
          //
          // Use a stable topological order over the acyclic inner CFG so we
          // don't reject structured control flow due to an unlucky traversal
          // order.
          DenseMap<BasicBlock *, unsigned> FuncOrder;
          unsigned FuncIdx = 0;
          for (BasicBlock &BB : F)
            FuncOrder[&BB] = FuncIdx++;

          auto isBodyBlock = [&](BasicBlock *BB) -> bool {
            if (!BB || !L->contains(BB))
              return false;
            if (IfConvertibleRegionBlocks.contains(BB))
              return false;
            // Header is always emitted first. Include the latch block as part
            // of the linearized body so we don't drop iteration-tail side
            // effects (stores/recurrence updates) that are commonly placed in
            // the latch after if/else lowering under -O2.
            if (BB == Header)
              return false;
            return true;
          };

          SmallVector<BasicBlock *, 16> Nodes;
          SmallPtrSet<BasicBlock *, 16> NodeSet;
          Nodes.push_back(Header);
          NodeSet.insert(Header);

          auto addNode = [&](BasicBlock *BB) {
            if (!isBodyBlock(BB))
              return;
            if (NodeSet.insert(BB).second)
              Nodes.push_back(BB);
          };

          auto forEachDiscoveredBodySucc =
              [&](BasicBlock *BB, function_ref<void(BasicBlock *)> Fn) {
                auto SplitIt = IfConvertibleSplits.find(BB);
                if (SplitIt != IfConvertibleSplits.end()) {
                  Fn(SplitIt->second.MergeBB);
                  return;
                }
                auto *TI = BB ? BB->getTerminator() : nullptr;
                if (!TI)
                  return;
                if (auto *BI = dyn_cast<BranchInst>(TI)) {
                  for (unsigned SI = 0; SI < BI->getNumSuccessors(); ++SI)
                    Fn(BI->getSuccessor(SI));
                  return;
                }
                if (auto *SI = dyn_cast<SwitchInst>(TI)) {
                  Fn(SI->getDefaultDest());
                  for (auto Case : SI->cases())
                    Fn(Case.getCaseSuccessor());
                }
              };

          // Discover all blocks reachable from Header within the "iteration"
          // CFG (excluding edges to Header/Latch).
          for (unsigned NI = 0; NI < Nodes.size(); ++NI) {
            BasicBlock *BB = Nodes[NI];
            auto *TI = BB->getTerminator();
            if (!TI) {
              reject("missing_terminator");
              return false;
            }
            if (isa<BranchInst>(TI) || isa<SwitchInst>(TI)) {
              forEachDiscoveredBodySucc(
                  BB, [&](BasicBlock *Succ) { addNode(Succ); });
              continue;
            }
            reject("unsupported_terminator");
            return false;
          }

          auto forEachBodySucc = [&](BasicBlock *BB,
                                     function_ref<void(BasicBlock *)> Fn) {
            auto SplitIt = IfConvertibleSplits.find(BB);
            if (SplitIt != IfConvertibleSplits.end()) {
              BasicBlock *Succ = SplitIt->second.MergeBB;
              if (NodeSet.count(Succ) && Succ != Header)
                Fn(Succ);
              return;
            }
            auto *TI = BB ? BB->getTerminator() : nullptr;
            if (!TI)
              return;
            if (auto *BI = dyn_cast<BranchInst>(TI)) {
              for (unsigned SI = 0; SI < BI->getNumSuccessors(); ++SI) {
                BasicBlock *Succ = BI->getSuccessor(SI);
                if (NodeSet.count(Succ) && Succ != Header)
                  Fn(Succ);
              }
              return;
            }
            if (auto *SI = dyn_cast<SwitchInst>(TI)) {
              BasicBlock *Def = SI->getDefaultDest();
              if (NodeSet.count(Def) && Def != Header)
                Fn(Def);
              for (auto Case : SI->cases()) {
                BasicBlock *Succ = Case.getCaseSuccessor();
                if (NodeSet.count(Succ) && Succ != Header)
                  Fn(Succ);
              }
              return;
            }
          };

          DenseMap<BasicBlock *, unsigned> Indegree;
          for (BasicBlock *BB : Nodes)
            Indegree[BB] = 0;
          for (BasicBlock *BB : Nodes)
            forEachBodySucc(BB, [&](BasicBlock *Succ) { ++Indegree[Succ]; });

          SmallVector<BasicBlock *, 16> Ready;
          for (BasicBlock *BB : Nodes) {
            if (BB == Header)
              continue;
            if (Indegree.lookup(BB) == 0)
              Ready.push_back(BB);
          }

          SmallVector<BasicBlock *, 16> EmitOrder;
          EmitOrder.reserve(Nodes.size());
          EmitOrder.push_back(Header);

          auto pickReady = [&]() -> BasicBlock * {
            unsigned BestI = 0;
            unsigned BestOrder = std::numeric_limits<unsigned>::max();
            for (unsigned I = 0; I < Ready.size(); ++I) {
              BasicBlock *BB = Ready[I];
              unsigned Ord = FuncOrder.lookup(BB);
              if (Ord < BestOrder) {
                BestOrder = Ord;
                BestI = I;
              }
            }
            BasicBlock *BB = Ready[BestI];
            Ready.erase(Ready.begin() + BestI);
            return BB;
          };

          auto process = [&](BasicBlock *BB) {
            forEachBodySucc(BB, [&](BasicBlock *Succ) {
              auto It = Indegree.find(Succ);
              if (It == Indegree.end())
                return;
              if (It->second == 0)
                return;
              if (--It->second == 0 && Succ != Header)
                Ready.push_back(Succ);
            });
          };

          // Seed ready set after processing Header first.
          process(Header);
          while (!Ready.empty()) {
            BasicBlock *BB = pickReady();
            EmitOrder.push_back(BB);
            process(BB);
          }

          if (EmitOrder.size() != Nodes.size()) {
            reject("unsupported_inner_cycle");
            return false;
          }

          DenseMap<BasicBlock *, std::string> Labels;
          DenseMap<BasicBlock *, unsigned> LabelIndex;
          for (unsigned I = 0; I < EmitOrder.size(); ++I)
            Labels[EmitOrder[I]] = ("L" + std::to_string(I));
          for (unsigned I = 0; I < EmitOrder.size(); ++I)
            LabelIndex[EmitOrder[I]] = I;
          const std::string EndLabel = "L_end";

          auto labelForSucc = [&](BasicBlock *Succ) -> std::string {
            if (!Succ || !L->contains(Succ) || Succ == Header)
              return EndLabel;
            auto It = Labels.find(Succ);
            if (It == Labels.end())
              return EndLabel;
            return It->second;
          };

          // Plan inner-CF PHIs: allocate vector registers for scalar value PHIs
          // and create selector plans for pointer PHIs so loads/stores can
          // dispatch to the correct invariant base.
          DenseMap<BasicBlock *, SmallVector<PHINode *, 4>> ValuePhisByBlock;
          DenseMap<BasicBlock *, SmallVector<PHINode *, 2>> PtrPhisByBlock;

          auto planInnerPhis = [&]() -> bool {
            for (BasicBlock *BB : EmitOrder) {
              if (!BB || BB == Header)
                continue;

              for (Instruction &I : *BB) {
                auto *Phi = dyn_cast<PHINode>(&I);
                if (!Phi)
                  break;
                if (Phi->use_empty())
                  continue;

                if (Phi->getType()->isPointerTy()) {
                  if (!PtrPhiPlans.count(Phi)) {
                    PtrPhiPlan Plan;
                    auto Sel = allocVec();
                    if (!Sel) {
                      reject("vector_reg_exhausted");
                      return false;
                    }
                    Plan.SelReg = *Sel;
                    DenseMap<const Value *, unsigned> SelIdByPtr;

                    for (unsigned II = 0, IE = Phi->getNumIncomingValues();
                         II != IE; ++II) {
                      Value *InV = Phi->getIncomingValue(II);
                      BasicBlock *Pred = Phi->getIncomingBlock(II);
                      Value *InBase = InV ? InV->stripPointerCasts() : nullptr;
                      if (!InBase || !InBase->getType()->isPointerTy()) {
                        reject("unsupported_ptr_phi_incoming");
                        return false;
                      }
                      if (!L->isLoopInvariant(InBase)) {
                        reject("unsupported_ptr_phi_variant_incoming");
                        return false;
                      }

                      unsigned SelId = 0;
                      auto Seen = SelIdByPtr.find(InBase);
                      if (Seen != SelIdByPtr.end()) {
                        SelId = Seen->second;
                      } else {
                        Value *I64V = PB.CreatePtrToInt(InBase, I64Ty);
                        auto BaseOpt = bindI64(I64V);
                        if (!BaseOpt) {
                          reject("ptr_phi_bind_exhausted");
                          return false;
                        }
                        SelId = Plan.BaseRis.size();
                        Plan.BaseRis.push_back(*BaseOpt);
                        SelIdByPtr[InBase] = SelId;
                      }
                      Plan.SelByPred[Pred] = SelId;
                    }

                    PtrPhiPlans[Phi] = std::move(Plan);
                  }
                  PtrPhisByBlock[BB].push_back(Phi);
                  continue;
                }

                Type *Ty = Phi->getType();
                if (Ty == Type::getFloatTy(Ctx) ||
                    (Ty->isIntegerTy() && Ty->getScalarSizeInBits() <= 64)) {
                  auto Dst = allocVec();
                  if (!Dst) {
                    reject("vector_reg_exhausted");
                    return false;
                  }
                  ValOp[Phi] = *Dst;
                  ValuePhisByBlock[BB].push_back(Phi);
                  continue;
                }

                reject("unsupported_inner_phi_type");
                return false;
              }
            }
            return true;
          };

          if (!planInnerPhis())
            return false;

          SmallVector<
              std::pair<std::string, std::pair<BasicBlock *, BasicBlock *>>, 16>
              PhiEdgeLabels;

          auto needsPhiEdge = [&](BasicBlock *SuccBB) -> bool {
            if (!SuccBB)
              return false;
            auto VI = ValuePhisByBlock.find(SuccBB);
            if (VI != ValuePhisByBlock.end() && !VI->second.empty())
              return true;
            auto PI = PtrPhisByBlock.find(SuccBB);
            if (PI != PtrPhisByBlock.end() && !PI->second.empty())
              return true;
            return false;
          };

          auto getPhiEdgeLabel = [&](BasicBlock *PredBB,
                                     BasicBlock *SuccBB) -> std::string {
            std::string P = Labels.lookup(PredBB);
            if (P.empty())
              P = "L" + std::to_string(LabelIndex.lookup(PredBB));
            std::string S = Labels.lookup(SuccBB);
            if (S.empty())
              S = "L" + std::to_string(LabelIndex.lookup(SuccBB));
            std::string Lbl = P + "_to_" + S;
            for (auto &E : PhiEdgeLabels) {
              if (E.first == Lbl)
                return Lbl;
            }
            PhiEdgeLabels.push_back(
                std::make_pair(Lbl, std::make_pair(PredBB, SuccBB)));
            return Lbl;
          };

          auto targetLabelForSucc = [&](BasicBlock *PredBB,
                                        BasicBlock *SuccBB) -> std::string {
            std::string Base = labelForSucc(SuccBB);
            if (Base != EndLabel && needsPhiEdge(SuccBB))
              return getPhiEdgeLabel(PredBB, SuccBB);
            return Base;
          };

          auto isVectorToken = [](StringRef Tok) -> bool {
            std::string Lower = Tok.trim().lower();
            StringRef T(Lower);
            return T.starts_with("vt#") || T.starts_with("vu#") ||
                   T.starts_with("vm#") || T.starts_with("vn#") ||
                   T.starts_with("lc") || T == "ta" || T == "tb" || T == "tc" ||
                   T == "td" || T == "to" || T == "ts" || T.starts_with("acc");
          };

          auto emitExecMaskCompare = [&](StringRef Mnemonic, StringRef Lhs,
                                         StringRef Rhs) {
            OS << "  " << Mnemonic << " " << Lhs << ", " << Rhs << ", ->p\n";
          };

          auto emitBranchOnExecMask = [&](StringRef TrueLabel,
                                          StringRef FalseLabel) {
            OS << "  b.nz " << TrueLabel << "\n";
            if (TrueLabel != FalseLabel)
              OS << "  j " << FalseLabel << "\n";
          };

          auto emitPredicateToExecMask = [&](Value *PredicateExpr) -> bool {
            if (auto *Cmp = dyn_cast<ICmpInst>(PredicateExpr)) {
              auto L = emitValue(Cmp->getOperand(0));
              auto R = emitValue(Cmp->getOperand(1));
              if (!L || !R)
                return false;
              if (!isVectorToken(*L) && !isVectorToken(*R))
                return false;
              if (Cmp->getOperand(0)->getType()->isIntegerTy(1) ||
                  Cmp->getOperand(1)->getType()->isIntegerTy(1)) {
                return false;
              }

              StringRef Mn;
              std::string A = formatIntSrc(*L);
              std::string B = formatIntSrc(*R);
              switch (Cmp->getPredicate()) {
              case CmpInst::ICMP_EQ:
                Mn = "v.cmp.eq";
                break;
              case CmpInst::ICMP_NE:
                Mn = "v.cmp.ne";
                break;
              case CmpInst::ICMP_SLT:
                Mn = "v.cmp.lt";
                break;
              case CmpInst::ICMP_SLE:
                Mn = "v.cmp.ge";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_SGT:
                Mn = "v.cmp.lt";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_SGE:
                Mn = "v.cmp.ge";
                break;
              case CmpInst::ICMP_ULT:
                Mn = "v.cmp.ltu";
                break;
              case CmpInst::ICMP_ULE:
                Mn = "v.cmp.geu";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_UGT:
                Mn = "v.cmp.ltu";
                std::swap(A, B);
                break;
              case CmpInst::ICMP_UGE:
                Mn = "v.cmp.geu";
                break;
              default:
                return false;
              }

              emitExecMaskCompare(Mn, A, B);
              return true;
            }

            if (auto *FCmp = dyn_cast<FCmpInst>(PredicateExpr)) {
              auto L = emitValue(FCmp->getOperand(0));
              auto R = emitValue(FCmp->getOperand(1));
              if (!L || !R)
                return false;

              StringRef Mn;
              std::string A = formatFloatSrc(*L);
              std::string B = formatFloatSrc(*R);
              switch (FCmp->getPredicate()) {
              case CmpInst::FCMP_OEQ:
              case CmpInst::FCMP_UEQ:
                Mn = "v.feq";
                break;
              case CmpInst::FCMP_ONE:
              case CmpInst::FCMP_UNE:
                Mn = "v.fne";
                break;
              case CmpInst::FCMP_OLT:
              case CmpInst::FCMP_ULT:
                Mn = "v.flt";
                break;
              case CmpInst::FCMP_OLE:
              case CmpInst::FCMP_ULE:
                Mn = "v.fge";
                std::swap(A, B);
                break;
              case CmpInst::FCMP_OGT:
              case CmpInst::FCMP_UGT:
                Mn = "v.flt";
                std::swap(A, B);
                break;
              case CmpInst::FCMP_OGE:
              case CmpInst::FCMP_UGE:
                Mn = "v.fge";
                break;
              default:
                return false;
              }

              emitExecMaskCompare(Mn, A, B);
              return true;
            }

            auto Pred = emitCondition(PredicateExpr);
            if (!Pred || !isVectorToken(*Pred))
              return false;
            emitExecMaskCompare("v.cmp.ne", formatMaskSrc(*Pred), "zero");
            return true;
          };

          auto emitCondBranch =
              [&](Value *Cond, StringRef TrueLabel, StringRef FalseLabel,
                  StringRef SavedMaskReg = StringRef()) -> bool {
            std::string Mnemonic = "b.ne";
            std::string Lhs;
            std::string Rhs = "zero";
            bool BranchAlreadyEmitted = false;

            auto emitPredicatedBranch = [&](Value *PredicateExpr) -> bool {
              if (emitPredicateToExecMask(PredicateExpr)) {
                if (!SavedMaskReg.empty()) {
                  OS << "  v.psel p, ri" << *ExecMaskSaveOneBind << ", ->"
                     << formatWordDest(SavedMaskReg) << "\n";
                }
                emitBranchOnExecMask(TrueLabel, FalseLabel);
                BranchAlreadyEmitted = true;
                return true;
              }
              auto Pred = emitCondition(PredicateExpr);
              if (!Pred) {
                reject("unsupported_branch_condition");
                return false;
              }
              Lhs = *Pred;
              Mnemonic = "b.ne";
              Rhs = "zero";
              return true;
            };

            if (auto *Cmp = dyn_cast<ICmpInst>(Cond)) {
              auto L = emitValue(Cmp->getOperand(0));
              auto R = emitValue(Cmp->getOperand(1));
              if (!L || !R) {
                reject("unsupported_branch_condition");
                return false;
              }

              if (Cmp->getOperand(0)->getType()->isIntegerTy(1) ||
                  Cmp->getOperand(1)->getType()->isIntegerTy(1)) {
                reject("unsupported_branch_i1_condition");
                return false;
              }

              const bool UsePredicateFallback =
                  isVectorToken(*L) || isVectorToken(*R);
              if (UsePredicateFallback) {
                if (!emitPredicatedBranch(Cond))
                  return false;
              } else {
                Lhs = *L;
                Rhs = *R;
                switch (Cmp->getPredicate()) {
                case CmpInst::ICMP_EQ:
                  Mnemonic = "b.eq";
                  break;
                case CmpInst::ICMP_NE:
                  Mnemonic = "b.ne";
                  break;
                case CmpInst::ICMP_SLT:
                  Mnemonic = "b.lt";
                  break;
                case CmpInst::ICMP_SLE:
                  Mnemonic = "b.ge";
                  std::swap(Lhs, Rhs);
                  break;
                case CmpInst::ICMP_SGT:
                  Mnemonic = "b.lt";
                  std::swap(Lhs, Rhs);
                  break;
                case CmpInst::ICMP_SGE:
                  Mnemonic = "b.ge";
                  break;
                case CmpInst::ICMP_ULT:
                  Mnemonic = "b.ltu";
                  break;
                case CmpInst::ICMP_ULE:
                  Mnemonic = "b.geu";
                  std::swap(Lhs, Rhs);
                  break;
                case CmpInst::ICMP_UGT:
                  Mnemonic = "b.ltu";
                  std::swap(Lhs, Rhs);
                  break;
                case CmpInst::ICMP_UGE:
                  Mnemonic = "b.geu";
                  break;
                default:
                  reject("unsupported_branch_predicate");
                  return false;
                }
              }
            } else if (isa<FCmpInst>(Cond)) {
              if (!emitPredicatedBranch(Cond)) {
                reject("unsupported_branch_fcmp_condition");
                return false;
              }
            } else {
              if (!emitPredicatedBranch(Cond))
                return false;
            }

            if (BranchAlreadyEmitted)
              return true;

            OS << "  " << Mnemonic << " " << Lhs << ", " << Rhs << ", "
               << TrueLabel << "\n";
            if (TrueLabel != FalseLabel)
              OS << "  j " << FalseLabel << "\n";
            return true;
          };

          SmallVector<std::pair<std::string, BasicBlock *>, 8> ExitEdgeLabels;

          auto emitExitEdgeStores = [&](BasicBlock *PredBB) -> bool {
            auto It = ExitPhiStoresByBlock.find(PredBB);
            if (It != ExitPhiStoresByBlock.end()) {
              for (auto &Pair : It->second) {
                unsigned BaseRi = Pair.first;
                Value *VIn = Pair.second;
                auto Tok = emitValue(VIn);
                if (!Tok) {
                  reject("exit_phi_value_emit_failed");
                  return false;
                }
                const ExitPhiPlan *Plan = nullptr;
                for (const ExitPhiPlan &P : ExitPhiPlans) {
                  if (P.SlotBind == BaseRi) {
                    Plan = &P;
                    break;
                  }
                }
                const bool Stored =
                    (Plan && Plan->LocalWordBase)
                        ? emitStoreToSharedLocalWordBase(
                              *Tok, *Plan->LocalWordBase,
                              /*IsFloat=*/VIn->getType() ==
                                  Type::getFloatTy(Ctx))
                        : emitStoreToInvariantBind(*Tok, BaseRi,
                                                   /*IsFloat=*/VIn->getType() ==
                                                       Type::getFloatTy(Ctx));
                if (!Stored) {
                  reject("exit_phi_store_emit_failed");
                  return false;
                }
              }
            }
            if (NeedsActiveReplay &&
                (ActiveSlotBind || ActiveSlotLocalWordBase)) {
              const bool StoredActive =
                  ActiveSlotLocalWordBase
                      ? emitStoreToLocalWordBase("zero",
                                                 *ActiveSlotLocalWordBase)
                      : emitStoreToActiveBind("zero", *ActiveSlotBind);
              if (!StoredActive) {
                reject("active_store_emit_failed");
                return false;
              }
            }
            return true;
          };

          auto getExitEdgeLabel = [&](BasicBlock *PredBB,
                                      unsigned SuccIdx) -> std::string {
            std::string Base = Labels.lookup(PredBB);
            if (Base.empty())
              Base = "L" + std::to_string(LabelIndex.lookup(PredBB));
            std::string Lbl = Base + "_exit" + std::to_string(SuccIdx);
            for (auto &P : ExitEdgeLabels) {
              if (P.first == Lbl)
                return Lbl;
            }
            ExitEdgeLabels.push_back(std::make_pair(Lbl, PredBB));
            return Lbl;
          };

          for (BasicBlock *BB : EmitOrder) {
            if (BB != Header)
              OS << Labels.lookup(BB) << ":\n";
            if (auto RestoreIt = ReplayMaskRestoreRegsByMerge.find(BB);
                RestoreIt != ReplayMaskRestoreRegsByMerge.end()) {
              for (const std::string &SavedMaskReg : RestoreIt->second)
                emitExecMaskCompare("v.cmp.ne", formatIntSrc(SavedMaskReg),
                                    "zero");
            }
            auto StoreMergeIt = IfConvertibleStoreMerges.find(BB);
            if (StoreMergeIt != IfConvertibleStoreMerges.end()) {
              const IfConvertibleStoreMergePlan &Plan = StoreMergeIt->second;
              auto *BI =
                  dyn_cast_or_null<BranchInst>(Plan.BranchBB->getTerminator());
              if (!BI || !BI->isConditional() || BI->getNumSuccessors() != 2) {
                reject("invalid_store_merge_plan");
                return false;
              }
              if (!Plan.TrueStore || !Plan.FalseStore) {
                reject("invalid_store_merge_plan");
                return false;
              }
              if (Plan.TrueStore->getValueOperand()->getType() !=
                      Type::getFloatTy(Ctx) ||
                  Plan.FalseStore->getValueOperand()->getType() !=
                      Type::getFloatTy(Ctx)) {
                reject("non_float_store_value");
                return false;
              }
              auto Pred = emitCondition(BI->getCondition());
              auto TV = emitValue(Plan.TrueStore->getValueOperand());
              auto FV = emitValue(Plan.FalseStore->getValueOperand());
              if (!Pred || !TV || !FV) {
                reject("store_merge_value_emit_failed");
                return false;
              }
              auto Dst = allocVec();
              if (!Dst) {
                reject("vector_reg_exhausted");
                return false;
              }
              OS << "  v.csel " << formatMaskSrc(*Pred) << ", "
                 << formatFloatSrc(*TV) << ", " << formatFloatSrc(*FV) << ", ->"
                 << formatWordDest(*Dst) << "\n";
              if (!emitStoreValueToPtr(Plan.TrueStore->getPointerOperand(),
                                       *Dst, /*IsFloat=*/true,
                                       /*ElemBytes=*/4)) {
                reject("store_merge_emit_failed");
                return false;
              }
            }
            if (!emitBodyInstructions(BB))
              return false;

            auto *TI = BB->getTerminator();
            auto *BI = dyn_cast_or_null<BranchInst>(TI);
            auto *SI = dyn_cast_or_null<SwitchInst>(TI);
            if (!BI && !SI) {
              reject("unsupported_terminator");
              return false;
            }

            if (BI) {
              auto SplitIt = IfConvertibleSplits.find(BB);
              if (SplitIt != IfConvertibleSplits.end()) {
                BasicBlock *MergeBB = SplitIt->second.MergeBB;
                std::string MergeLabel = labelForSucc(MergeBB);
                if (MergeLabel != EndLabel) {
                  auto CurIt = LabelIndex.find(BB);
                  auto MergeIt = LabelIndex.find(MergeBB);
                  if (CurIt != LabelIndex.end() &&
                      MergeIt != LabelIndex.end() &&
                      MergeIt->second <= CurIt->second) {
                    reject("unsupported_inner_backedge");
                    return false;
                  }
                }
                OS << "  j " << MergeLabel << "\n";
                continue;
              }
              if (!BI->isConditional()) {
                BasicBlock *Succ = BI->getSuccessor(0);
                std::string Target = targetLabelForSucc(BB, Succ);
                if (Succ && !L->contains(Succ)) {
                  if (!emitExitEdgeStores(BB))
                    return false;
                }
                if (Target != EndLabel) {
                  auto CurIt = LabelIndex.find(BB);
                  auto SuccIt = LabelIndex.find(Succ);
                  if (CurIt != LabelIndex.end() && SuccIt != LabelIndex.end() &&
                      SuccIt->second <= CurIt->second) {
                    reject("unsupported_inner_backedge");
                    return false;
                  }
                }
                OS << "  j " << Target << "\n";
                continue;
              }

              BasicBlock *S0 = BI->getSuccessor(0);
              BasicBlock *S1 = BI->getSuccessor(1);
              const bool S0InLoop = L->contains(S0);
              const bool S1InLoop = L->contains(S1);

              // Header loop-entry guard is represented by B.DIM replay and is
              // not part of the decoupled body control flow.
              if (BB == Header && (S0InLoop != S1InLoop)) {
                BasicBlock *InLoopSucc = S0InLoop ? S0 : S1;
                if (InLoopSucc && InLoopSucc != Latch)
                  continue;
              }

              std::string TrueLabel = (S0 && !S0InLoop)
                                          ? getExitEdgeLabel(BB, 0)
                                          : targetLabelForSucc(BB, S0);
              std::string FalseLabel = (S1 && !S1InLoop)
                                           ? getExitEdgeLabel(BB, 1)
                                           : targetLabelForSucc(BB, S1);
              if (TrueLabel != EndLabel) {
                auto CurIt = LabelIndex.find(BB);
                auto S0It = LabelIndex.find(S0);
                if (CurIt != LabelIndex.end() && S0It != LabelIndex.end() &&
                    S0It->second <= CurIt->second) {
                  reject("unsupported_inner_backedge");
                  return false;
                }
              }
              if (FalseLabel != EndLabel) {
                auto CurIt = LabelIndex.find(BB);
                auto S1It = LabelIndex.find(S1);
                if (CurIt != LabelIndex.end() && S1It != LabelIndex.end() &&
                    S1It->second <= CurIt->second) {
                  reject("unsupported_inner_backedge");
                  return false;
                }
              }

              if (TrueLabel == FalseLabel) {
                OS << "  j " << TrueLabel << "\n";
                continue;
              }
              StringRef SavedMaskReg;
              if (auto MaskIt = ReplayMaskSaveRegByBranch.find(BB);
                  MaskIt != ReplayMaskSaveRegByBranch.end())
                SavedMaskReg = MaskIt->second;
              if (!emitCondBranch(BI->getCondition(), TrueLabel, FalseLabel,
                                  SavedMaskReg))
                return false;
              continue;
            }

            // SwitchInst: lower as a linear compare chain. In bring-up mode we
            // model SIMT divergence via LaneCount=1, so a scalar branch is
            // sufficient.
            auto CondTok = emitValue(SI->getCondition());
            if (!CondTok) {
              reject("unsupported_switch_condition");
              return false;
            }

            for (auto Case : SI->cases()) {
              auto CaseTok = emitValue(Case.getCaseValue());
              if (!CaseTok) {
                reject("unsupported_switch_case");
                return false;
              }
              BasicBlock *DestBB = Case.getCaseSuccessor();
              std::string DestLabel = targetLabelForSucc(BB, DestBB);
              if (DestLabel != EndLabel) {
                auto CurIt = LabelIndex.find(BB);
                auto DestIt = LabelIndex.find(DestBB);
                if (CurIt != LabelIndex.end() && DestIt != LabelIndex.end() &&
                    DestIt->second <= CurIt->second) {
                  reject("unsupported_inner_backedge");
                  return false;
                }
              }
              OS << "  v.cmp.eq " << formatIntSrc(*CondTok) << ", "
                 << formatIntSrc(*CaseTok) << ", ->p\n";
              OS << "  b.nz " << DestLabel << "\n";
            }
            std::string DefaultLabel =
                targetLabelForSucc(BB, SI->getDefaultDest());
            OS << "  j " << DefaultLabel << "\n";
          }

          // Emit phi-edge labels (PHI copies + ptr-phi selector writes)
          // before the exit-edge labels so we can branch to them from
          // within the linearized body.
          for (auto &P : PhiEdgeLabels) {
            OS << P.first << ":\n";
            BasicBlock *PredBB = P.second.first;
            BasicBlock *SuccBB = P.second.second;
            if (!PredBB || !SuccBB) {
              reject("invalid_phi_edge");
              return false;
            }

            auto PI = PtrPhisByBlock.find(SuccBB);
            if (PI != PtrPhisByBlock.end()) {
              for (PHINode *Phi : PI->second) {
                auto PlanIt = PtrPhiPlans.find(Phi);
                if (PlanIt == PtrPhiPlans.end()) {
                  reject("missing_ptr_phi_plan");
                  return false;
                }
                PtrPhiPlan &Plan = PlanIt->second;
                auto SelIt = Plan.SelByPred.find(PredBB);
                if (SelIt == Plan.SelByPred.end()) {
                  reject("missing_ptr_phi_edge");
                  return false;
                }
                const unsigned SelId = SelIt->second;
                std::string SelTok = "zero";
                if (SelId != 0) {
                  auto Tok = emitValue(ConstantInt::get(I64Ty, SelId));
                  if (!Tok) {
                    reject("ptr_phi_sel_emit_failed");
                    return false;
                  }
                  SelTok = *Tok;
                }
                OS << "  v.add zero, " << formatIntSrc(SelTok) << ", ->"
                   << formatAssignedWordDest(Plan.SelReg) << "\n";
              }
            }

            auto VI = ValuePhisByBlock.find(SuccBB);
            if (VI != ValuePhisByBlock.end()) {
              for (PHINode *Phi : VI->second) {
                int Idx = Phi->getBasicBlockIndex(PredBB);
                if (Idx < 0) {
                  reject("missing_phi_incoming");
                  return false;
                }
                Value *InV = Phi->getIncomingValue(Idx);
                std::optional<std::string> SrcTok;
                bool NeedsEdgeFresh = false;
                if (InV->getType()->isIntegerTy() &&
                    InV->getType()->getScalarSizeInBits() <= 64) {
                  const SCEV *InS = SE.getSCEVAtScope(InV, L);
                  const auto *AR = dyn_cast<SCEVAddRecExpr>(InS);
                  if (AR && AR->getLoop() == L && AR->isAffine()) {
                    NeedsEdgeFresh = true;
                    SrcTok =
                        emitIntegerAffineAddRecValue(InV, /*EdgeFresh=*/true);
                  }
                }
                if (NeedsEdgeFresh && !SrcTok) {
                  reject("phi_incoming_addrec_emit_failed");
                  return false;
                }
                if (!SrcTok)
                  SrcTok = emitValue(InV);
                if (!SrcTok) {
                  reject("phi_incoming_emit_failed");
                  return false;
                }
                auto DIt = ValOp.find(Phi);
                if (DIt == ValOp.end()) {
                  reject("missing_phi_reg");
                  return false;
                }
                const bool IsFloat = Phi->getType() == Type::getFloatTy(Ctx);
                OS << "  v.add "
                   << (IsFloat ? formatFloatSrc(*SrcTok)
                               : formatIntSrc(*SrcTok))
                   << ", zero, ->" << formatAssignedWordDest(DIt->second)
                   << "\n";
              }
            }

            OS << "  j " << labelForSucc(SuccBB) << "\n";
          }

          // Emit exit-edge labels (stores + active=0) before the
          // end-of-iteration label so we can branch to them from within the
          // linearized body.
          for (auto &P : ExitEdgeLabels) {
            OS << P.first << ":\n";
            if (!emitExitEdgeStores(P.second))
              return false;
            OS << "  j " << EndLabel << "\n";
          }

          OS << EndLabel << ":\n";
          return true;
        };

        if (ActiveSlotLocalWordBase) {
          auto OneTok = emitValue(ConstantInt::get(I64Ty, 1));
          if (!OneTok ||
              !emitStoreToLocalWordBase(*OneTok, *ActiveSlotLocalWordBase)) {
            reject("active_init_emit_failed");
            return false;
          }
        }

        for (const ExitPhiPlan &Plan : ExitPhiPlans) {
          if (!Plan.LocalWordBase)
            continue;
          auto InitTok = emitLoadFromInvariantBind(Plan.SlotBind);
          if (!InitTok ||
              !emitStoreToSharedLocalWordBase(*InitTok, *Plan.LocalWordBase,
                                              /*IsFloat=*/Plan.Phi &&
                                                  Plan.Phi->getType() ==
                                                      Type::getFloatTy(Ctx))) {
            reject("exit_phi_store_emit_failed");
            return false;
          }
        }

        const std::string AfterLabel = "L_after";
        if (ActiveSlotBind || ActiveSlotLocalWordBase) {
          auto ActiveTok =
              ActiveSlotLocalWordBase
                  ? emitLoadFromLocalWordBase(*ActiveSlotLocalWordBase)
                  : emitLoadFromActiveBind(*ActiveSlotBind);
          if (!ActiveTok) {
            reject("active_load_failed");
            return false;
          }
          auto Pred = allocVec();
          if (!Pred) {
            reject("vector_reg_exhausted");
            return false;
          }
          OS << "  v.cmp.eq " << formatIntSrc(*ActiveTok) << ", zero, ->"
             << formatMaskDest(*Pred) << "\n";
          // Reduce ops accumulate into the destination register; seed our
          // scratch reduce destination before each use.
          OS << "  c.movr zero, ->t\n";
          OS << "  v.rdor " << formatMaskSrc(*Pred) << ", ->t#1\n";
          OS << "  b.ne t#1, zero, " << AfterLabel << "\n";
        }

        if (IsSingleBlock) {
          if (!emitBodyInstructions(Header))
            return false;
        } else if (HasInnerCF) {
          if (!emitInnerControlFlowBody())
            return false;
        } else {
          for (StoreInst *SI : Stores) {
            if (!emitStoreInst(SI))
              return false;
          }
        }

        for (unsigned RecIdx = 0; RecIdx < RecurrencePlans.size(); RecIdx++) {
          const RecurrencePlan &Plan = RecurrencePlans[RecIdx];
          auto It = PendingRecurrenceValues.find(RecIdx);
          if (It == PendingRecurrenceValues.end()) {
            auto UpdateVal = emitValue(Plan.Update);
            if (!UpdateVal) {
              reject("recurrence_update_not_emitted");
              return false;
            }
            if (Plan.LocalWordBase &&
                !emitStoreToLocalWordBase(*UpdateVal, *Plan.LocalWordBase + 1u,
                                          /*IsFloat=*/true)) {
              reject("recurrence_store_emit_failed");
              return false;
            }
            if (!emitStoreToInvariantBind(*UpdateVal, Plan.SlotBind,
                                          /*IsFloat=*/true)) {
              reject("recurrence_store_emit_failed");
              return false;
            }
            continue;
          }
          if (Plan.LocalWordBase &&
              !emitStoreToLocalWordBase(It->second, *Plan.LocalWordBase + 1u,
                                        /*IsFloat=*/true)) {
            reject("recurrence_store_emit_failed");
            return false;
          }
          if (!emitStoreToInvariantBind(It->second, Plan.SlotBind,
                                        /*IsFloat=*/true)) {
            reject("recurrence_store_emit_failed");
            return false;
          }
        }

        for (unsigned FI = 0; FI < F32InductionPlans.size(); ++FI) {
          const F32InductionPlan &Plan = F32InductionPlans[FI];
          if (!Plan.Cast) {
            reject("invalid_f32_induction_plan");
            return false;
          }
          auto Cur = emitValue(Plan.Cast);
          if (!Cur) {
            reject("f32_induction_not_emitted");
            return false;
          }
          auto StepTok = emitValue(
              ConstantFP::get(Type::getFloatTy(Ctx), (double)Plan.Step));
          if (!StepTok) {
            reject("f32_induction_step_emit_failed");
            return false;
          }
          auto Next = allocVec();
          if (!Next) {
            reject("vector_reg_exhausted");
            return false;
          }
          OS << "  v.fadd " << formatFloatSrc(*Cur) << ", "
             << formatFloatSrc(*StepTok) << ", ->" << formatWordDest(*Next)
             << "\n";
          if (!emitStoreToInvariantBind(*Next, Plan.SlotBind,
                                        /*IsFloat=*/true)) {
            reject("f32_induction_store_emit_failed");
            return false;
          }
        }

        if ((ActiveSlotBind || ActiveSlotLocalWordBase) && NeedsActiveReplay &&
            ActiveContinueCond) {
          auto PredTok = emitCondition(ActiveContinueCond);
          if (!PredTok) {
            reject("active_cond_emit_failed");
            return false;
          }
          std::string PredName = *PredTok;
          if (ActiveContinueInvert) {
            auto Inv = allocVec();
            if (!Inv) {
              reject("vector_reg_exhausted");
              return false;
            }
            OS << "  v.cmp.eq " << formatMaskSrc(PredName) << ", zero, ->"
               << formatMaskDest(*Inv) << "\n";
            PredName = *Inv;
          }
          // Reduce ops accumulate into the destination register; seed our
          // scratch reduce destination before each use.
          OS << "  c.movr zero, ->t\n";
          OS << "  v.rdor " << formatMaskSrc(PredName) << ", ->t#1\n";
          const bool StoredActive =
              ActiveSlotLocalWordBase
                  ? emitStoreToLocalWordBase("t#1", *ActiveSlotLocalWordBase)
                  : emitStoreToActiveBind("t#1", *ActiveSlotBind);
          if (!StoredActive) {
            reject("active_store_emit_failed");
            return false;
          }
        }

        static constexpr const char *kReductionDstRegs[] = {"a0", "a1", "a2",
                                                            "a3", "a4", "a5"};
        if (ReductionPlans.size() >
            (sizeof(kReductionDstRegs) / sizeof(kReductionDstRegs[0]))) {
          reject("too_many_reductions");
          return false;
        }

        if (!ReductionPlans.empty()) {
          BasicBlock &EntryBB = F.getEntryBlock();
          Instruction *EntryIP = &*EntryBB.getFirstInsertionPt();
          IRBuilder<> EB(EntryIP);

          for (unsigned RI = 0; RI < ReductionPlans.size(); RI++) {
            ReductionPlan &Plan = ReductionPlans[RI];
            Type *RedTy = Plan.Update->getType();
            const uint32_t SlotElems =
                static_cast<uint32_t>(LaneCount ? LaneCount : 1u);
            Plan.Slot = EB.CreateAlloca(
                RedTy, ConstantInt::get(I32Ty, SlotElems), "linx.simt.redslot");
            Plan.SlotElems = SlotElems;
            Plan.DstName = kReductionDstRegs[RI];

            Value *SlotI64 = PB.CreatePtrToInt(Plan.Slot, I64Ty);
            auto Bind = bindI64(SlotI64);
            if (!Bind) {
              reject("reduction_bind_exhausted");
              return false;
            }
            Plan.SlotBind = *Bind;
            if (HasBoundedGroupedScratch)
              Plan.LocalWordBase = reserveLocalWords(LaneCount * GroupCount);

            auto InitTok = emitValue(Plan.InitValue);
            if (!InitTok) {
              reject("unsupported_reduction_init");
              return false;
            }
            OS << "  c.movr " << *InitTok << ", ->" << Plan.DstName << "\n";

            std::optional<std::string> Src;
            if (Plan.LaneMulL && Plan.LaneMulR) {
              auto Lhs = emitValue(Plan.LaneMulL);
              auto Rhs = emitValue(Plan.LaneMulR);
              if (!Lhs || !Rhs) {
                reject("unsupported_reduction_value");
                return false;
              }
              auto Mul = allocVec();
              if (!Mul) {
                reject("vector_reg_exhausted");
                return false;
              }
              OS << "  v.fmul " << formatFloatSrc(*Lhs) << ", "
                 << formatFloatSrc(*Rhs) << ", ->" << formatWordDest(*Mul)
                 << "\n";
              Src = *Mul;
            } else {
              Src = emitValue(Plan.LaneValue);
            }
            if (!Src) {
              reject("unsupported_reduction_value");
              return false;
            }

            OS << "  " << reductionMnemonic(Plan.Kind) << " "
               << formatFloatSrc(*Src) << ", ->" << Plan.DstName << "\n";
            if (Plan.LocalWordBase) {
              if (!emitStoreToLocalWordBase(Plan.DstName,
                                            *Plan.LocalWordBase)) {
                reject("reduction_store_emit_failed");
                return false;
              }
            } else {
              OS << "  v.sw.brg " << Plan.DstName << ", [ri" << Plan.SlotBind
                 << ", " << formatAddrExpr("lc0<<2") << ", "
                 << formatAddrExpr("zero<<2") << "]\n";
            }
          }
        }

        for (const LiveOutPlan &Plan : LiveOutPlans) {
          if (!Plan.Inst)
            continue;
          auto Tok = emitValue(Plan.Inst);
          if (!Tok) {
            reject("unsupported_liveout_value");
            return false;
          }
          const bool Stored =
              Plan.LocalWordBase
                  ? emitStoreToSharedLocalWordBase(
                        *Tok, *Plan.LocalWordBase,
                        /*IsFloat=*/Plan.Inst->getType() ==
                            Type::getFloatTy(Ctx))
                  : emitStoreToInvariantBind(*Tok, Plan.SlotBind,
                                             /*IsFloat=*/Plan.Inst->getType() ==
                                                 Type::getFloatTy(Ctx));
          if (!Stored) {
            reject("liveout_store_emit_failed");
            return false;
          }
        }

        if (ActiveSlotBind || ActiveSlotLocalWordBase)
          OS << AfterLabel << ":\n";

        for (const ExitPhiPlan &Plan : ExitPhiPlans) {
          if (!Plan.LocalWordBase)
            continue;
          auto Tok = emitLoadFromSharedLocalWordBase(*Plan.LocalWordBase);
          if (!Tok || !emitStoreToInvariantBind(
                          *Tok, Plan.SlotBind,
                          /*IsFloat=*/Plan.Phi &&
                              Plan.Phi->getType() == Type::getFloatTy(Ctx))) {
            reject("exit_phi_store_emit_failed");
            return false;
          }
        }

        for (const LiveOutPlan &Plan : LiveOutPlans) {
          if (!Plan.LocalWordBase)
            continue;
          auto Tok = emitLoadFromSharedLocalWordBase(*Plan.LocalWordBase);
          if (!Tok || !emitStoreToInvariantBind(
                          *Tok, Plan.SlotBind,
                          /*IsFloat=*/Plan.Inst &&
                              Plan.Inst->getType() == Type::getFloatTy(Ctx))) {
            reject("liveout_store_emit_failed");
            return false;
          }
        }

        for (const ReductionPlan &Plan : ReductionPlans) {
          if (!Plan.LocalWordBase)
            continue;
          auto Tok = emitLoadFromLocalWordBase(*Plan.LocalWordBase);
          if (!Tok) {
            reject("reduction_store_emit_failed");
            return false;
          }
          if (!emitStoreToInvariantBind(*Tok, Plan.SlotBind,
                                        /*IsFloat=*/true)) {
            reject("reduction_store_emit_failed");
            return false;
          }
        }
        OS << "  C.BSTOP\n";
        F.removeFnAttr("linx-vblock-ts-bytes");
        if (LocalScratchWordCount != 0) {
          const uint64_t ScratchBytes = LocalScratchWordCount * 4u;
          const uint64_t RoundedBytes =
              std::max<uint64_t>(16u, PowerOf2Ceil(ScratchBytes));
          F.addFnAttr("linx-vblock-ts-bytes", std::to_string(RoundedBytes));
        }
        F.addFnAttr("linx-vblock-body-asm", OS.str());

        // Decoupled body contract:
        // - Launch block carries only BSTART.{MSEQ,MPAR} descriptors.
        // - Out-of-line body is linear and ends with C.BSTOP.
        // - Header and body are connected via B.TEXT and execute with
        //   lane/group replay state (LB0/LB1/LB2).
        //
        // Create a dedicated launch block so the backend can form a valid
        // block header (BSTART.MSEQ/MPAR + descriptors) without non-descriptor
        // instructions preceding it.
        BasicBlock *LaunchBB =
            BasicBlock::Create(Ctx, "linx.vblock.launch", &F, Exit);
        IRBuilder<> LB(LaunchBB);

        // Recurrence lowering synthesizes v.lw/v.sw.brg traffic even when the
        // source loop has no explicit memory operation. Such bodies require an
        // MSEQ/MPAR header; VSEQ/VPAR is tile-only.
        const bool TouchesMemory =
            !Stores.empty() || !Loads.empty() || !RecurrencePlans.empty();
        const bool ParallelMode = (SelectedMode == "mpar");
        unsigned VKindImm = 0;
        if (TouchesMemory) {
          VKindImm = ParallelMode ? 1u : 0u; // MPAR/MSEQ
          RemarkHeaderKind = ParallelMode ? "mpar" : "mseq";
        } else {
          VKindImm = ParallelMode ? 3u : 2u; // VPAR/VSEQ
          RemarkHeaderKind = ParallelMode ? "vpar" : "vseq";
        }
        RemarkTouchesMemoryState = TouchesMemory ? 1 : 0;
        Value *VKind = ConstantInt::get(I32Ty, VKindImm);
        Value *BodySym = ConstantPointerNull::get(PointerType::getUnqual(Ctx));
        Value *Dim0 = ConstantInt::get(I64Ty, LaneCount);
        Value *Dim1 =
            (HasConstTripCount || GroupCount > 1)
                ? static_cast<Value *>(ConstantInt::get(I64Ty, GroupCount))
                : TripCountV;
        Value *Dim2 = ConstantInt::get(I64Ty, 1);
        Value *AttrBits = ConstantInt::get(I32Ty, 0);

        while (BindVals.size() < kMaxVBlockBinds)
          BindVals.push_back(ConstantInt::get(I64Ty, 0));

        LB.CreateCall(Intr,
                      {VKind, BodySym, Dim0, Dim1, Dim2, AttrBits, BindVals[0],
                       BindVals[1], BindVals[2], BindVals[3], BindVals[4],
                       BindVals[5], BindVals[6], BindVals[7], BindVals[8],
                       BindVals[9], BindVals[10], BindVals[11]});

        if (!ExitPhiPlans.empty()) {
          for (ExitPhiPlan &Plan : ExitPhiPlans) {
            if (!Plan.Phi || !Plan.Slot) {
              reject("invalid_exit_phi_plan");
              return false;
            }
            LoadInst *LiveOut =
                LB.CreateLoad(Plan.Phi->getType(), Plan.Slot, "linx.exitphi");
            Plan.Phi->addIncoming(LiveOut, LaunchBB);
          }
        }

        if (!ReductionPlans.empty() || !RecurrencePlans.empty() ||
            !LiveOutPlans.empty()) {
          Instruction *ExitIP = &*Exit->getFirstInsertionPt();
          IRBuilder<> ExitB(ExitIP);

          auto replaceOutsideUses = [&](Value *From, Value *To) {
            if (!From || !To)
              return;
            SmallVector<Use *, 8> ToReplace;
            for (Use &U : From->uses()) {
              auto *UI = dyn_cast<Instruction>(U.getUser());
              if (!UI)
                continue;
              if (!L->contains(UI))
                ToReplace.push_back(&U);
            }
            for (Use *U : ToReplace)
              U->set(To);
          };

          for (ReductionPlan &Plan : ReductionPlans) {
            Value *LoadPtr = Plan.Slot;
            if (Plan.SlotElems > 1) {
              LoadPtr = ExitB.CreateConstInBoundsGEP1_32(
                  Plan.Update->getType(), Plan.Slot,
                  static_cast<unsigned>(Plan.SlotElems - 1u),
                  "linx.red.last.ptr");
            }
            LoadInst *LiveOut =
                ExitB.CreateLoad(Plan.Update->getType(), LoadPtr, "linx.red");
            replaceOutsideUses(Plan.Update, LiveOut);
            replaceOutsideUses(Plan.Phi, LiveOut);
          }

          for (RecurrencePlan &Plan : RecurrencePlans) {
            if (!Plan.Slot || !Plan.Phi || !Plan.SlotTy) {
              reject("invalid_recurrence_plan");
              return false;
            }

            Value *Raw = ExitB.CreateLoad(Plan.SlotTy, Plan.Slot, "linx.rec");

            Value *PhiOut = Raw;
            if (Plan.SlotTy != Plan.Phi->getType()) {
              if (!Plan.SlotTy->isIntegerTy() ||
                  !Plan.Phi->getType()->isIntegerTy()) {
                reject("invalid_recurrence_liveout_cast");
                return false;
              }
              PhiOut = ExitB.CreateZExtOrTrunc(Raw, Plan.Phi->getType(),
                                               "linx.rec.zext");
            }

            Value *UpdateOut = PhiOut;
            if (Plan.Update && Plan.Update->getType() != Plan.Phi->getType()) {
              if (!Plan.Update->getType()->isIntegerTy() ||
                  !Plan.Phi->getType()->isIntegerTy()) {
                reject("invalid_recurrence_liveout_cast");
                return false;
              }
              UpdateOut = ExitB.CreateZExtOrTrunc(
                  PhiOut, Plan.Update->getType(), "linx.rec.upd.zext");
            }

            replaceOutsideUses(Plan.Update, UpdateOut);
            replaceOutsideUses(Plan.Phi, PhiOut);
          }

          for (const LiveOutPlan &Plan : LiveOutPlans) {
            if (!Plan.Inst || !Plan.Slot)
              continue;
            LoadInst *LiveOut = ExitB.CreateLoad(Plan.Inst->getType(),
                                                 Plan.Slot, "linx.liveout");
            replaceOutsideUses(Plan.Inst, LiveOut);
          }
        }

        LB.CreateBr(Exit);

        PHBr->setSuccessor(0, LaunchBB);

        FunctionLowered = true;
        Changed = true;
        Status = "lowered";
        Reason = (IsAffine ? ("lowered_vblock_" + RemarkHeaderKind + "_affine")
                           : ("lowered_vblock_" + RemarkHeaderKind));
        return true;
      };

      if (!IsInnermost) {
        reject("not_innermost_loop");
      } else if (!IsCanonical) {
        // Still reject non-simplified loops in the first slice: we rely on
        // LoopSimplifyForm for a stable preheader/header/exit structure.
        reject("not_loop_simplify");
      } else {
        (void)tryLowerToVBlock();
      }

      BasicBlock *Header = L->getHeader();
      StringRef LoopName = Header ? Header->getName() : StringRef("<unnamed>");
      emitRemark(F.getName(), LoopName, Status, Reason, ConfigMode,
                 SelectedMode, IsCounted, IsCanonical, IsSingleBlock, HasStore,
                 HasExtraPhi, RemarkLaneCount, RemarkGroupCount,
                 RemarkForceScalarLane, RemarkHasRecurrence, RemarkHeaderKind,
                 RemarkTouchesMemoryState, RemarkTripcountSource,
                 RemarkAddressModel, layoutPolicyName(LinxSIMTAutoVecLayout),
                 RemarkLayoutKind, RemarkCFStrategy);
    }
    return Changed;
  }

  bool runOnFunction(Function &F) override {
    auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
    return runWithAnalyses(F, LI, SE);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
  }
};

char LinxISASIMTAutoVectorize::ID = 0;

} // namespace

INITIALIZE_PASS_BEGIN(LinxISASIMTAutoVectorize, "linx-simt-autovec-pass",
                      "Linx SIMT AutoVectorize", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(LinxISASIMTAutoVectorize, "linx-simt-autovec-pass",
                    "Linx SIMT AutoVectorize", false, false)

bool llvm::linxSIMTAutoVectorizeEnabled() { return LinxSIMTAutoVec; }

StringRef llvm::linxSIMTAutoVectorizeMode() {
  return modeName(LinxSIMTAutoVecMode);
}

StringRef llvm::linxSIMTAutoVectorizeRemarksPath() {
  return LinxSIMTAutoVecRemarks;
}

FunctionPass *llvm::createLinxISASIMTAutoVectorizePass() {
  return new LinxISASIMTAutoVectorize();
}

PreservedAnalyses
llvm::LinxISASIMTAutoVectorizePass::run(Function &F,
                                        FunctionAnalysisManager &AM) {
  if (!linxSIMTAutoVectorizeEnabled() || F.isDeclaration() ||
      isTsvcAuxHelperName(F.getName()) || !F.getParent()) {
    return PreservedAnalyses::all();
  }

  auto &LI = AM.getResult<LoopAnalysis>(F);
  auto &SE = AM.getResult<ScalarEvolutionAnalysis>(F);
  bool Changed = LinxISASIMTAutoVectorize::runWithAnalyses(F, LI, SE);
  return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}
