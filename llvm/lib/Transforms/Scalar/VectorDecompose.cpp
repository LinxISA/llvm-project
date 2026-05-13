#include "llvm/Transforms/Scalar/VectorDecompose.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "vector-decompose"

using namespace llvm;

namespace {

class VectorDecomposer {
public:
  VectorDecomposer(LLVMContext &Ctx) : Ctx(Ctx) {}

  // Recursively map types containing vectors to types containing arrays
  Type *decomposeType(Type *Ty) {
    if (!Ty)
      return nullptr;
    if (auto *VecTy = dyn_cast<FixedVectorType>(Ty)) {
      return ArrayType::get(VecTy->getElementType(), VecTy->getNumElements());
    }
    if (auto *ArrTy = dyn_cast<ArrayType>(Ty)) {
      return ArrayType::get(decomposeType(ArrTy->getElementType()),
                            ArrTy->getNumElements());
    }
    if (auto *StructTy = dyn_cast<StructType>(Ty)) {
      if (StructTy->isOpaque())
        return StructTy;
      SmallVector<Type *, 8> Elts;
      for (auto *Elt : StructTy->elements())
        Elts.push_back(decomposeType(Elt));
      return StructTy->hasName() ? StructTy : StructType::get(Ctx, Elts);
    }
    return Ty; // In Opaque Pointers, ptr is just ptr
  }

  void decomposeAlloca(AllocaInst *AI) {
    Type *NewTy = decomposeType(AI->getAllocatedType());
    if (NewTy == AI->getAllocatedType())
      return;

    LLVM_DEBUG(dbgs() << "[VectorDecompose] Decomposing Alloca: " << *AI
                      << "\n");
    IRBuilder<> Builder(AI);
    auto *NewAI = Builder.CreateAlloca(NewTy, AI->getArraySize(),
                                       AI->getName() + ".array");
    NewAI->setAlignment(AI->getAlign());
    AI->replaceAllUsesWith(NewAI);
    markDead(AI);
  }

  void decomposeGEP(GetElementPtrInst *GEP) {
    Type *NewSourceTy = decomposeType(GEP->getSourceElementType());
    if (NewSourceTy == GEP->getSourceElementType())
      return;

    LLVM_DEBUG(dbgs() << "[VectorDecompose] Decomposing GEP: " << *GEP << "\n");
    IRBuilder<> Builder(GEP);
    auto *NewGEP = Builder.CreateGEP(NewSourceTy, GEP->getPointerOperand(),
                                     SmallVector<Value *, 4>(GEP->indices()),
                                     GEP->getName() + ".agep");
    GEP->replaceAllUsesWith(NewGEP);
    markDead(GEP);
  }

  void decomposeLoad(LoadInst *LI) {
    if (!LI->getType()->isVectorTy())
      return;
    auto *VecTy = cast<FixedVectorType>(LI->getType());

    LLVM_DEBUG(dbgs() << "[VectorDecompose] Decomposing Load: " << *LI << "\n");
    IRBuilder<> Builder(LI);
    Type *ArrTy = decomposeType(VecTy);
    Value *ResVec = UndefValue::get(VecTy);

    for (unsigned i = 0; i < VecTy->getNumElements(); ++i) {
      Value *EltPtr =
          Builder.CreateInBoundsGEP(ArrTy, LI->getPointerOperand(),
                                    {Builder.getInt32(0), Builder.getInt32(i)});
      Value *Elt = Builder.CreateLoad(VecTy->getElementType(), EltPtr);
      ResVec = Builder.CreateInsertElement(ResVec, Elt, i);
    }
    LI->replaceAllUsesWith(ResVec);
    markDead(LI);
  }

  void decomposeStore(StoreInst *SI) {
    Value *Val = SI->getValueOperand();
    if (!Val->getType()->isVectorTy())
      return;
    auto *VecTy = cast<FixedVectorType>(Val->getType());

    LLVM_DEBUG(dbgs() << "[VectorDecompose] Decomposing Store: " << *SI
                      << "\n");
    IRBuilder<> Builder(SI);
    Type *ArrTy = decomposeType(VecTy);

    for (unsigned i = 0; i < VecTy->getNumElements(); ++i) {
      Value *Elt = Builder.CreateExtractElement(Val, i);
      Value *EltPtr =
          Builder.CreateInBoundsGEP(ArrTy, SI->getPointerOperand(),
                                    {Builder.getInt32(0), Builder.getInt32(i)});
      Builder.CreateStore(Elt, EltPtr);
    }
    markDead(SI);
  }

