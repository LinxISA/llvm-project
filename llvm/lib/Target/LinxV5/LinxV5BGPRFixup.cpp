// ===--------------------- LinxV5BGPRFixup.cpp ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===---------------------------------------------------------------------===//
//
// This pass is to eliminate illegal bgpr uses intra block.
// According to LinxV5.3:
// * One block can only def bgpr once.
// * Allow bgpr read after write
// In this pass, illegal bgpr def and use will be eliminated.
// These illegal bgpr def/use generate by:
// * LiveIn/Outs analyze Pass
// * RA-Split, RA-Spill
// Here, we trace bgpr def and uses and eliminate bgpr multi-def and its uses.
// E.g:
// 0   gpr2 = ADDI %tr1, 1
// 20  ....
// 24  %tr2 = ADDI gpr2, 1
// 44  ...
// 48  gpr2 = ADDI %tr3, 1 // gpr2 multi def
//
// ==>
//
// 0   %tr9 = ADDI %tr1, 1
// 20  ....
// 24  %tr2 = ADDI %tr9, 1
// 44  ...
// 48  gpr2 = ADDI %tr3, 1
//
// Note: we eliminate bgpr multi-def by
// * reduce bgpr LiveRange, which avoid bgpr allocate fail.
// * create new tr LiveRange, which will be allocated at round-2 RA.
//
// ===---------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5TargetMachine.h"
#include <llvm/CodeGen/LiveIntervals.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineRegisterInfo.h>
#include <llvm/CodeGen/VirtRegMap.h>

using namespace llvm;

// Only control by wireless
static cl::opt<bool>
    EnableBGPRAWROpt("linxv5-enable-bgpr-raw-opt",
                     cl::desc("BGPR Read After Write Optimization"),
                     cl::init(false), cl::Hidden);

#define DEBUG_TYPE "linxv5-bgpr-fixup"
#define LINXV5_BGPR_FIXUP_NAME "LinxV5 BGPR Fixup"

// record def and its uses, do not include multi define.
struct DefUses {
  MachineOperand *DefOp;
  SmallVector<MachineOperand *> UsesOp;

  DefUses() : DefOp(nullptr) {}
  DefUses(MachineOperand *defop) : DefOp(defop) {}
};

struct BlockRegion {
  MachineBasicBlock &MBB;
  MachineBasicBlock::iterator Begin;
  MachineBasicBlock::iterator End;
  BlockRegion(MachineBasicBlock &mbb, MachineBasicBlock::iterator begin,
              MachineBasicBlock::iterator end)
      : MBB(mbb), Begin(begin), End(end) {}
  void dump() {
    dbgs() << ">> BlockRegion:";
    for (auto I = Begin, E = End; I != E; ++I) {
      dbgs() << "  " << *I;
    }
  }
};

namespace {

class LinxV5BGPRFixup : public MachineFunctionPass {
private:
  DenseMap<Register, SmallVector<DefUses>> GPRInfo;

public:
  static char ID;
  MachineFunction *MF;
  const TargetInstrInfo *TII;
  const TargetRegisterInfo *TRI;
  MachineRegisterInfo *MRI;
  LiveIntervals *LIS;
  VirtRegMap *VRM;
  LinxV5BGPRFixup() : MachineFunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.addRequired<VirtRegMap>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  bool runOnMachineFunction(MachineFunction &mf) override;
  StringRef getPassName() const override { return LINXV5_BGPR_FIXUP_NAME; }

  bool isRC(Register Reg, const TargetRegisterClass *RC) {
    return Reg.isPhysical() ? RC->contains(Reg) : MRI->getRegClass(Reg) == RC;
  }
  bool isBGPR(Register Reg) { return isRC(Reg, &LinxV5::GRRegClass); }

  Register createVirtReg(const TargetRegisterClass *RC) {
    Register Reg = MRI->createVirtualRegister(RC);
    VRM->grow();
    return Reg;
  }

  void releaseMemory() override { GPRInfo.clear(); }

  void collectBGPRInfo(BlockRegion &MBB);
  bool eliminateBGPRMultiDef(BlockRegion &MBB);
  void updateIntervals();
};
} // namespace

static void getBlockRegions(MachineBasicBlock &MBB,
                            SmallVectorImpl<BlockRegion> &Regions) {
  auto Begin = MBB.begin();
  for (auto I = MBB.begin(), E = MBB.end(); I != E; ++I) {
    auto &MI = *I;
    if (LinxV5::isIsolateInstr(MI)) {
      Regions.emplace_back(MBB, Begin, I);
      Regions.emplace_back(MBB, I, std::next(I));
      Begin = std::next(I);
    } else if (MI.isCall()) {
      Regions.emplace_back(MBB, Begin, std::next(I));
      Begin = std::next(I);
    }
  }
  if (Begin != MBB.end())
    Regions.emplace_back(MBB, Begin, MBB.end());
}

