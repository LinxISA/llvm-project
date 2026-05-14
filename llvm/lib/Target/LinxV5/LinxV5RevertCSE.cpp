//===----------------------- LinxV5RevertCSE.cpp -------------------------===//
//
// Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// A pass that reverts CSE based on some specific instruction sequence.
//   ......
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
#include <limits>
#include <set>
using namespace llvm;

#define DEBUG_TYPE "linx-revert-cse"
#define LINX_REVERT_CSE "LinxV5 Revert CSE"

namespace {

class LinxV5RevertCSE : public MachineFunctionPass {
public:
  static char ID;

  LinxV5RevertCSE() : MachineFunctionPass(ID) {
    initializeLinxV5RevertCSEPass(*PassRegistry::getPassRegistry());
  }
  virtual ~LinxV5RevertCSE() = default;

  StringRef getPassName() const override { return LINX_REVERT_CSE; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;

  static bool isOperandVirtualReg(const MachineOperand &Op) {
    return Op.isReg() && Op.getReg().isVirtual();
  }

  bool revertCSEForAddressComputation(MachineBasicBlock &MBB) const;

  bool
  duplicateInstructions(MachineBasicBlock &MBB,
                        std::vector<MachineInstr *> &DuplicateListStack) const;

  // Given an instruction MI, find its defs until it's not a COPY, and return
  // this non-COPY instruction. Also append all COPYs into DuplicateList.
  MachineInstr *
  skipCopyInstructions(MachineInstr *MI,
                       std::vector<MachineInstr *> &DuplicateList) const {
    MachineInstr *NonCopyMI = MI;
    while (NonCopyMI && NonCopyMI->isCopyLike()) {
      const MachineOperand &Imm = NonCopyMI->getOperand(1);
      if (!isOperandVirtualReg(Imm))
        return nullptr;
      DuplicateList.push_back(NonCopyMI);
      NonCopyMI = MRI->getVRegDef(Imm.getReg());
    }
    return NonCopyMI;
  }
};
char LinxV5RevertCSE::ID = 0;

bool LinxV5RevertCSE::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  MRI = &MF.getRegInfo();
  TII = MF.getSubtarget().getInstrInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    Changed |= revertCSEForAddressComputation(MBB);
  }
  return Changed;
}

// Revert the CSE'd instructions when accessing multiple array elements with
// the same base address across MBBs. Re-using already computed addresses from
// dominator block can lead to unnecessary dependencies between MBBs.
//
// For instance:
// .MBB1:
//   %baseAddr:tr = ......  ; this is the base address to access an array
//   %1:tr = CONST 700
//   %2:tr = ADD %baseAddr:tr, %1:tr
//   %3:tr = LW %2:tr, 0
//   ......
//   %101:tr = CONST 704
//   %102:tr = ADD %baseAddr:tr, %101:tr
//   %103:tr = LW %102:tr, 0
//   ......
//   %201:tr = CONST 708
//   %202:tr = ADD %baseAddr:tr, %201:tr
//   %203:tr = LW %202:tr, 0
//   ......
//
// .MBB2:      ; Dominatoed by .MBB1; accessing the same loactions as .MBB1
//   %1000:tr = LW %2
//   ......
//   %1100:tr = LW %102
//   ......
//   %1200:tr = LW %202
//   ......
//
// .MBB2 above is the result of MachineCSE pass and will simply re-use the
// precomputed pointer values from .MBB1. This will greatly increase the
// register pressure.
//
// In this scenario, because all the memory locations come from the same
// base address (plus a constant offset), we can revert the CSE'd instructions
// so that the "%baseAddr" value becomes the only dependency between blocks.
// Thus after reverting CSE'd instructions for .MBB2, it becomes:
//
// .MBB2:        ; only depends on "%baseAddr" now from .MBB1
//   %5000:tr = CONST 700
//   %5001:tr = ADD %baseAddr:tr, %5000:tr
//   %1000:tr = LW %5001:tr, 0
//   ......
//   %5100:tr = CONST 704
//   %5101:tr = ADD %baseAddr:tr, %5100:tr
//   %1100:tr = LW %5101:tr, 0
//   ......
//   %5200:tr = CONST 708
//   %5201:tr = ADD %baseAddr:tr, %5200:tr
//   %1200:tr = LW %5201:tr, 0
bool LinxV5RevertCSE::revertCSEForAddressComputation(
    MachineBasicBlock &MBB) const {
  DenseMap<Register, SmallVector<MachineInstr *, 8>> BaseAddr2Users;
  DenseMap<MachineInstr *, std::vector<MachineInstr *>> RevertMap;

  SmallSet<Register, 2> VisitedPtrOp;
  for (MachineInstr &MI : MBB) {
    // Looking for load/store instructions only
    unsigned int OpCode = MI.getOpcode();
    unsigned int PtrOpIndex;
    if (OpCode == LinxV5::PseudoVBXLD || OpCode == LinxV5::PseudoVBXLW ||
        OpCode == LinxV5::PseudoVBXLWU)
      PtrOpIndex = 1;
    else if (OpCode == LinxV5::PseudoVBXSD || OpCode == LinxV5::PseudoVBXSW)
      PtrOpIndex = 2;
    else
      continue;

    // Skip if pointer operand isn't a virt reg, or we have already visited it
    MachineOperand &PtrOp = MI.getOperand(PtrOpIndex);
    if (!isOperandVirtualReg(PtrOp) ||
        !VisitedPtrOp.insert(PtrOp.getReg()).second)
      continue;

    std::vector<MachineInstr *> TempCandidateInsts;
    TempCandidateInsts.reserve(4);

    MachineInstr *DefMI = skipCopyInstructions(MRI->getVRegDef(PtrOp.getReg()),
                                               TempCandidateInsts);
    // The def-use chain from ADD instruction to current MI may be propagated
    // via some COPY instructions (due to bitcast instructions from mid-end).
    // Collect all such COPY if any, since they need removing too once we revert
    // CSE for this MI.
    if (!DefMI || DefMI->getParent() == &MBB)
      continue;

    // Check if the opinter operand is defined by an ADD instruction, and only
    // one of ADD's operands is a constant (either is an immediate operand or
    // defined by a PseudoVBXCONST).
    unsigned int AddInstrOp = DefMI->getOpcode();
    if (AddInstrOp == LinxV5::PseudoVBXADD ||
        AddInstrOp == LinxV5::PseudoVBXADDW) {
      // Both operands must be virtual registers
      MachineOperand &BaseAddrOp = DefMI->getOperand(1);
      MachineOperand &ConstOp = DefMI->getOperand(2);
      if (!isOperandVirtualReg(BaseAddrOp) || !isOperandVirtualReg(ConstOp))
        continue;
      TempCandidateInsts.push_back(DefMI);

      // Second operand must be defined by a CONST/LCONST instruction
      MachineInstr *ConstMI = skipCopyInstructions(
          MRI->getVRegDef(ConstOp.getReg()), TempCandidateInsts);
      if (!ConstMI || ConstMI->getOpcode() != LinxV5::PseudoVBXCONST)
        continue;
      TempCandidateInsts.push_back(ConstMI);
      RevertMap[&MI] = std::move(TempCandidateInsts);

      BaseAddr2Users[BaseAddrOp.getReg()].push_back(&MI);
    }
  }

  // Remove some candidates: reverting CSE for address computation is
  // beneficial only if one base address register is used to compute
  // multiple DIFFERENT pointer operands. Multiple load or store
  // instructions using the same pointer operand register is considered as 1
  // user only altogether for the base address, which is already filtered
  // out by the "Visited" set.
  bool Changed = false;
  for (auto It = BaseAddr2Users.begin(), E = BaseAddr2Users.end(); It != E;
       ++It) {
    SmallVectorImpl<MachineInstr *> &UserMIs = It->second;
    if (UserMIs.size() > 1) {
      for (MachineInstr *UI : UserMIs)
        Changed |= duplicateInstructions(MBB, RevertMap[UI]);
    }
  }

  return Changed;
}

