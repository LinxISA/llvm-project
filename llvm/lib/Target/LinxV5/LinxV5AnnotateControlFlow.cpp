//===- LinxV5AnnotateControlFlow.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Annotates the control flow with hardware specific intrinsics.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5Subtarget.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/InitializePasses.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

#define DEBUG_TYPE "linx-annotate-control-flow"

namespace {

// Complex types used in this pass
using StackEntry = std::pair<BasicBlock *, Value *>;
using StackVector = SmallVector<StackEntry, 16>;

class LinxV5AnnotateControlFlow : public FunctionPass {
  LegacyDivergenceAnalysis *DA;

  Type *Boolean;
  Type *Void;
  Type *IntMask;
  Type *ReturnStruct;

  ConstantInt *BoolTrue;
  ConstantInt *BoolFalse;
  UndefValue *BoolUndef;
  Constant *IntMaskZero;

  Function *If;
  Function *Else;
  Function *IfBreak;
  Function *Loop;
  Function *EndCf;
  Function *MergeCF;

  DominatorTree *DT;
  StackVector Stack;

  LoopInfo *LI;

  void initialize(Module &M, const LinxV5Subtarget &ST);

  bool isUniform(BranchInst *T);

  bool isTopOfStack(BasicBlock *BB);

  Value *popSaved();

  void push(BasicBlock *BB, Value *Saved);

  bool isElse(PHINode *Phi);

  bool hasKill(const BasicBlock *BB);

  bool eraseIfUnused(PHINode *Phi);

  bool openIf(BranchInst *Term);

  bool insertElse(BranchInst *Term);

  Value *
  handleLoopCondition(Value *Cond, PHINode *Broken, llvm::Loop *L,
                      BranchInst *Term);

  bool handleLoop(BranchInst *Term);

  bool closeControlFlow(BasicBlock *BB);

  std::pair<PHINode *, PHINode *> splitPhiNode(llvm::PHINode *PHI,
                                               BasicBlock *BB);

public:
  static char ID;

  LinxV5AnnotateControlFlow() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