bool LinxV5BGPRFixup::runOnMachineFunction(MachineFunction &mf) {
  bool Changed = false;
  MF = &mf;
  TII = mf.getSubtarget().getInstrInfo();
  TRI = mf.getSubtarget().getRegisterInfo();
  MRI = &mf.getRegInfo();
  LIS = &getAnalysis<LiveIntervals>();
  VRM = &getAnalysis<VirtRegMap>();

  if (mf.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  if (!mf.getSubtarget<LinxV5Subtarget>().isWireless())
    EnableBGPRAWROpt = true;

  for (MachineBasicBlock &MBB : mf) {
    assert(!MBB.isMBBGroupMember() && "TODO: Support Hyper!");
    SmallVector<BlockRegion, 2> Regions;
    getBlockRegions(MBB, Regions);
    for (auto &R : Regions) {
      LLVM_DEBUG(R.dump());
      collectBGPRInfo(R);
      Changed |= eliminateBGPRMultiDef(R);
    }
  }

  // rebuild LiveIntervals
  updateIntervals();
  return Changed;
}

void LinxV5BGPRFixup::collectBGPRInfo(BlockRegion &Region) {
  GPRInfo.clear();
  for (auto I = Region.Begin, E = Region.End; I != E; ++I) {
    auto &MI = *I;
    if (MI.isDebugInstr() || MI.isCFIInstruction() || MI.isInlineAsm() ||
        MI.isReturn())
      continue;

    if (MI.isImplicitDef() ||
        MI.isKill()) // do not care only interval matter instrs
      continue;

    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isUse() || MO.isImplicit())
        continue;
      Register Reg = MO.getReg();
      if (!isBGPR(Reg))
        continue;
      SmallVector<DefUses> &V = GPRInfo[Reg];
      if (V.empty()) {
        DefUses defUses;
        defUses.UsesOp.push_back(&MO);
        V.push_back(defUses);
      } else {
        DefUses &defUses = V.back();
        defUses.UsesOp.push_back(&MO);
      }
    }

    for (MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
        continue;
      Register Reg = MO.getReg();
      if (!isBGPR(Reg))
        continue;
      SmallVector<DefUses> &V = GPRInfo[Reg];
      // keep first elememt of V always record live-in uses.
      if (V.empty()) {
        DefUses defUses;
        V.push_back(defUses);
      }
      DefUses defUses(&MO);
      V.push_back(defUses);
    }

    if (MI.getOpcode() == LinxV5::ADJCALLSTACKDOWN ||
        MI.getOpcode() == LinxV5::ADJCALLSTACKUP) {
      // Do not care about sp multi-def.
      // Cause Block Split Pass will avoid set sp twice.
      // Maybe do some check here?
    }
  }

  LLVM_DEBUG(bool hasPrintMBB = false; for (auto info
                                            : GPRInfo) {
    SmallVector<DefUses> &V = info.second;
    Register R = info.first;
    if (V.size() > 2) { // multi-def
      if (!hasPrintMBB) {
        dbgs() << Region.MBB << "\n"
               << "multi-def infos\n";
        hasPrintMBB = true;
      }
      dbgs() << "Reg: " << printReg(R, TRI) << "\n";
      for (unsigned i = 0, ei = V.size(); i < ei; ++i) {
        dbgs() << "  Def NO." << i << " use inst are:\n";
        SmallVector<MachineOperand *> &uses = V[i].UsesOp;
        for (unsigned j = 0, ej = uses.size(); j != ej; ++j) {
          dbgs() << "    " << *uses[j]->getParent() << "\n";
        }
      }
    }
  };);
}

bool LinxV5BGPRFixup::eliminateBGPRMultiDef(BlockRegion &Region) {
  bool Changed = false;

  unsigned MaxBGPRDefNum = EnableBGPRAWROpt ? 2 : 1;

  for (auto info : GPRInfo) {
    SmallVector<DefUses> &V = info.second;
    // V[0]: LiveIns use
    // V[1]: First Def use
    // V[2]: Second Def use
    // ...
    if (V.size() > MaxBGPRDefNum) { // multi-def
      for (unsigned i = 1, ei = V.size() - 1; i < ei; ++i) {
        Changed = true;
        MachineOperand *DefOp = V[i].DefOp;
        SmallVector<MachineOperand *> &uses = V[i].UsesOp;
        Register NewTR = createVirtReg(&LinxV5::MixedGPRRegClass);
        DefOp->setReg(NewTR);
        for (unsigned j = 0, ej = uses.size(); j != ej; ++j) {
          uses[j]->setReg(NewTR);
        }
      }
    }
  }

  LLVM_DEBUG(if (Changed) {
    dbgs() << "\nAfter multi-def eliminate\n" << Region.MBB << "\n";
  };);

  return Changed;
}

void LinxV5BGPRFixup::updateIntervals() {
  LIS->releaseMemory();
  LIS->runOnMachineFunction(*MF);
}

char LinxV5BGPRFixup::ID = 0;
INITIALIZE_PASS(LinxV5BGPRFixup, LINXV5_BGPR_FIXUP_NAME, "LinxV5 BGPR Fixup",
                false, false)

FunctionPass *llvm::createLinxV5BGPRFixupPass() { return new LinxV5BGPRFixup; }
