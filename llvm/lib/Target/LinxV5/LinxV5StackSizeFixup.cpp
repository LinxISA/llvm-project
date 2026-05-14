//===----------------------- LinxV5StackSizeFixup.cpp -------------------------===//
//
// Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
//
//===----------------------------------------------------------------------===//
//
// Calculate stack size for SIMT blocks with vector register spills
//
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
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "linxv5-stack-size-fixup"
#define PASS_NAME "LinxV5 Stack Size Fixup"

namespace {

class LinxV5StackSizeFixup : public MachineFunctionPass {
public:
  static char ID;

  LinxV5StackSizeFixup() : MachineFunctionPass(ID) {
    initializeLinxV5StackSizeFixupPass(*PassRegistry::getPassRegistry());
  }
  virtual ~LinxV5StackSizeFixup() = default;

  StringRef getPassName() const override { return PASS_NAME; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  const LinxV5RegisterInfo *TRI;
  MachineFrameInfo *MFI;
};
char LinxV5StackSizeFixup::ID = 0;

bool LinxV5StackSizeFixup::runOnMachineFunction(MachineFunction &MF) {
  if (!MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  MRI = &MF.getRegInfo();
  TII = MF.getSubtarget<LinxV5Subtarget>().getInstrInfo();
  TRI = MF.getSubtarget<LinxV5Subtarget>().getRegisterInfo();
  MFI = &MF.getFrameInfo();

  unsigned LaneNum = MF.getSubtarget<LinxV5Subtarget>().getLaneNum();

  // Adjust the stack slot size of the Tile register
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // set width to 0 when spill/reload a scalar reg
      if (MI.getOpcode() == LinxV5::PseudoVecReload) {
        Register Reg = MI.getOperand(0).getReg();
        bool IsVec = !LinxV5::SIMTCGSRegClass.contains(Reg);
        unsigned RegSizeInBytes =
            IsVec ? LinxV5::getUseRegSize(MBB, MI.getIterator(), Reg)
                  : LinxV5::SIMTRegSize::SIMT_REG_SIZE_D;
        MI.getOperand(1).ChangeToImmediate(IsVec ? RegSizeInBytes : 0);
        int FI = MI.getOperand(2).getIndex();
        MFI->setObjectSize(FI, IsVec ? RegSizeInBytes * LaneNum : RegSizeInBytes);
        Changed = true;
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // set width to 0 when spill/reload a scalar reg
      if (MI.getOpcode() == LinxV5::PseudoVecSpill) {
        int FI = MI.getOperand(2).getIndex();
        unsigned SlotSizeInBytes = MFI->getObjectSize(FI);
        //TODO: getUseRegSize here
        if (SlotSizeInBytes < LaneNum) {
          // Scalar Reg Reload
          MI.getOperand(1).ChangeToImmediate(0);
        } else {
          unsigned RegSizeInBytes = SlotSizeInBytes / LaneNum;
          MI.getOperand(1).ChangeToImmediate(RegSizeInBytes);
        }
      }
    }
  }

  return Changed;
}

} // namespace

INITIALIZE_PASS(LinxV5StackSizeFixup, DEBUG_TYPE, PASS_NAME, false, false)

namespace llvm {

FunctionPass *createLinxV5StackSizeFixupPass() { return new LinxV5StackSizeFixup(); }

} // namespace llvm
