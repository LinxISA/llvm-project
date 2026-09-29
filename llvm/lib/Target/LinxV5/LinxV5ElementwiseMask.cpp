//===- LinxV5ElementwiseMask.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// Establishes the frontend contract for PTO element-wise control flow.
//
/// This pass deliberately does not lower to SIMT intrinsics or registers. It
/// marks divergent branches in an opt-in element-wise function so later Linx
/// element-wise lowering stages can form logical active-mask SSA.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/InitializePasses.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define DEBUG_TYPE "linx-elementwise-mask"

namespace {

static constexpr StringLiteral ElementwiseAttr = "linx.elementwise";
static constexpr StringLiteral BranchMetadata = "linx.elementwise.branch";
static constexpr StringLiteral LoopMetadata = "linx.elementwise.loop";
static constexpr StringLiteral ModelMetadata = "linx.elementwise.mask-model";
static constexpr StringLiteral LanesAttr = "linx.elementwise.lanes";

static Optional<uint64_t> getLaneCount(const Function &F) {
  Attribute Attr = F.getFnAttribute(LanesAttr);
  if (!Attr.isValid())
    return None;

  uint64_t Lanes = 0;
  if (Attr.getValueAsString().getAsInteger(10, Lanes) || Lanes == 0)
    report_fatal_error("linx.elementwise.lanes must be a positive integer");
  return Lanes;
}

class LinxV5ElementwiseMask : public FunctionPass {
  static uint64_t getConstantMaskDimension(CallInst *Call, unsigned Index,
                                           StringRef Name) {
    auto *Constant = dyn_cast<ConstantInt>(Call->getArgOperand(Index));
    if (!Constant || Constant->isZero() ||
        Constant->getValue().getActiveBits() > 16)
      report_fatal_error(
          Twine(Name) + " requires a positive i16-sized constant");
    return Constant->getZExtValue();
  }

  Value *lowerGPRPack(CallInst *Call, bool HighWord) {
    auto *MaskTy = dyn_cast<FixedVectorType>(Call->getArgOperand(0)->getType());
    if (!MaskTy || !MaskTy->getElementType()->isIntegerTy(1))
      report_fatal_error(
          "GPR execution-mask packing requires a fixed <N x i1> operand");

    uint64_t ValidRows = getConstantMaskDimension(Call, 1, "mask pack");
    uint64_t ValidColumns = getConstantMaskDimension(Call, 2, "mask pack");
    auto *M32Flag = dyn_cast<ConstantInt>(Call->getArgOperand(3));
    if (!M32Flag)
      report_fatal_error("GPR execution-mask packing requires constant is_m32");
    bool IsM32 = !M32Flag->isZero();
    uint64_t Rows = IsM32 ? 32 : 16;
    if (ValidRows > Rows || ValidColumns * Rows > 128)
      report_fatal_error(
          "GPR execution-mask packing exceeds the CUBE M16/M32 carrier");
    if (ValidRows * ValidColumns > MaskTy->getNumElements())
      report_fatal_error(
          "GPR execution-mask packing exceeds the logical mask length");

    IRBuilder<> Builder(Call);
    Value *Packed = ConstantInt::get(Type::getInt64Ty(Call->getContext()), 0);
    for (uint64_t Column = 0; Column < ValidColumns; ++Column) {
      for (uint64_t Row = 0; Row < ValidRows; ++Row) {
        uint64_t CubeBit = Row + Column * Rows;
        if ((HighWord && CubeBit < 64) || (!HighWord && CubeBit >= 64))
          continue;
        uint64_t WordBit = HighWord ? CubeBit - 64 : CubeBit;
        Value *Bit = Builder.CreateExtractElement(
            Call->getArgOperand(0),
            ConstantInt::get(Type::getInt32Ty(Call->getContext()),
                             Row * ValidColumns + Column));
        Bit = Builder.CreateZExt(Bit, Type::getInt64Ty(Call->getContext()));
        Bit = Builder.CreateShl(
            Bit, ConstantInt::get(Type::getInt64Ty(Call->getContext()),
                                  WordBit));
        Packed = Builder.CreateOr(Packed, Bit,
                                  HighWord ? "ew.mask.gpr.high"
                                            : "ew.mask.gpr.low");
      }
    }
    return Packed;
  }