bool LinxV5RevertCSE::duplicateInstructions(
    MachineBasicBlock &MBB,
    std::vector<MachineInstr *> &DuplicateListStack) const {
  SmallVector<MachineInstr *, 2> RedundantCopy;

  // In MBB, find the earlist user of last duplicate instruction as insert
  // point. However we must be careful that if the first user is a PHI node,
  // then we abort the CSE revertion since we can't insert any
  // instruction before it
  MachineOperand &LastDupInstDefMO = DuplicateListStack[0]->getOperand(0);
  assert(isOperandVirtualReg(LastDupInstDefMO) &&
         "Only virtual reg is allowed!");
  Register OldReg = LastDupInstDefMO.getReg();
  bool IsKill = LastDupInstDefMO.isKill();
  MachineInstr *InsertPoint = nullptr;
  for (MachineInstr &MI : MBB) {
    if (MI.findRegisterUseOperandIdx(OldReg, IsKill) != -1) {
      if (MI.isPHI())
        return false;
      InsertPoint = &MI;
      break;
    }
  }
  assert(InsertPoint && "Last duplicate instruction must have a non-PHI user!");

  // Start duplicating all collected instructions before InsertPoint
  const DebugLoc &DL = InsertPoint->getDebugLoc();
  Register PrevNewReg, NewReg;
  const MachineOperand *PrevInstDef = nullptr;
  while (!DuplicateListStack.empty()) {
    MachineInstr *OriginalInst = DuplicateListStack.back();
    DuplicateListStack.pop_back();

    const MachineOperand &DefMO = OriginalInst->getOperand(0);
    assert(isOperandVirtualReg(DefMO) &&
           "Def operand of instruction should all be virtual regs!");
    Register Reg = DefMO.getReg();
    if (OriginalInst->isCopyLike()) {
      // remove copy with only 1 user
      if (MRI->hasOneNonDBGUse(Reg))
        RedundantCopy.push_back(OriginalInst);
      continue;
    }

    PrevNewReg = std::exchange(NewReg, MRI->cloneVirtualRegister(Reg));
    MachineInstr &DupInst =
        TII->duplicate(MBB, InsertPoint->getIterator(), *OriginalInst);
    DupInst.getOperand(0).setReg(NewReg);
    DupInst.setDebugLoc(DL);

    if (PrevInstDef) {
      // replace use of register from original instruction to NewReg
      MachineOperand *OpToChange = DupInst.findRegisterUseOperand(
          PrevInstDef->getReg(), PrevInstDef->isKill());
      OpToChange->setReg(PrevNewReg);
    }
    PrevInstDef = &DefMO;
  }

  // Update all users of MO in current block to use NewReg
  for (MachineOperand &Op : make_early_inc_range(MRI->use_operands(OldReg))) {
    if (Op.getParent()->getParent() == &MBB)
      Op.setReg(NewReg);
  }

  // Remove all redundant copy instructions
  for (MachineInstr *CopyMI : RedundantCopy)
    CopyMI->eraseFromParent();
  return true;
}

} // namespace

INITIALIZE_PASS(LinxV5RevertCSE, DEBUG_TYPE, LINX_REVERT_CSE, false, false)

namespace llvm {

FunctionPass *createLinxV5RevertCsePass() { return new LinxV5RevertCSE(); }

} // namespace llvm