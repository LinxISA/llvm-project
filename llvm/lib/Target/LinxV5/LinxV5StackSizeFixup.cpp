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
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "linxv5-stack-size-fixup"
#define PASS_NAME "LinxV5 Stack Size Fixup"

namespace {

static unsigned getDefRegSize(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              Register Reg, bool TraceDef = false);

static bool shouldTraceDefRegSize(const MachineFunction &MF, int FI,
                                  Register Reg) {
  return MF.getName() == "_Z17hkv_lookup_kernelILi64EEvPaPiPmPfii" &&
         FI == 7 && Reg == LinxV5::SIMT_VN2;
}

static void traceDefMI(const char *Tag, MachineInstr &MI) {
  errs() << Tag;
  MI.print(errs());
  errs() << "\n";
}

static unsigned getRegSizeFromSlotSize(unsigned SlotSizeInBytes,
                                       unsigned LaneNum) {
  if (SlotSizeInBytes < LaneNum)
    return 0;
  return SlotSizeInBytes / LaneNum;
}

static unsigned getDefRegSizeAtSingleBlock(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MBBI,
                                           Register Reg, bool &FindDef,
                                           bool TraceDef = false) {
  if (TraceDef)
    errs() << "[def-trace] scan-bb=" << MBB.getNumber()
           << " reg=" << printReg(
                  Reg, MBB.getParent()->getSubtarget<LinxV5Subtarget>()
                           .getRegisterInfo())
           << "\n";
  for (MachineInstr &MI : reverse(make_range(MBB.begin(), MBBI))) {
    if (!MI.definesRegister(Reg))
      continue;

    FindDef = true;
    if (TraceDef)
      traceDefMI("[def-trace] hit-def: ", MI);

    if (MI.getOpcode() == TargetOpcode::COPY) {
      Register SrcReg = MI.getOperand(1).getReg();
      if (TraceDef)
        errs() << "[def-trace] follow-copy src="
               << printReg(
                      SrcReg,
                      MBB.getParent()->getSubtarget<LinxV5Subtarget>()
                          .getRegisterInfo())
               << "\n";
      return getDefRegSize(MBB, MI.getIterator(), SrcReg, TraceDef);
    }

    if (MI.getOpcode() == LinxV5::PseudoVecReload) {
      if (TraceDef)
        errs() << "[def-trace] width-from-pseudoreload="
               << MI.getOperand(1).getImm() << "\n";
      return MI.getOperand(1).getImm();
    }

    if (MI.getOpcode() == LinxV5::LinxV5ImplicitDef) {
      if (TraceDef)
        errs() << "[def-trace] width-from-implicitdef="
               << LinxV5::getSizeFromSIMTType(MI.getOperand(1).getImm())
               << "\n";
      return LinxV5::getSizeFromSIMTType(MI.getOperand(1).getImm());
    }

    unsigned NumDefs = MI.getNumExplicitDefs();
    if (NumDefs < MI.getNumOperands() && MI.getOperand(NumDefs).isImm()) {
      if (TraceDef)
        errs() << "[def-trace] width-from-def-type-imm="
               << LinxV5::getSizeFromSIMTType(MI.getOperand(NumDefs).getImm())
               << "\n";
      return LinxV5::getSizeFromSIMTType(MI.getOperand(NumDefs).getImm());
    }

    if (TraceDef)
      errs() << "[def-trace] def-found-but-no-width\n";
    return 0;
  }
  if (TraceDef)
    errs() << "[def-trace] no-def-in-bb=" << MBB.getNumber() << "\n";
  return 0;
}

static unsigned getDefRegSize(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              Register Reg, bool TraceDef) {
  bool FindDef = false;
  if (TraceDef)
    errs() << "[def-trace] begin mf=" << MBB.getParent()->getName()
           << " start-bb=" << MBB.getNumber()
           << " reg=" << printReg(
                  Reg, MBB.getParent()->getSubtarget<LinxV5Subtarget>()
                           .getRegisterInfo())
           << "\n";
  unsigned RegSize =
      getDefRegSizeAtSingleBlock(MBB, MBBI, Reg, FindDef, TraceDef);
  if (TraceDef)
    errs() << "[def-trace] first-result size=" << RegSize
           << " finddef=" << FindDef << "\n";
  if (RegSize || FindDef)
    return RegSize;

  std::queue<MachineBasicBlock *> Queue;
  SmallDenseSet<MachineBasicBlock *> Candidates;
  Candidates.insert(&MBB);
  for (MachineBasicBlock *Pred : MBB.predecessors()) {
    if (!Candidates.count(Pred)) {
      Queue.push(Pred);
      Candidates.insert(Pred);
    }
  }

  while (!Queue.empty()) {
    MachineBasicBlock *Top = Queue.front();
    Queue.pop();
    if (TraceDef)
      errs() << "[def-trace] visit-pred-bb=" << Top->getNumber() << "\n";
    FindDef = false;
    RegSize = getDefRegSizeAtSingleBlock(*Top, Top->end(), Reg, FindDef,
                                         TraceDef);
    if (TraceDef)
      errs() << "[def-trace] pred-result bb=" << Top->getNumber()
             << " size=" << RegSize
             << " finddef=" << FindDef << "\n";
    if (RegSize || FindDef)
      return RegSize;
    for (MachineBasicBlock *Pred : Top->predecessors()) {
      if (!Candidates.count(Pred)) {
        Queue.push(Pred);
        Candidates.insert(Pred);
      }
    }
  }

  if (TraceDef)
    errs() << "[def-trace] no-def-found-anywhere\n";
  return 0;
}

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
      // Learn the true vector width from the spilled value itself, then
      // normalize the frame object size so later spill/reload expansion can
      // uniformly derive width from FI like tile spills do.
      if (MI.getOpcode() == LinxV5::PseudoVecSpill) {
        Register Reg = MI.getOperand(0).getReg();
        bool IsVec = !LinxV5::SIMTCGSRegClass.contains(Reg);
        int FI = MI.getOperand(2).getIndex();
        bool TraceDef = shouldTraceDefRegSize(MF, FI, Reg);
        unsigned OldSlotSizeInBytes = MFI->getObjectSize(MI.getOperand(2).getIndex());
        unsigned RegSizeInBytes =
            IsVec ? getDefRegSize(MBB, MI.getIterator(), Reg, TraceDef)
                  : LinxV5::SIMTRegSize::SIMT_REG_SIZE_D;
        errs() << "[spill-fixup:pre] MF=" << MF.getName()
               << " BB=" << MBB.getNumber()
               << " FI=" << FI
               << " Reg=" << printReg(Reg, TRI)
               << " IsVec=" << IsVec
               << " OldSlot=" << OldSlotSizeInBytes
               << " DefSize=" << RegSizeInBytes
               << " LaneNum=" << LaneNum << "\n";
        if (!IsVec) {
          if (MFI->getObjectSize(FI) != RegSizeInBytes) {
            MFI->setObjectSize(FI, RegSizeInBytes);
            Changed = true;
          }
          errs() << "[spill-fixup:post] MF=" << MF.getName()
                 << " BB=" << MBB.getNumber()
                 << " FI=" << FI
                 << " NewSlot=" << MFI->getObjectSize(FI) << "\n";
          continue;
        }

        if (!RegSizeInBytes)
          continue;

        unsigned SlotSizeInBytes = RegSizeInBytes * LaneNum;
        if (MFI->getObjectSize(FI) != SlotSizeInBytes) {
          MFI->setObjectSize(FI, SlotSizeInBytes);
          Changed = true;
        }
        errs() << "[spill-fixup:post] MF=" << MF.getName()
               << " BB=" << MBB.getNumber()
               << " FI=" << FI
               << " NewSlot=" << MFI->getObjectSize(FI) << "\n";
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // set width to 0 when spill/reload a scalar reg
      if (MI.getOpcode() == LinxV5::PseudoVecReload) {
        Register Reg = MI.getOperand(0).getReg();
        bool IsVec = !LinxV5::SIMTCGSRegClass.contains(Reg);
        int FI = MI.getOperand(2).getIndex();
        unsigned SlotSizeInBytes = MFI->getObjectSize(FI);
        unsigned RegSizeInBytes = 0;
        if (!IsVec) {
          RegSizeInBytes = 0;
        } else {
          RegSizeInBytes = getRegSizeFromSlotSize(SlotSizeInBytes, LaneNum);
          if (!RegSizeInBytes) {
            RegSizeInBytes = LinxV5::getUseRegSize(MBB, MI.getIterator(), Reg);
            if (RegSizeInBytes) {
              MFI->setObjectSize(FI, RegSizeInBytes * LaneNum);
              Changed = true;
            }
          }
        }
        errs() << "[reload-fixup] MF=" << MF.getName()
               << " BB=" << MBB.getNumber()
               << " FI=" << FI
               << " Reg=" << printReg(Reg, TRI)
               << " IsVec=" << IsVec
               << " SlotSize=" << MFI->getObjectSize(FI)
               << " Width=" << RegSizeInBytes
               << " NewSlot=" << MFI->getObjectSize(FI)
               << " LaneNum=" << LaneNum << "\n";
        MI.getOperand(1).ChangeToImmediate(RegSizeInBytes);
      }
    }
  }

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // set width to 0 when spill/reload a scalar reg
      if (MI.getOpcode() == LinxV5::PseudoVecSpill) {
        Register Reg = MI.getOperand(0).getReg();
        int FI = MI.getOperand(2).getIndex();
        if (LinxV5::SIMTCGSRegClass.contains(Reg)) {
          errs() << "[spill-width] MF=" << MF.getName()
                 << " BB=" << MBB.getNumber()
                 << " FI=" << FI
                 << " Reg=" << printReg(Reg, TRI)
                 << " SlotSize=" << MFI->getObjectSize(FI)
                 << " Width=0\n";
          MI.getOperand(1).ChangeToImmediate(0);
          continue;
        }

        unsigned SlotSizeInBytes = MFI->getObjectSize(FI);
        unsigned Width = getRegSizeFromSlotSize(SlotSizeInBytes, LaneNum);
        errs() << "[spill-width] MF=" << MF.getName()
               << " BB=" << MBB.getNumber()
               << " FI=" << FI
               << " Reg=" << printReg(Reg, TRI)
               << " SlotSize=" << SlotSizeInBytes
               << " Width=" << Width
               << " LaneNum=" << LaneNum << "\n";
        MI.getOperand(1).ChangeToImmediate(
            Width);
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
