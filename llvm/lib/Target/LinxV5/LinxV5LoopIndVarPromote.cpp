//===- LinxV5LoopIndVarPromote.cpp - Loop Induction Variable Promote Pass ===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass promote the evolution of the induction variable to the earliest
// possible position of the loop body.
//
// Currently, the only indvar pattern is "%indvar.next = %indvar + %invariant",
// or "%indvar.next = %indvar - %invariant", where the "%indvar" is a PHI node
// and "%invariant" is a loop invariant.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

using namespace llvm;

#define LINX_LIVP_OPTION "linx-loop-indvar-promote"
#define DEBUG_TYPE LINX_LIVP_OPTION
#define LINX_LIVP_NAME "LinxV5 Induction Variable Promote"

namespace {

class LinxV5LoopIndVarPromote : public MachineFunctionPass {
public:
  static char ID;

  LinxV5LoopIndVarPromote()
      : MachineFunctionPass(ID), MLI(nullptr), MRI(nullptr), TII(nullptr) {
    initializeLinxV5LoopIndVarPromotePass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return LINX_LIVP_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  MachineLoopInfo *MLI;
  MachineRegisterInfo *MRI;
  const LinxV5InstrInfo *TII;

  /// Find the register that contains the loop controlling induction variable.
  /// If successful, it will return true and set the \p IndReg, and \p IVOp
  /// arguments.  Otherwise it will return false.
  /// The returned induction register is the register R that follows the
  /// following induction pattern:
  /// loop:
  ///   R = phi ..., [ R.next, LatchBlock ]
  ///   R.next = R +/- #invariant
  ///   if (R.next < #N) goto loop
  /// #invariant is the loop invariant added/subbed to R,
  /// and IVOp is the instruction "R.next = R +/- #invariant".
  bool findInductionRegister(MachineLoop *ML, unsigned &IndReg,
                             MachineInstr *&IVOp) const;

  bool getPromotableMIs(MachineLoop *ML, MachineInstr *IVOp,
                        SmallVector<MachineInstr *, 4> &PromotableMIs);

  bool checkForLoopInvariant(MachineLoop *ML, const MachineOperand &MO) const;

  bool isIndVarGenerateOp(MachineInstr *MI) const;
};

} // end anonymous namespace

char LinxV5LoopIndVarPromote::ID;

INITIALIZE_PASS_BEGIN(LinxV5LoopIndVarPromote, LINX_LIVP_OPTION, LINX_LIVP_NAME,
                      false, false)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(LinxV5LoopIndVarPromote, LINX_LIVP_OPTION, LINX_LIVP_NAME,
                    false, false)

bool LinxV5LoopIndVarPromote::checkForLoopInvariant(
    MachineLoop *ML, const MachineOperand &MO) const {
  if (MO.isImm())
    return true;

  Register Reg = MO.getReg();
  if (!Reg.isPhysical()) {
    MachineInstr &DefMI = *MRI->getVRegDef(Reg);
    return ML->isLoopInvariant(DefMI);
  }
  return false;
}

bool LinxV5LoopIndVarPromote::isIndVarGenerateOp(MachineInstr *MI) const {
  switch (MI->getOpcode()) {
  case LinxV5::PseudoVBXADD:
  case LinxV5::PseudoVBXADDW:
  case LinxV5::PseudoVBXSUB:
  case LinxV5::PseudoVBXSUBW:
    return true;
  default:
    return false;
  }
}

// Find the following pattern (BrReg and UpdReg can be the same one):
//    %IndReg = PHI UpdReg, PhiOpReg2
//    %UpdReg = ADD/SUB %IndReg, %Invariant
//    %BrReg  = ADD/SUB %IndReg, %Invariant
//    br %BrReg, %Foo, %Bar
// TODO: support the %UpdReg = ADD %Invariant, %IndReg pattern
bool LinxV5LoopIndVarPromote::findInductionRegister(MachineLoop *ML,
                                                    unsigned &IndRegOut,
                                                    MachineInstr *&IVOp) const {
  MachineBasicBlock *Header = ML->getHeader();
  MachineBasicBlock *Latch = ML->getLoopLatch();
  MachineBasicBlock *ExitingBlock = ML->findLoopControlBlock();
  if (!Header || !Latch || !ExitingBlock)
    return false;

  using InductionMap = std::map<Register, std::vector<Register>>;
  InductionMap IndMap;

  for (auto I = Header->instr_begin(), E = Header->instr_end();
       I != E && I->isPHI(); ++I) {
    MachineInstr *Phi = &*I;

    // Have a PHI instruction.  Get the operand that corresponds to the
    // latch block, and see if is a result of form "reg+/-invariant",
    // where the "reg" is defined by the PHI node we are looking at.
    for (unsigned i = 1, n = Phi->getNumOperands(); i < n; i += 2) {
      if (Phi->getOperand(i + 1).getMBB() != Latch)
        continue;

      Register PhiOpReg = Phi->getOperand(i).getReg();
      MachineInstr *DI = MRI->getVRegDef(PhiOpReg);

      if (isIndVarGenerateOp(DI) && DI->getOperand(1).isReg()) {
        // If the register operand to the add/sub is the PHI we're looking at,
        // this meets the induction pattern.
        Register IndReg = DI->getOperand(1).getReg();
        MachineOperand &Opnd2 = DI->getOperand(2);
        if (MRI->getVRegDef(IndReg) == Phi &&
            checkForLoopInvariant(ML, Opnd2)) {
          Register UpdReg = DI->getOperand(0).getReg();
          if (IndMap.find(IndReg) == IndMap.end()) {
            std::vector<Register> UpdRegVec = {UpdReg};
            IndMap.insert(std::make_pair(IndReg, UpdRegVec));
          } else {
            IndMap[IndReg].push_back(UpdReg);
          }
        }
      }
    } // for (i)
  }   // for (instr)

  // Find the predicate regs that leads to the exiting of loop
  // Cond[3] = {BranchOp, Reg1, Reg2/Imm}
  SmallVector<MachineOperand, 3> Cond;
  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  bool NotAnalyzed = TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false);
  // If the ExitingBlock exits with an unconditional branch/not a branch,
  // there will be no induction register
  if (NotAnalyzed || FB == nullptr)
    return false;

