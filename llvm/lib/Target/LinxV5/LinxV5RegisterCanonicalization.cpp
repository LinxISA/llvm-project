// ===---- LinxV5RegisterCanonicalization.cpp - Reg canonicalization ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ===----------------------------------------------------------------------===//
//
// Before this pass, all virt regs are TRs to simplify optimization of data
// flow. This pass is to rewrite TR to BGPR for MBBGroup LiveIns and outs.
//
// ===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveRangeEdit.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-reg-cannon"
#define LINX_BISA_REG_CANNON_NAME "LinxV5 Bisa Reg Cannon"
#define LINX_SCRUB_REGS_NAME "LinxV5 Scrub Regs"

namespace {
class LinxV5RegisterCanonicalization : public MachineFunctionPass {
public:
  static char ID;
  MachineRegisterInfo *MRI;
  const LinxV5RegisterInfo *TRI;
  const LinxV5InstrInfo *TII;
  LiveIntervals *LIS;
  DenseMap<unsigned, DenseSet<Register>> MBBBGPRCandidatesMap;
  LinxV5RegisterCanonicalization()
      : MachineFunctionPass(ID), MRI(nullptr), TRI(nullptr), TII(nullptr),
        LIS(nullptr) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  void releaseMemory() override { MBBBGPRCandidatesMap.clear(); }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return LINX_BISA_REG_CANNON_NAME; }

  void analyzeLiveInsOuts(MachineFunction &MF);
  void rewriteLiveInsOuts(MachineFunction &MF);
};
} // namespace

INITIALIZE_PASS(LinxV5RegisterCanonicalization, DEBUG_TYPE,
                LINX_BISA_REG_CANNON_NAME, false, false)
FunctionPass *llvm::createLinxV5RegisterCanonicalizationPass() {
  return new LinxV5RegisterCanonicalization();
}

char LinxV5RegisterCanonicalization::ID = 0;

static bool isRewriteRC(const TargetRegisterClass *RC,
                        const LinxV5RegisterInfo *TRI) {
  return RC == TRI->getSTDRC() || RC == &LinxV5::MixedGPRNoRARegClass;
}

/// If Virtual T Register satisfy:
/// * LiveInterval active at MBBGroup Entry Begin Slot.
/// * LiveInterval active at MBBGroup Exit End Slot.
/// should rewrite to Virtual BGPR Register.
/// This pass only ensure all liveIn and liveout value will be capture,
/// and then rewrite to BGPR. Note that it will also treat some part of
/// MBBGroup inner date as liveIn or liveout, but trust me, it is the cheap
/// way to do liveIn/Out analyze! It also does not matter generate BGPR
/// redefine in one MBBGroup, cause RA pass will also generate BGPR redefine
/// on RA_Split, and post-RA passes should handle this anyway.
void LinxV5RegisterCanonicalization::analyzeLiveInsOuts(MachineFunction &MF) {
  for (MachineBasicBlock &MBB : MF) {
    DenseSet<Register> BGPRCandidates;

    MachineBasicBlock *Entry = &MBB;
    MachineBasicBlock *Exit = &MBB;

    for (MachineInstr &MI : MBB) {
      if (MI.isDebugInstr())
        continue;
      for (MachineOperand &MO : MI.operands()) {
        if (!MO.isReg() || MO.isImplicit())
          continue;
        Register TReg = MO.getReg();
        if (TReg.isPhysical() || !isRewriteRC(MRI->getRegClass(TReg), TRI))
          continue;
        // 1. inline asm is one Inst one block, so every operand should be
        // BGPRCandidate.
        // 2. undef value can treat as live-in directly, cause LIS can not
        // capture it.
        if (MI.isInlineAsm() || MO.isUndef() || LinxV5::isTileOp(MI)) {
          BGPRCandidates.insert(TReg);
          continue;
        }
        LiveInterval &LI = LIS->getInterval(TReg);
        if (MO.isUse()) {
          if (LIS->isLiveInToMBB(LI, Entry))
            BGPRCandidates.insert(TReg);
        } else {
          assert(MO.isDef());
          if (LIS->isLiveOutOfMBB(LI, Exit))
            BGPRCandidates.insert(TReg);
        }
      }
    }

    MBBBGPRCandidatesMap.insert(
        std::make_pair(MBB.getNumber(), BGPRCandidates));
  }
}