  bool lowerMaskIntrinsics(Function &F) {
    SmallVector<CallInst *, 16> Calls;
    for (BasicBlock &BB : F)
      for (Instruction &I : BB)
        if (auto *Call = dyn_cast<CallInst>(&I))
          if (Function *Callee = Call->getCalledFunction())
            if (Callee->isIntrinsic() &&
                Callee->getName().startswith(
                    "llvm.linx.experimental.ew.mask."))
              Calls.push_back(Call);

    bool Changed = false;
    for (CallInst *Call : Calls) {
      auto RequireMaskVector = [Call]() -> FixedVectorType * {
        auto *MaskTy = dyn_cast<FixedVectorType>(Call->getType());
        if (!MaskTy || !MaskTy->getElementType()->isIntegerTy(1))
          report_fatal_error(
              "element-wise mask intrinsic requires a fixed <N x i1> vector");
        return MaskTy;
      };
      auto RequireMaskOperand = [Call](unsigned Index) {
        auto *MaskTy = dyn_cast<FixedVectorType>(
            Call->getArgOperand(Index)->getType());
        if (!MaskTy || !MaskTy->getElementType()->isIntegerTy(1))
          report_fatal_error(
              "element-wise mask intrinsic requires a fixed <N x i1> operand");
      };

      IRBuilder<> Builder(Call);
      Value *Replacement = nullptr;
      switch (Call->getIntrinsicID()) {
      case Intrinsic::linx_experimental_ew_mask_splat: {
        auto *MaskTy = RequireMaskVector();
        Replacement =
            Builder.CreateVectorSplat(MaskTy->getNumElements(),
                                      Call->getArgOperand(0), "ew.mask.splat");
        break;
      }
      case Intrinsic::linx_experimental_ew_mask_and:
        RequireMaskVector();
        RequireMaskOperand(0);
        RequireMaskOperand(1);
        Replacement = Builder.CreateAnd(Call->getArgOperand(0),
                                        Call->getArgOperand(1), "ew.mask.and");
        break;
      case Intrinsic::linx_experimental_ew_mask_andnot:
        RequireMaskVector();
        RequireMaskOperand(0);
        RequireMaskOperand(1);
        Replacement = Builder.CreateAnd(
            Call->getArgOperand(0), Builder.CreateNot(Call->getArgOperand(1)),
            "ew.mask.andnot");
        break;
      case Intrinsic::linx_experimental_ew_mask_or:
        RequireMaskVector();
        RequireMaskOperand(0);
        RequireMaskOperand(1);
        Replacement = Builder.CreateOr(Call->getArgOperand(0),
                                       Call->getArgOperand(1), "ew.mask.or");
        break;
      case Intrinsic::linx_experimental_ew_mask_select:
        RequireMaskVector();
        RequireMaskOperand(0);
        RequireMaskOperand(1);
        RequireMaskOperand(2);
        Replacement = Builder.CreateSelect(Call->getArgOperand(2),
                                           Call->getArgOperand(0),
                                           Call->getArgOperand(1),
                                           "ew.mask.select");
        break;
      case Intrinsic::linx_experimental_ew_mask_any: {
        RequireMaskOperand(0);
        Function *Reduce = Intrinsic::getDeclaration(
            F.getParent(), Intrinsic::vector_reduce_or,
            {Call->getArgOperand(0)->getType()});
        Replacement = Builder.CreateCall(Reduce, Call->getArgOperand(0),
                                         "ew.mask.any");
        break;
      }
      case Intrinsic::linx_experimental_ew_mask_all: {
        RequireMaskOperand(0);
        Function *Reduce = Intrinsic::getDeclaration(
            F.getParent(), Intrinsic::vector_reduce_and,
            {Call->getArgOperand(0)->getType()});
        Replacement = Builder.CreateCall(Reduce, Call->getArgOperand(0),
                                         "ew.mask.all");
        break;
      }
      case Intrinsic::linx_experimental_ew_mask_pack_gpr_low:
        Replacement = lowerGPRPack(Call, false);
        break;
      case Intrinsic::linx_experimental_ew_mask_pack_gpr_high:
        Replacement = lowerGPRPack(Call, true);
        break;
      default:
        llvm_unreachable("unexpected element-wise mask intrinsic");
      }

      Call->replaceAllUsesWith(Replacement);
      Call->eraseFromParent();
      Changed = true;
    }
    return Changed;
  }

public:
  static char ID;
  LinxV5ElementwiseMask() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    if (!F.hasFnAttribute(ElementwiseAttr))
      return false;

    // Element-wise PTO lowering must never be selected for the SIMT frontend
    // entry points. The target-machine CPU selection already keeps these
    // paths separate; this guard makes the frontend contract explicit.
    if (F.hasFnAttribute("__vec__") || F.hasFnAttribute("__mtc__"))
      return false;

    auto &DA = getAnalysis<LegacyDivergenceAnalysis>();
    bool Changed = lowerMaskIntrinsics(F);
    LLVMContext &Context = F.getContext();
    Optional<uint64_t> LaneCount = getLaneCount(F);
    SmallVector<Metadata *, 3> ModelOperands;
    ModelOperands.push_back(MDString::get(Context, "logical-i1"));
    if (LaneCount) {
      ModelOperands.push_back(ConstantAsMetadata::get(
          ConstantInt::get(Type::getInt64Ty(Context), *LaneCount)));
      ModelOperands.push_back(MDString::get(
          Context, *LaneCount <= 128 ? "gpr-mask" : "mask-tile"));
    }
    MDNode *Model = MDNode::get(Context, ModelOperands);
    F.setMetadata(ModelMetadata, Model);
    Changed = true;

    for (BasicBlock &BB : F) {
      auto *Branch = dyn_cast<BranchInst>(BB.getTerminator());
      if (!Branch || !Branch->isConditional() || DA.isUniform(Branch))
        continue;

      Branch->setMetadata(
          BranchMetadata,
          MDNode::get(Context, {MDString::get(Context, "divergent"),
                                MDString::get(Context, "active-mask-operand")}));
      Changed = true;

      if (Loop *L = getAnalysis<LoopInfoWrapperPass>().getLoopInfo().getLoopFor(&BB)) {
        BasicBlock *Header = L->getHeader();
        if (Header == &BB)
          Header->getTerminator()->setMetadata(
              LoopMetadata,
              MDNode::get(Context, {MDString::get(Context, "active-mask-phi"),
                                    MDString::get(Context, "break-mask")}));
      }
    }

    return Changed;
  }

  StringRef getPassName() const override {
    return "Linx element-wise logical mask contract";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LegacyDivergenceAnalysis>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesCFG();
    FunctionPass::getAnalysisUsage(AU);
  }
};

} // namespace

char LinxV5ElementwiseMask::ID = 0;

INITIALIZE_PASS_BEGIN(LinxV5ElementwiseMask, DEBUG_TYPE,
                      "Mark Linx element-wise logical mask regions", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(LegacyDivergenceAnalysis)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(LinxV5ElementwiseMask, DEBUG_TYPE,
                    "Mark Linx element-wise logical mask regions", false,
                    false)

FunctionPass *llvm::createLinxV5ElementwiseMaskPass() {
  return new LinxV5ElementwiseMask();
}