  // Handle <2 x i32> -> <8 x i8> bitcast + extract
  void handleVectorToVectorBitcast(BitCastInst *BC) {
    auto *SrcVec = dyn_cast<FixedVectorType>(BC->getSrcTy());
    auto *DstVec = dyn_cast<FixedVectorType>(BC->getDestTy());
    if (!SrcVec || !DstVec)
      return;

    for (auto it = BC->user_begin(); it != BC->user_end();) {
      User *U = *it++;
      if (auto *EEI = dyn_cast<ExtractElementInst>(U)) {
        if (auto *CIdx = dyn_cast<ConstantInt>(EEI->getOperand(1))) {
          LLVM_DEBUG(dbgs()
                     << "[VectorDecompose] Folding Cross-Vector Bitcast\n");
          IRBuilder<> Builder(EEI);
          uint64_t DstBits = DstVec->getScalarSizeInBits();
          uint64_t SrcBits = SrcVec->getScalarSizeInBits();
          uint64_t TotalOffset = CIdx->getZExtValue() * DstBits;

          Value *SrcElt = Builder.CreateExtractElement(BC->getOperand(0),
                                                       TotalOffset / SrcBits);
          Value *AsInt =
              Builder.CreateBitCast(SrcElt, Builder.getIntNTy(SrcBits));
          Value *Shift = Builder.CreateLShr(AsInt, TotalOffset % SrcBits);
          Value *Trunc = Builder.CreateTrunc(Shift, Builder.getIntNTy(DstBits));
          EEI->replaceAllUsesWith(Builder.CreateBitCast(Trunc, EEI->getType()));
          markDead(EEI);
        }
      }
    }
  }

  // Handle bitcast i32 to <4 x i8> + extractelement to sclar
  void handleBitcastExtractOptimization(BitCastInst *BC) {
    if (!BC || !BC->getSrcTy()->isIntegerTy() || !BC->getDestTy()->isVectorTy())
      return;

    for (auto it = BC->user_begin(); it != BC->user_end();) {
      User *U = *it++;
      if (auto *EEI = dyn_cast<ExtractElementInst>(U)) {
        if (auto *CIdx = dyn_cast<ConstantInt>(EEI->getOperand(1))) {
          LLVM_DEBUG(dbgs()
                     << "[VectorDecompose] Folding Cross-Vector Bitcast\n");
          IRBuilder<> Builder(EEI);
          uint64_t DstBits = EEI->getType()->getScalarSizeInBits();
          uint64_t TotalOffset = CIdx->getZExtValue() * DstBits;
          Value *OriginalVal = BC->getOperand(0);
          Value *Shift = Builder.CreateLShr(OriginalVal, TotalOffset);
          Value *Trunc = Builder.CreateTrunc(Shift, Builder.getIntNTy(DstBits));
          EEI->replaceAllUsesWith(Trunc);
          markDead(EEI);
          markDead(BC);
        }
      }
    }
  }

  // Handle insertelement chain -> bitcast to scalar
  void handleInsertToScalarBitcast(BitCastInst *BC) {
    if (!BC->getSrcTy()->isVectorTy() || !BC->getDestTy()->isIntegerTy())
      return;
    IRBuilder<> Builder(BC);
    Value *Accum = ConstantInt::get(BC->getDestTy(), 0);
    Value *Curr = BC->getOperand(0);
    bool Valid = true;

    while (auto *IEI = dyn_cast<InsertElementInst>(Curr)) {
      if (auto *CIdx = dyn_cast<ConstantInt>(IEI->getOperand(2))) {
        uint64_t Offset =
            CIdx->getZExtValue() * IEI->getType()->getScalarSizeInBits();
        Value *Elt = Builder.CreateZExt(IEI->getOperand(1), BC->getDestTy());
        Accum = Builder.CreateOr(Accum, Builder.CreateShl(Elt, Offset));
        Curr = IEI->getOperand(0);
      } else {
        Valid = false;
        break;
      }
    }
    if (Valid) {
      BC->replaceAllUsesWith(Accum);
      markDead(BC);
    }
  }

