//===--------------------- LinxV5SIMTSpillFixup.cpp -----------------------===//
//
// Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// Keep SIMT spills and reloads on the same side of a SIMT_P update when it is
// safe to do so within the same block.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-simt-spill-fixup"
#define PASS_NAME "LinxV5 SIMT Spill Fixup"

namespace {

class LinxV5SIMTSpillFixup : public MachineFunctionPass {
public:
  static char ID;

  LinxV5SIMTSpillFixup() : MachineFunctionPass(ID) {
    initializeLinxV5SIMTSpillFixupPass(*PassRegistry::getPassRegistry());
  }
  ~LinxV5SIMTSpillFixup() override = default;

  StringRef getPassName() const override { return PASS_NAME; }
  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const LinxV5InstrInfo *TII = nullptr;
  const LinxV5RegisterInfo *TRI = nullptr;

  bool fixupBlock(MachineBasicBlock &MBB);
  bool isSIMTPBoundary(const MachineInstr &MI) const;
  bool isSIMTSpillOrReload(const MachineInstr &MI) const;
  bool isSIMTSpill(const MachineInstr &MI) const;
  bool isSIMTReload(const MachineInstr &MI) const;
  bool canMoveAcrossBoundary(MachineInstr &MI,
                             MachineBasicBlock::iterator Boundary) const;
  const char *getReasonBlocked(MachineInstr &MI,
                               MachineBasicBlock::iterator Boundary) const;
};

char LinxV5SIMTSpillFixup::ID = 0;

bool LinxV5SIMTSpillFixup::isSIMTPBoundary(const MachineInstr &MI) const {
  return MI.modifiesRegister(LinxV5::SIMT_P, TRI);
}

bool LinxV5SIMTSpillFixup::isSIMTSpillOrReload(const MachineInstr &MI) const {
  return isSIMTSpill(MI) || isSIMTReload(MI);
}

bool LinxV5SIMTSpillFixup::isSIMTSpill(const MachineInstr &MI) const {
  return MI.getOpcode() == LinxV5::PseudoVecSpill;
}

bool LinxV5SIMTSpillFixup::isSIMTReload(const MachineInstr &MI) const {
  return MI.getOpcode() == LinxV5::PseudoVecReload;
}

bool LinxV5SIMTSpillFixup::canMoveAcrossBoundary(
    MachineInstr &MI, MachineBasicBlock::iterator Boundary) const {
  return !getReasonBlocked(MI, Boundary);
}

const char *LinxV5SIMTSpillFixup::getReasonBlocked(
    MachineInstr &MI, MachineBasicBlock::iterator Boundary) const {
  Register Reg = MI.getOperand(0).getReg();
  int FI = MI.getOperand(2).getIndex();

  MachineBasicBlock::iterator End = MI.getIterator();
  for (MachineBasicBlock::iterator It = std::next(Boundary); It != End; ++It) {
    MachineInstr &Cur = *It;

    if (Cur.mayLoadOrStore())
      return "intervening-memory";
    if (Cur.modifiesRegister(Reg, TRI))
      return "intervening-reg-clobber";
    if (isSIMTReload(MI) && Cur.readsRegister(Reg, TRI))
      return "intervening-reg-use";

    if (isSIMTSpillOrReload(Cur) && Cur.getOperand(2).getIndex() == FI)
      return "intervening-same-fi";
  }

  return nullptr;
}

bool LinxV5SIMTSpillFixup::fixupBlock(MachineBasicBlock &MBB) {
  bool Changed = false;
  MachineBasicBlock::iterator LastPBoundary = MBB.end();
  MachineBasicBlock::iterator FirstInsertPt = TII->getFirstInsertionPoint(MBB);

  for (auto It = MBB.begin(), End = MBB.end(); It != End;) {
    MachineInstr &MI = *It++;

    if (isSIMTPBoundary(MI)) {
      LastPBoundary = MI.getIterator();
      continue;
    }

    if (!isSIMTSpillOrReload(MI) || LastPBoundary == MBB.end())
      continue;

    errs() << "[simt-spill-fixup] consider MF=" << MBB.getParent()->getName()
           << " BB=" << MBB.getNumber()
           << " MI=" << (isSIMTSpill(MI) ? "spill" : "reload")
           << " FI=" << MI.getOperand(2).getIndex()
           << " Reg=" << printReg(MI.getOperand(0).getReg(), TRI)
           << " boundary-opc=" << TII->getName(LastPBoundary->getOpcode())
           << "\n";

    if (!canMoveAcrossBoundary(MI, LastPBoundary)) {
      errs() << "[simt-spill-fixup] blocked MF=" << MBB.getParent()->getName()
             << " BB=" << MBB.getNumber()
             << " FI=" << MI.getOperand(2).getIndex()
             << " reason=" << getReasonBlocked(MI, LastPBoundary) << "\n";
      continue;
    }

    bool InPrologue = false;
    for (auto PrologueIt = MBB.begin(); PrologueIt != FirstInsertPt;
         ++PrologueIt) {
      if (PrologueIt == LastPBoundary) {
        InPrologue = true;
        break;
      }
    }
    if (InPrologue) {
      errs() << "[simt-spill-fixup] blocked MF=" << MBB.getParent()->getName()
             << " BB=" << MBB.getNumber()
             << " FI=" << MI.getOperand(2).getIndex()
             << " reason=leading-prologue-boundary\n";
      continue;
    }

    errs() << "[simt-spill-fixup] move MF=" << MBB.getParent()->getName()
           << " BB=" << MBB.getNumber()
           << " FI=" << MI.getOperand(2).getIndex()
           << " before-opc=" << TII->getName(LastPBoundary->getOpcode())
           << "\n";
    MBB.splice(LastPBoundary, &MBB, MI.getIterator());
    Changed = true;
  }

  return Changed;
}

bool LinxV5SIMTSpillFixup::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()) ||
      !MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  TII = MF.getSubtarget<LinxV5Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget<LinxV5Subtarget>().getRegisterInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    Changed |= fixupBlock(MBB);
  return Changed;
}

} // namespace

INITIALIZE_PASS(LinxV5SIMTSpillFixup, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {

FunctionPass *createLinxV5SIMTSpillFixupPass() {
  return new LinxV5SIMTSpillFixup();
}

} // namespace llvm