  // Find the induction reg that relates to the branch
  for (auto &Item : IndMap) {
    if (Item.second.size() > 1)
      continue;
    Register IndReg = Item.first;
    for (auto &MI : MRI->use_instructions(IndReg)) {
      if (MI.getParent() != Latch || !isIndVarGenerateOp(&MI))
        continue;
      if (MI.getOperand(0).getReg() != Cond[1].getReg())
        continue;
      if ((MI.getOperand(1).getReg() == IndReg &&
           checkForLoopInvariant(ML, MI.getOperand(2))) ||
          (MI.getOperand(2).getReg() == IndReg &&
           checkForLoopInvariant(ML, MI.getOperand(1)))) {
        IndRegOut = IndReg;
        IVOp = MRI->getVRegDef(Item.second[0]);
        return true;
      }
    }
  }

  return false;
}

bool LinxV5LoopIndVarPromote::getPromotableMIs(
    MachineLoop *ML, MachineInstr *IVOp,
    SmallVector<MachineInstr *, 4> &PromotableMIs) {
  SmallVector<MachineInstr *, 8> ToVisit = {IVOp};
  while (!ToVisit.empty()) {
    MachineInstr *MI = ToVisit.back();
    ToVisit.pop_back();
    PromotableMIs.push_back(MI);
    for (MachineOperand &MO : MI->operands()) {
      if (!MO.isReg() || MO.isDef())
        continue;
      Register Reg = MO.getReg();
      // Never promote a MI with physical regs
      if (Reg.isPhysical())
        return false;

      MachineInstr *DefMI = MRI->getVRegDef(Reg);
      // Never promote a PHI node which is not in the loop header
      if (DefMI->getOpcode() == LinxV5::PHI &&
          DefMI->getParent() != ML->getHeader())
        return false;

      if (ML->contains(DefMI) && DefMI->getOpcode() != LinxV5::PHI) {
        ToVisit.push_back(DefMI);
      }
    }
  }

  return true;
}

bool LinxV5LoopIndVarPromote::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;

  MRI = &MF.getRegInfo();
  MLI = &getAnalysis<MachineLoopInfo>();
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());

  for (MachineLoop *ML : *MLI) {
    unsigned IndReg;
    MachineInstr *IVOp;

    LLVM_DEBUG(dbgs() << "\nTry promoting Loop IndVar in: "; ML->dump(););

    if (!findInductionRegister(ML, IndReg, IVOp)) {
      LLVM_DEBUG(dbgs() << "No induction variable found!\n";);
      continue;
    }

    LLVM_DEBUG(dbgs() << "IndVar is identified as: "; IVOp->dump(););

    SmallVector<MachineInstr *, 4> PromotableMIs;
    if (!getPromotableMIs(ML, IVOp, PromotableMIs))
      continue;

    LLVM_DEBUG(dbgs() << PromotableMIs.size()
                      << " IndVar related instructions will be promoted:\n";
               for (auto &MI
                    : PromotableMIs) { MI->dump(); });

    // Promote the indvar related instructions to the top of the loop header
    MachineBasicBlock *Header = ML->getHeader();
    auto MovePos = Header->getFirstNonPHI();
    for (auto &MI : PromotableMIs) {
      Header->splice(MovePos, MI->getParent(), MI->getIterator());
      MovePos = MI->getIterator();
    }

    // Update liveins
    for (auto &MBB : reverse(ML->getBlocks())) {
      recomputeLiveIns(*MBB);
    }

    Changed = true;
  }
  return Changed;
}

FunctionPass *llvm::createLinxV5LoopIndVarPromotePass() {
  return new LinxV5LoopIndVarPromote();
}