  // Handle insertelement <2 * float> chain + bitcast <2 x float> to double
  // input:
  //   %t010.i0.upto0 = insertelement <2 x float> poison, float %.i066, i32 0
  //   %t010.i0.upto1 = insertelement <2 x float> %t010.i0.upto0, float %.i074, i32 1
  //   %t010.i0 = bitcast <2 x float> %t010.i0.upto1 to double
  // output:
  //   %83 = bitcast float %.i074 to i32
  //   %84 = bitcast float %.i066 to i32
  //   %85 = zext i32 %83 to i64
  //   %86 = zext i32 %84 to i64
  //   %87 = shl i64 %86, 32
  //   %88 = or i64 %85, %87
  //   %89 = bitcast i64 %88 to double
  void handleInsertFloatToScalarBitcast(BitCastInst *BC) {
    if (!BC->getSrcTy()->isVectorTy() || !BC->getDestTy()->isDoubleTy())
      return;
    IRBuilder<> Builder(BC);
    Value *Curr = BC->getOperand(0);

    if (auto *IEI = dyn_cast<InsertElementInst>(Curr)) {
      auto *IEI2 = dyn_cast<InsertElementInst>(IEI->getOperand(0));
      auto *Float1 = IEI->getOperand(1);
      auto *Float2 = IEI2->getOperand(1);
      auto *Float1Cast =
          Builder.CreateBitCast(Float1, Type::getInt32Ty(BC->getContext()));
      auto *Float2Cast =
          Builder.CreateBitCast(Float2, Type::getInt32Ty(BC->getContext()));
      auto *Float1Ext =
          Builder.CreateZExt(Float1Cast, Type::getInt64Ty(BC->getContext()));
      auto *Float2Ext =
          Builder.CreateZExt(Float2Cast, Type::getInt64Ty(BC->getContext()));
      auto *ShiftedFloat2 = Builder.CreateShl(Float2Ext, 32);
      auto *Combined = Builder.CreateOr(Float1Ext, ShiftedFloat2);
      auto *NewDouble =
          Builder.CreateBitCast(Combined, Type::getDoubleTy(BC->getContext()));
      BC->replaceAllUsesWith(NewDouble);
      markDead(BC);
      markDead(IEI);
      markDead(IEI2);
    }
  }

  void markDead(Instruction *I) { DeadInsts.insert(I); }

  void sweep() {
    // Iteratively delete instructions to handle dependencies
    while (!DeadInsts.empty()) {
      Instruction *I = DeadInsts.pop_back_val();
      if (I->use_empty()) {
        for (Value *Op : I->operands())
          if (auto *OpI = dyn_cast<Instruction>(Op))
            DeadInsts.insert(OpI);
        I->eraseFromParent();
      }
    }
  }

  SetVector<Instruction *> DeadInsts;

private:
  LLVMContext &Ctx;
};

} // namespace

PreservedAnalyses VectorDecomposePass::run(Function &F,
                                           FunctionAnalysisManager &AM) {
  if (!F.getFnAttribute("__mtc__").isValid() &&
      !F.getFnAttribute("__vec__").isValid())
    return PreservedAnalyses::none();

  VectorDecomposer Decomposer(F.getContext());

  // Phase 1: Main Decomposition
  for (Instruction &I : make_early_inc_range(instructions(F))) {
    if (auto *AI = dyn_cast<AllocaInst>(&I))
      Decomposer.decomposeAlloca(AI);
    else if (auto *GEP = dyn_cast<GetElementPtrInst>(&I))
      Decomposer.decomposeGEP(GEP);
    else if (auto *LI = dyn_cast<LoadInst>(&I))
      Decomposer.decomposeLoad(LI);
    else if (auto *SI = dyn_cast<StoreInst>(&I))
      Decomposer.decomposeStore(SI);
    else if (auto *BC = dyn_cast<BitCastInst>(&I)) {
      Decomposer.handleVectorToVectorBitcast(BC);
      Decomposer.handleBitcastExtractOptimization(BC);
      Decomposer.handleInsertToScalarBitcast(BC);
      Decomposer.handleInsertFloatToScalarBitcast(BC);
      // Cleanup A->B->C bitcasts
      if (!BC->use_empty()) {
        if (auto *NextBC = dyn_cast<BitCastInst>(BC->user_back())) {
          if (BC->hasOneUse()) {
            IRBuilder<> B(NextBC);
            NextBC->replaceAllUsesWith(
                B.CreateBitCast(BC->getOperand(0), NextBC->getDestTy()));
            Decomposer.markDead(NextBC);
            Decomposer.markDead(BC);
          }
        }
      }
    }
  }

  // Phase 2: Insert-to-Extract Cleanup
  for (Instruction &I : make_early_inc_range(instructions(F))) {
    if (auto *EEI = dyn_cast<ExtractElementInst>(&I)) {
      Value *V = EEI->getOperand(0);
      while (auto *IEI = dyn_cast<InsertElementInst>(V)) {
        auto *C1 = dyn_cast<ConstantInt>(EEI->getOperand(1));
        auto *C2 = dyn_cast<ConstantInt>(IEI->getOperand(2));
        assert(C1 && C2 && "Insert-to-Extract Cleanup Fail!");
        if (C1 && C2 &&
            C1->getValue().getSExtValue() == C2->getValue().getSExtValue()) {
          EEI->replaceAllUsesWith(IEI->getOperand(1));
          Decomposer.markDead(EEI);
          break;
        }
        V = IEI->getOperand(0);
      }
    }
  }

  Decomposer.sweep();
  return PreservedAnalyses::none();
}