  StringRef getPassName() const override { return "Linx annotate control flow"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LegacyDivergenceAnalysis>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<TargetPassConfig>();
    FunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

INITIALIZE_PASS_BEGIN(LinxV5AnnotateControlFlow, DEBUG_TYPE,
                      "Annotate Block Function Control Flow", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LegacyDivergenceAnalysis)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(LinxV5AnnotateControlFlow, DEBUG_TYPE,
                    "Annotate Block Function Control Flow", false, false)

char LinxV5AnnotateControlFlow::ID = 0;

/// Initialize all the types and constants used in the pass
void LinxV5AnnotateControlFlow::initialize(Module &M, const LinxV5Subtarget &ST) {
  LLVMContext &Context = M.getContext();

  Void = Type::getVoidTy(Context);
  Boolean = Type::getInt1Ty(Context);
  IntMask = Type::getInt64Ty(Context);
  ReturnStruct = StructType::get(Boolean, IntMask);

  BoolTrue = ConstantInt::getTrue(Context);
  BoolFalse = ConstantInt::getFalse(Context);
  BoolUndef = UndefValue::get(Boolean);
  IntMaskZero = ConstantInt::get(IntMask, 0);

  If = Intrinsic::getDeclaration(&M, Intrinsic::blkv_if, { IntMask });
  Else = Intrinsic::getDeclaration(&M, Intrinsic::blkv_flow,
                                   { IntMask, IntMask });
  IfBreak = Intrinsic::getDeclaration(&M, Intrinsic::blkv_if_break,
                                      { IntMask });
  Loop = Intrinsic::getDeclaration(&M, Intrinsic::blkv_loop, { IntMask });
  EndCf = Intrinsic::getDeclaration(&M, Intrinsic::blkv_end_cf, { IntMask });
}

/// Is the branch condition uniform or did the StructurizeCFG pass
/// consider it as such?
bool LinxV5AnnotateControlFlow::isUniform(BranchInst *T) {
  return DA->isUniform(T) ||
         T->getMetadata("structurizecfg.uniform") != nullptr;
}

/// Is BB the last block saved on the stack ?
bool LinxV5AnnotateControlFlow::isTopOfStack(BasicBlock *BB) {
  return !Stack.empty() && Stack.back().first == BB;
}

/// Pop the last saved value from the control flow stack
Value *LinxV5AnnotateControlFlow::popSaved() {
  return Stack.pop_back_val().second;
}

/// Push a BB and saved value to the control flow stack
void LinxV5AnnotateControlFlow::push(BasicBlock *BB, Value *Saved) {
  Stack.push_back(std::make_pair(BB, Saved));
}

/// Can the condition represented by this PHI node treated like
/// an "Else" block?
bool LinxV5AnnotateControlFlow::isElse(PHINode *Phi) {
  BasicBlock *IDom = DT->getNode(Phi->getParent())->getIDom()->getBlock();
  for (unsigned i = 0, e = Phi->getNumIncomingValues(); i != e; ++i) {
    if (Phi->getIncomingBlock(i) == IDom) {

      if (Phi->getIncomingValue(i) != BoolTrue)
        return false;

    } else {
      if (Phi->getIncomingValue(i) != BoolFalse)
        return false;

    }
  }
  return true;
}

bool LinxV5AnnotateControlFlow::hasKill(const BasicBlock *BB) {
  for (const Instruction &I : *BB) {
    if (const CallInst *CI = dyn_cast<CallInst>(&I))
      if (CI->getIntrinsicID() == Intrinsic::blkv_kill)
        return true;
  }
  return false;
}

// Erase "Phi" if it is not used any more. Return true if any change was made.
bool LinxV5AnnotateControlFlow::eraseIfUnused(PHINode *Phi) {
  bool Changed = RecursivelyDeleteDeadPHINode(Phi);
  if (Changed)
    LLVM_DEBUG(dbgs() << "Erased unused condition phi\n");
  return Changed;
}

/// Open a new "If" block
bool LinxV5AnnotateControlFlow::openIf(BranchInst *Term) {
  if (isUniform(Term))
    return false;

  Value *Ret = CallInst::Create(If, Term->getCondition(), "", Term);
  Term->setCondition(ExtractValueInst::Create(Ret, 0, "", Term));
  push(Term->getSuccessor(1), ExtractValueInst::Create(Ret, 1, "", Term));
  return true;
}

/// Close the last "If" block and open a new "Else" block
bool LinxV5AnnotateControlFlow::insertElse(BranchInst *Term) {
  if (isUniform(Term)) {
    return false;
  }
  Value *Ret = CallInst::Create(Else, popSaved(), "", Term);
  Term->setCondition(ExtractValueInst::Create(Ret, 0, "", Term));
  push(Term->getSuccessor(1), ExtractValueInst::Create(Ret, 1, "", Term));
  return true;
}

/// Recursively handle the condition leading to a loop
Value *LinxV5AnnotateControlFlow::handleLoopCondition(
    Value *Cond, PHINode *Broken, llvm::Loop *L, BranchInst *Term) {
  if (Instruction *Inst = dyn_cast<Instruction>(Cond)) {
    BasicBlock *Parent = Inst->getParent();
    Instruction *Insert;
    if (L->contains(Inst)) {
      Insert = Parent->getTerminator();
    } else {
      Insert = L->getHeader()->getFirstNonPHIOrDbgOrLifetime();
    }

    Value *Args[] = { Cond, Broken };
    return CallInst::Create(IfBreak, Args, "", Insert);
  }

  // Insert IfBreak in the loop header TERM for constant COND other than true.
  if (isa<Constant>(Cond)) {
    Instruction *Insert = Cond == BoolTrue ?
      Term : L->getHeader()->getTerminator();

    Value *Args[] = { Cond, Broken };
    return CallInst::Create(IfBreak, Args, "", Insert);
  }

  if (isa<Argument>(Cond)) {
    Instruction *Insert = L->getHeader()->getFirstNonPHIOrDbgOrLifetime();
    Value *Args[] = { Cond, Broken };
    return CallInst::Create(IfBreak, Args, "", Insert);
  }

  llvm_unreachable("Unhandled loop condition!");
}

/// Handle a back edge (loop)
// Insert an phi.broken instruction at the front of loop body
// Set the exiting condition of the loop to true
// Insert the blkv_if_break intrinsic before the loop exiting branch
// Assign 0 to the broken variable in all predecessors
// Insert the blkv_loop intrinsic before the loop exiting branch with the result of the blkv_if_break
// Set the exiting condition of the loop to the result of the blkv_loop intrinsic
bool LinxV5AnnotateControlFlow::handleLoop(BranchInst *Term) {
  if (isUniform(Term))
    return false;

  BasicBlock *BB = Term->getParent();
  llvm::Loop *L = LI->getLoopFor(BB);
  if (!L)
    return false;

  BasicBlock *Target = Term->getSuccessor(1);
  PHINode *Broken = PHINode::Create(IntMask, 0, "phi.broken", &Target->front());

  Value *Cond = Term->getCondition();
  Term->setCondition(BoolTrue);
  Value *Arg = handleLoopCondition(Cond, Broken, L, Term);

  for (BasicBlock *Pred : predecessors(Target)) {
    Value *PHIValue = IntMaskZero;
    if (Pred == BB) // Remember the value of the previous iteration.
      PHIValue = Arg;
    // If the backedge from Pred to Target could be executed before the exit
    // of the loop at BB, it should not reset or change "Broken", which keeps
    // track of the number of threads exited the loop at BB.
    else if (L->contains(Pred) && DT->dominates(Pred, BB))
      PHIValue = Broken;
    Broken->addIncoming(PHIValue, Pred);
  }

  Term->setCondition(CallInst::Create(Loop, Arg, "", Term));

  push(Term->getSuccessor(0), Arg);

  return true;
}

/// Close the last opened control flow
bool LinxV5AnnotateControlFlow::closeControlFlow(BasicBlock *BB) {
  llvm::Loop *L = LI->getLoopFor(BB);

  assert(Stack.back().first == BB);

  if (L && L->getHeader() == BB) {
    // We can't insert an EndCF call into a loop header, because it will
    // get executed on every iteration of the loop, when it should be
    // executed only once before the loop.
    SmallVector <BasicBlock *, 8> Latches;
    L->getLoopLatches(Latches);

    SmallVector<BasicBlock *, 2> Preds;
    for (BasicBlock *Pred : predecessors(BB)) {
      if (!is_contained(Latches, Pred))
        Preds.push_back(Pred);
    }

    BB = SplitBlockPredecessors(BB, Preds, "endcf.split", DT, LI, nullptr,
                                false);
  }

  Value *Exec = popSaved();
  Instruction *FirstInsertionPt = &*BB->getFirstInsertionPt();
  if (!isa<UndefValue>(Exec) && !isa<UnreachableInst>(FirstInsertionPt)) {
    Instruction *ExecDef = cast<Instruction>(Exec);
    BasicBlock *DefBB = ExecDef->getParent();
    if (!DT->dominates(DefBB, BB)) {
      // Split edge to make Def dominate Use
      FirstInsertionPt = &*SplitEdge(DefBB, BB, DT, LI)->getFirstInsertionPt();
    }
    CallInst *End = CallInst::Create(EndCf, Exec, "", FirstInsertionPt);
    Instruction *FirstNonPHI = BB->getFirstNonPHI();
    // BB has PHINode
    if (FirstNonPHI != &*BB->begin()) {
      std::vector<llvm::PHINode *> philist;
      for (llvm::PHINode &Phi : BB->phis())
        philist.push_back(&Phi);
      for (llvm::PHINode *PHI : philist) {
        if (DA->isDivergent(PHI)) {
          Type *Ty = PHI->getIncomingValue(0)->getType();
          MergeCF = Intrinsic::getDeclaration(BB->getParent()->getParent(),
                                            Intrinsic::blkv_merge_cf, {Ty});
          auto PHIPair = splitPhiNode(PHI, BB);
          SmallVector<Value *, 2> Args = {PHIPair.first, PHIPair.second};
          // insert before "blkv.end.cf"
          Instruction *InsertPt = End;
          CallInst *Merge = CallInst::Create(MergeCF, Args, "merge", InsertPt);
          PHI->replaceAllUsesWith(Merge);
          PHI->eraseFromParent();
        }
      }
    }
  }

  return true;
}

/* Function: split 1 phi to 2 phi
*     %.sink = phi float [ %7, %Flow ], [ %mul, %if.then ]
*  ->
*     %.sink.phi0 = phi float [ %7, %Flow ], [ undef, %if.then ]
*     %.sink.phi1 = phi float [ undef, %Flow ], [ %mul, %if.then ]
*/
std::pair<PHINode *, PHINode *> LinxV5AnnotateControlFlow::splitPhiNode(llvm::PHINode *OriginPHI,
                                             BasicBlock *InsertBB) {
  Type *Ty = OriginPHI->getIncomingValue(0)->getType();
  StringRef BaseName = OriginPHI->getName();

  PHINode *PHIFirst = PHINode::Create(
      Ty, 2, Twine(BaseName) + ".phi0",
      InsertBB->empty() ? InsertBB->getTerminator() : &*InsertBB->begin());
  PHINode *PHISecond = PHINode::Create(
      Ty, 2, Twine(BaseName) + ".phi1",
      InsertBB->empty() ? InsertBB->getTerminator() : &*InsertBB->begin());

  PHIFirst->addIncoming(OriginPHI->getIncomingValue(0),
                        OriginPHI->getIncomingBlock(0));
  if (DT->dominates(OriginPHI->getIncomingBlock(1), InsertBB))
    PHIFirst->addIncoming(UndefValue::get(Ty), OriginPHI->getIncomingBlock(1));
  else
    PHIFirst->addIncoming(OriginPHI->getIncomingValue(0), OriginPHI->getIncomingBlock(1));

  if (DT->dominates(OriginPHI->getIncomingBlock(0), InsertBB))
    PHISecond->addIncoming(UndefValue::get(Ty), OriginPHI->getIncomingBlock(0));
  else
    PHISecond->addIncoming(OriginPHI->getIncomingValue(1), OriginPHI->getIncomingBlock(0));
  PHISecond->addIncoming(OriginPHI->getIncomingValue(1), OriginPHI->getIncomingBlock(1));

  bool FirstPredDominates =
      DT->dominates(OriginPHI->getIncomingBlock(0), InsertBB);
  bool SecondPredDominates =
      DT->dominates(OriginPHI->getIncomingBlock(1), InsertBB);
  if (FirstPredDominates && !SecondPredDominates)
    return {PHISecond, PHIFirst};
  return {PHIFirst, PHISecond};
}

/// Annotate the control flow with intrinsics so the backend can
/// recognize if/then/else and loops.
bool LinxV5AnnotateControlFlow::runOnFunction(Function &F) {

  DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  DA = &getAnalysis<LegacyDivergenceAnalysis>();
  TargetPassConfig &TPC = getAnalysis<TargetPassConfig>();
  const TargetMachine &TM = TPC.getTM<TargetMachine>();

  bool Changed = false;
  initialize(*F.getParent(), TM.getSubtarget<LinxV5Subtarget>(F));
  // run a depth first traversal on the structurized CFG, where each conditional branch
  // forms a triangle. And successor 0 finally reaches successor 1.
  // 1. if a block has 2 successors (succ0, succ1), generate a blkv_if intrinsic,
  //    then push succ1 to the stack
  // 2. in any case where we visit a block which is at the top of the stack,
  //    2.1 if the BB does not have conditional branch, we meet the converge point.
  //        Just close the control flow.
  //    2.2.1 if the BB has conditional branch, and the successor 1 is visited before, we may meet a latch block.
  //        Close the control flow first.
  //        If successor 1 dominates current BB, current BB is a latch block. Annotate the loop.
  //    2.2.2 if the BB has conditional branch, but the successor 1 is not visited before, we are at the flow of a branch
  //          Insert the blkv_flow intrinsic.
  for (df_iterator<BasicBlock *> I = df_begin(&F.getEntryBlock()),
       E = df_end(&F.getEntryBlock()); I != E; ++I) {
    BasicBlock *BB = *I;
    BranchInst *Term = dyn_cast<BranchInst>(BB->getTerminator());

    if (!Term || Term->isUnconditional()) {
      if (isTopOfStack(BB))
        Changed |= closeControlFlow(BB);

      continue;
    }

    if (I.nodeVisited(Term->getSuccessor(1))) {
      if (isTopOfStack(BB))
        Changed |= closeControlFlow(BB);

      if (DT->dominates(Term->getSuccessor(1), BB))
        Changed |= handleLoop(Term);
      continue;
    }

    if (isTopOfStack(BB)) {
      PHINode *Phi = dyn_cast<PHINode>(Term->getCondition());
      if (Phi && Phi->getParent() == BB && isElse(Phi) && !hasKill(BB)) {
        Changed |= insertElse(Term);
        Changed |= eraseIfUnused(Phi);
        continue;
      }

      Changed |= closeControlFlow(BB);
    }

    Changed |= openIf(Term);
  }

  if (!Stack.empty()) {
    // CFG was probably not structured.
    report_fatal_error("failed to annotate CFG");
  }

  return Changed;
}

/// Create the annotation pass
FunctionPass *llvm::createLinxV5AnnotateControlFlowPass() {
  return new LinxV5AnnotateControlFlow();
}