void LinxV5RegisterCanonicalization::rewriteLiveInsOuts(MachineFunction &MF) {
  DenseMap<Register, Register> TR2BGPRMap;
  for (auto BGPRCandidates : MBBBGPRCandidatesMap) {
    for (Register TReg : BGPRCandidates.second) {
      assert(TReg.isVirtual() && isRewriteRC(MRI->getRegClass(TReg), TRI));
      if (TR2BGPRMap.count(TReg))
        continue;
      TR2BGPRMap.insert(std::make_pair(
          TReg, MRI->createVirtualRegister(&LinxV5::GRRegClass)));
    }
  }

  auto isTargetOp = [&TR2BGPRMap, this](MachineOperand &Op) {
    if (!Op.isReg() || Op.isImplicit())
      return false;
    Register TReg = Op.getReg();
    if (TReg.isPhysical() || !isRewriteRC(MRI->getRegClass(TReg), TRI))
      return false;
    if (!TR2BGPRMap.count(TReg))
      return false;
    return true;
  };

  LLVM_DEBUG(dbgs() << " ********************* Rewrite TR to BGPR "
                       "*********************\n");

  for (MachineBasicBlock &MBB : MF) {
    LLVM_DEBUG(dbgs() << "\nBefore write:\n"; MBB.dump(););
    for (MachineInstr &MI : make_early_inc_range(MBB)) {
      if (MI.isDebugInstr())
        continue;

      if (MI.isCopy() || MI.isInlineAsm() || LinxV5::isTileOp(MI)) {
        for (MachineOperand &MO : make_early_inc_range(MI.operands())) {
          if (!isTargetOp(MO))
            continue;
          Register TReg = MO.getReg();
          MO.setReg(TR2BGPRMap.lookup(TReg));
        }
      } else if (MI.getOpcode() == TargetOpcode::IMPLICIT_DEF) {
        MachineOperand &MO = MI.getOperand(0);
        if (!isTargetOp(MO))
          continue;
        Register TReg = MO.getReg();
        BuildMI(MBB, std::next(MI.getIterator()), MI.getDebugLoc(),
                TII->get(TargetOpcode::IMPLICIT_DEF))
            .addReg(TR2BGPRMap.lookup(TReg), RegState::Define);
      } else {
        for (MachineOperand &MO : make_early_inc_range(MI.operands())) {
          if (!isTargetOp(MO))
            continue;
          Register TReg = MO.getReg();
          Register NewTR = MRI->createVirtualRegister(TRI->getSTDRC());
          if (MO.isUse()) {
            MachineInstr *Copy =
                BuildMI(MBB, MI.getIterator(), MI.getDebugLoc(),
                        TII->get(TargetOpcode::COPY))
                    .addReg(NewTR, RegState::Define)
                    .addReg(TR2BGPRMap.lookup(TReg));
            MO.setReg(NewTR);
            if (MO.isUndef()) {
              Copy->getOperand(1).setIsUndef();
              MO.setIsUndef(false);
            }
          } else {
            assert(MO.isDef());
            auto Pos = std::next(MI.getIterator());
            Register NewGPR = TR2BGPRMap.lookup(TReg);
            MO.setReg(NewTR);
            if (MI.getOpcode() == LinxV5::PseudoADDTPC_HI) {
              // addtpc do not write ra. lower to
              //   %mixedgprnora = addtpc xxx
              //   %grnora = COPY %mixedgprnora
              //   %gr = COPY %grnora
              // Then let RegCoalescer to reduce COPYs.
              NewTR = MRI->createVirtualRegister(&LinxV5::MixedGPRNoRARegClass);
              MO.setReg(NewTR);
              Register NoRA =
                  MRI->createVirtualRegister(&LinxV5::GRNoRARegClass);
              BuildMI(MBB, Pos, MI.getDebugLoc(), TII->get(LinxV5::COPY), NoRA)
                  .addReg(NewTR);
              NewTR = NoRA;
            }
            BuildMI(MBB, Pos, MI.getDebugLoc(), TII->get(TargetOpcode::COPY))
                .addReg(NewGPR, RegState::Define)
                .addReg(NewTR);
          }
        }
      }
    }

    LLVM_DEBUG(dbgs() << "\n\nAfter write:\n"; MBB.dump(););
  }

  LLVM_DEBUG(dbgs() << " ********************* ****************************** "
                       "*********************\n");
}

static DenseSet<const TargetRegisterClass *> KnownRCs = {
    // std
    &LinxV5::MixedGPRRegClass,     // general
    &LinxV5::MixedGPRNoRARegClass, // addtpc
    // inlineasm
    &LinxV5::GRRegClass,
    // tile-call
    &LinxV5::GRNoR0RegClass,       // gpr
    &LinxV5::Tile_ABSRegClass,     // tile
    &LinxV5::TILE_ABS_ACCRegClass, // acc
    // simt
    &LinxV5::SIMTCGVLRegClass, // general vector
    &LinxV5::SIMTCGVRegClass,  // general vector and CopyFromReg(ri/tile/lc)
    &LinxV5::SIMTCGSLRegClass, // general scalar
    &LinxV5::SIMTCGSRegClass,  // general scalar and CopyFromReg(ri/tile/lc)
    &LinxV5::CGSLRegClass      // 32-bits scalar: lui
};

void verifyRegClass(MachineRegisterInfo *MRI) {
  for (unsigned i = 0; i < MRI->getNumVirtRegs(); ++i) {
    unsigned VR = Register::index2VirtReg(i);
    if (MRI->reg_empty(VR))
      continue;
    const auto *RC = MRI->getRegClass(VR);
    if (!KnownRCs.count(RC)) {
      auto *TRI = MRI->getTargetRegisterInfo();
      report_fatal_error(Twine("unexpected RegClass ") +
                         TRI->getRegClassName(RC));
    }
  }
}

bool LinxV5RegisterCanonicalization::runOnMachineFunction(MachineFunction &MF) {
  MRI = &MF.getRegInfo();
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());
  TRI = MF.getSubtarget<LinxV5Subtarget>().getRegisterInfo();
  LIS = &getAnalysis<LiveIntervals>();

  verifyRegClass(MRI);

  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  analyzeLiveInsOuts(MF);

  rewriteLiveInsOuts(MF);

  return true;
}

namespace {
class LinxV5ScrubRegs : public MachineFunctionPass {
public:
  static char ID;
  MachineFunction *MF;
  LiveIntervals *LIS;
  MachineRegisterInfo *MRI;
  const TargetInstrInfo *TII;
  const LinxV5RegisterInfo *TRI;
  bool OnlyElimDeadDefs;
  explicit LinxV5ScrubRegs(bool dceonly = false)
      : MachineFunctionPass(ID), OnlyElimDeadDefs(dceonly) {}
  bool runOnMachineFunction(MachineFunction &mf) override {
    LIS = &getAnalysis<LiveIntervals>();
    MRI = &mf.getRegInfo();
    MF = &mf;
    TII = mf.getSubtarget().getInstrInfo();
    TRI = mf.getSubtarget<LinxV5Subtarget>().getRegisterInfo();
    return scrubAllRegDefs();
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool scrubAllRegDefs();
  bool separateUntiedUses(LiveInterval &LI);
  StringRef getPassName() const override { return LINX_SCRUB_REGS_NAME; }
};
} // namespace

char LinxV5ScrubRegs::ID = 0;
INITIALIZE_PASS(LinxV5ScrubRegs, DEBUG_TYPE, LINX_SCRUB_REGS_NAME, false, false)
FunctionPass *llvm::createLinxV5ScrubRegsPass(bool dceonly) {
  return new LinxV5ScrubRegs(dceonly);
}

/**
 * After TR->BGPR, regs are not very friendly for RA, scrub all:
 * 1. eliminate dead defs
 *   TR->BGPR does not recognise which instr is dead.
 * 2. split separated components
 *   TR->BGPR splits TR to differenct connected graphs.
 */
bool LinxV5ScrubRegs::scrubAllRegDefs() {
  bool Changed = false;
  unsigned NrVRegs = MRI->getNumVirtRegs();
  for (unsigned i = 0; i < NrVRegs; ++i) {
    Register Reg = Register::index2VirtReg(i);
    if (MRI->reg_empty(Reg))
      continue;
    LiveInterval &LI = LIS->getInterval(Reg);

    if (!OnlyElimDeadDefs) {
      // separate untied uses
      if (isRewriteRC(MRI->getRegClass(LI.reg()), TRI))
        Changed |= separateUntiedUses(LI);
    }

    // eliminate dead defs
    SmallVector<MachineInstr *, 2> Deads;
    LIS->shrinkToUses(&LI, &Deads);
    if (!Deads.empty())
      Changed = true;
    SmallVector<Register, 1> EmptyNewRegs;
    LiveRangeEdit(nullptr, EmptyNewRegs, *MF, *LIS, nullptr)
        .eliminateDeadDefs(Deads);

    if (!OnlyElimDeadDefs) {
      // split separated components
      SmallVector<LiveInterval *, 2> SplitLIs;
      LIS->splitSeparateComponents(LI, SplitLIs);
      if (SplitLIs.size() > 1)
        Changed = true;
    }
  }
  return Changed;
}

/**
 * A fix-up for ConnectedVNInfoEqClasses.
 * After RegCoalesc, we might get MIR like:
 *   %0:tr = op %0:tr, ..
 * One instr use and def a same virt reg without tied flag. While the
 * ConnectedVNInfoEqClasses treate this reg as tied and mark VNIs around the
 * instr as connected.
 * A simple fix-up is split the def and use to different instrs:
 *   %1:tr = COPY %0:tr
 *   %0:tr = op %1:tr, ..
 * This is not only for optimization. The LBGPR Optimizer do not expect
 * multi-def of a TR.
 */
bool LinxV5ScrubRegs::separateUntiedUses(LiveInterval &LI) {
  bool Changed = false;
  for (const auto *VNI : LI.valnos) {
    if (VNI->isUnused() || VNI->isPHIDef())
      continue;
    const auto *UVNI = LI.getVNInfoBefore(VNI->def);
    if (!UVNI)
      continue;
    auto *MI = LIS->getInstructionFromIndex(VNI->def);
    assert(MI && !MI->isInlineAsm() && MI->getNumExplicitDefs() > 0 &&
           "expect at least one def");
    MachineOperand &DefMO = MI->getOperand(0);
    assert(!DefMO.isImplicit() && "virt reg operand should be explicit");
    if (!DefMO.isReg() || !DefMO.isDef())
      continue;
    Register VDef = DefMO.getReg();
    if (VDef != LI.reg())
      continue;
    unsigned TiedUseOpIdx;
    if (MI->isRegTiedToUseOperand(0, &TiedUseOpIdx))
      continue;
    Register GapReg = MRI->createVirtualRegister(TRI->getSTDRC());
    for (auto &MO : MI->operands()) {
      if (MO.isReg() && MO.isUse() && MO.getReg() == VDef)
        MO.setReg(GapReg);
    }
    auto *MBB = MI->getParent();
    BuildMI(*MBB, MI, MI->getDebugLoc(), TII->get(TargetOpcode::COPY), GapReg)
        .addReg(VDef);
    LIS->InsertMachineInstrInMaps(*MI->getPrevNode());
    LIS->getInterval(GapReg);
    // no need to shrink LI, the next step of ScrubRegs will shrink it.
    Changed = true;
  }
  return Changed;
}
