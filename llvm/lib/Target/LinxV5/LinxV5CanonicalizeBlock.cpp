//===---- LinxV5CanonicalizeBlock.cpp - Canonicalize LinxV5 MIR  ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass splits MBB according LinxV5's limitation, like CALL as
// terminator.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5TargetMachine.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetOptions.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-canon-block"
#define BISA_CANON_BLOCK_NAME "LinxV5 Canonicalize Block"
#define LINX_ISOLATE_INLINE_NAME "LinxV5 Isolate InlineASM Block"
#define LINX_FIXSGPRCOPIES_NAME "LinxV5 Fix SGPR Copies"

static cl::opt<unsigned>
    MAX_BLOCK_SIZE("linxv5-max-block-size",
                   cl::desc("Maximum instructions to consider block size"),
                   cl::init(100), cl::Hidden);

static cl::opt<bool> DisableDupConst("linxv5-disable-dup-const",
                                     cl::desc("Disable Duplicate Constant"),
                                     cl::init(false), cl::Hidden);

static cl::opt<bool> RematAddr("linxv5-enable-remat-addr", cl::init(false));

namespace {

class LinxV5CanonicalizeBlock : public MachineFunctionPass {
public:
  static char ID;
  bool runOnMachineFunction(MachineFunction &Fn) override;
  LinxV5CanonicalizeBlock(bool dupConstOnly = false)
      : MachineFunctionPass(ID), TII(nullptr), MRI(nullptr), DT(nullptr),
        DupConstOnly(dupConstOnly) {}

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::IsSSA);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return BISA_CANON_BLOCK_NAME; }

private:
  const LinxV5InstrInfo *TII;
  MachineRegisterInfo *MRI;
  MachineDominatorTree *DT;
  bool DupConstOnly;

  bool canonicalizeBlockWithTerminator(MachineFunction &Fn);
  void dupConstInterBlock(MachineFunction &MF);

  MachineInstr *getSplitPointWithSizeLimitation(MachineBasicBlock *MBB);
  bool canonicalBlock(MachineBasicBlock *MBB);
  bool splitBlockWithTerminator(MachineFunction &MF);
  bool canonicalizeBlockWithSizeLimitation(MachineFunction &Fn);
  bool splitBlockWithSPAccess(MachineFunction &MF);

  MachineBasicBlock *splitAt(MachineInstr &MI, bool MoveToNewBlock);
};

} // end anonymous namespace

static bool isInlineASMBlock(MachineBasicBlock &MBB) {
  if (MBB.empty())
    return false;
  return (MBB.begin()->isInlineAsm() && MBB.size() == 1);
}

static bool hasInlineASMEpilogue(MachineBasicBlock &MBB) {
  if (MBB.succ_size() != 1)
    return false;
  MachineBasicBlock *Succ = *MBB.succ_begin();
  if (Succ->pred_size() != 1)
    return false;
  // Succ cannot be a InlineASM Block
  return !isInlineASMBlock(*Succ);
}

static bool hasInlineASMPrologue(MachineBasicBlock &MBB) {
  if (MBB.pred_size() != 1)
    return false;
  MachineBasicBlock *Pred = *MBB.pred_begin();
  if (Pred->succ_size() != 1)
    return false;
  // Pred cannot be an InlineASM Block
  return !isInlineASMBlock(*Pred);
}

char LinxV5CanonicalizeBlock::ID = 0;
INITIALIZE_PASS(LinxV5CanonicalizeBlock, DEBUG_TYPE, BISA_CANON_BLOCK_NAME,
                false, false)

MachineBasicBlock *LinxV5CanonicalizeBlock::splitAt(MachineInstr &MI,
                                                    bool MoveToNewBlock) {
  MachineBasicBlock *MBB = MI.getParent();
  MachineFunction *MF = MBB->getParent();

  LivePhysRegs LiveRegs;
  LiveRegs.init(*MF->getSubtarget().getRegisterInfo());
  LiveRegs.addLiveIns(*MBB);
  MachineBasicBlock::iterator Prev(&MI);
  if (MoveToNewBlock)
    --Prev;
  for (auto I = MBB->rbegin(), E = Prev.getReverse(); I != E; ++I)
    LiveRegs.stepBackward(*I);

  MachineBasicBlock *SplitMBB =
      MF->CreateMachineBasicBlock(MBB->getBasicBlock());
  SplitMBB->setMBBGroupID(MBB->getMBBGroupID());

  MF->insert(std::next(MachineFunction::iterator(MBB)), SplitMBB);
  MachineBasicBlock::iterator SplitPoint(&MI);
  if (!MoveToNewBlock)
    ++SplitPoint;
  SplitMBB->splice(SplitMBB->begin(), MBB, SplitPoint, MBB->end());

  SplitMBB->transferSuccessorsAndUpdatePHIs(MBB);
  MBB->addSuccessor(SplitMBB);

  addLiveIns(*SplitMBB, LiveRegs);

  return SplitMBB;
}

bool LinxV5CanonicalizeBlock::splitBlockWithTerminator(MachineFunction &MF) {
  bool Changed = false;
  SmallVector<MachineBasicBlock *, 8> ToVisitBBs;

  for (MachineFunction::iterator I = MF.begin(), E = MF.end(); I != E; I++)
    ToVisitBBs.push_back(&*I);
  while (!ToVisitBBs.empty()) {
    MachineBasicBlock *MBB = ToVisitBBs.pop_back_val();
    for (auto I = MBB->rbegin(), E = MBB->rend(); I != E; I++) {
      MachineInstr &MI = *I;
      if (MI.isInlineAsm()) {
        bool didSplit = false;
        // handle after INLINEASM
        if (I != MBB->rbegin()) {
          // move instrs after this INLINEASM to a new MBB
          splitAt(MI, false);
          didSplit = true;
        }
        // now i am the last instr of MBB
        if (!hasInlineASMEpilogue(*MBB)) {
          // create a new epilogue block
          splitAt(MI, false);
          didSplit = true;
        }

        // handle before INLINEASM
        MachineBasicBlock *AsmMBB = MBB;
        if (&MI != std::prev(AsmMBB->rend())) {
          // move this INLINEASM instr to a new MBB
          AsmMBB = splitAt(MI, true);
          didSplit = true;
        }
        // now i am the first instr of AsmMBB
        if (!hasInlineASMPrologue(*AsmMBB)) {
          // move this INLINEASM instr to a new MBB, let the old MBB to be a new
          // prologue block
          AsmMBB = splitAt(MI, true);
          didSplit = true;
        }
        if (didSplit) {
          Changed = true;
          ToVisitBBs.push_back(MBB);
          break;
        }
      } else if (MI.isCall() || LinxV5::isTileOp(MI)) {
        if (I == MBB->rbegin())
          continue;
        MachineBasicBlock *NewMBB = MBB->splitAt(MI, true);
        if (NewMBB != MBB) {
          ToVisitBBs.push_back(MBB);
          Changed = true;
          break;
        }
      }
    }
  }
  if (Changed)
    MF.RenumberBlocks();
  return Changed;
}

/// Once sp is changed, split the block.
/// Unlike the LinxV5V3, LinxV5 do not reserve local sp.
/// So, if we not split block when sp changed, at PEI pass, we need to
/// scavenge T/U hand register to represent changed local sp/fp.
/// Register scavenge may fail at that time, and we do not want to hand that.
bool LinxV5CanonicalizeBlock::splitBlockWithSPAccess(MachineFunction &MF) {
  bool Changed = false;

  for (auto I = MF.rbegin(), E = MF.rend(); I != E; ++I) {
    auto &MBB = *I;
    for (auto MBBI = MBB.rbegin(), MBBE = MBB.rend(); MBBI != MBBE; ++MBBI) {
      auto &MI = *MBBI;
      // dynamic stack
      if (MI.isCopy()) {
        if (MI.getOperand(0).getReg() == LinxV5::R1)
          MBB.splitAt(MI);
      }
      // stack alloc, such as put args on stack at callsite
      if (MI.getOpcode() == LinxV5::ADJCALLSTACKDOWN ||
          MI.getOpcode() == LinxV5::ADJCALLSTACKUP) {
        if (MI.getOperand(0).getImm() != 0)
          MBB.splitAt(MI);
      }
    }
  }

  if (Changed)
    MF.RenumberBlocks();
  return Changed;
}

MachineInstr *LinxV5CanonicalizeBlock::getSplitPointWithSizeLimitation(
    MachineBasicBlock *MBB) {
  // TODO: Try to find the splitting point to make less livein and liveout.
  unsigned InstNo = 0;
  for (auto I = MBB->rbegin(), E = MBB->rend(); I != E; I++) {
    MachineInstr *MI = &*I;
    if (MI->isLabel() || MI->isDebugInstr() || MI->isPHI())
      continue;
    if (++InstNo > MAX_BLOCK_SIZE)
      return MI;
  }
  return nullptr;
}

bool LinxV5CanonicalizeBlock::canonicalBlock(MachineBasicBlock *MBB) {
  if (MBB->size() <= MAX_BLOCK_SIZE)
    return false;
  MachineInstr *SPI = getSplitPointWithSizeLimitation(MBB);
  if (!SPI)
    return false;
  MBB->splitAt(*SPI, true);
  canonicalBlock(MBB);
  return true;
}

bool LinxV5CanonicalizeBlock::canonicalizeBlockWithSizeLimitation(
    MachineFunction &Fn) {
  bool Changed = false;
  SmallVector<MachineBasicBlock *, 8> ToVisitBBs;
  for (MachineFunction::iterator I = Fn.begin(), E = Fn.end(); I != E; I++)
    ToVisitBBs.push_back(&*I);
  while (!ToVisitBBs.empty()) {
    MachineBasicBlock *MBB = ToVisitBBs.pop_back_val();
    Changed |= canonicalBlock(MBB);
  }
  if (Changed)
    Fn.RenumberBlocks();
  return Changed;
}

/// First we convert linx mir with gpr into blockisa mir with bgpr, second we
/// replace some locally bgpr with tr.
bool LinxV5CanonicalizeBlock::canonicalizeBlockWithTerminator(
    MachineFunction &Fn) {
  bool Changed = false;

  // Split blocks.
  Changed |= splitBlockWithTerminator(Fn);
  Changed |= splitBlockWithSPAccess(Fn);
  if (Changed)
    Fn.RenumberBlocks();

  return Changed;
}

void LinxV5CanonicalizeBlock::dupConstInterBlock(MachineFunction &MF) {
  for (auto &MBB : MF) {
    for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
         MBBI != E; ++MBBI) {
      MachineInstr &MI = *MBBI;
      if (MI.isPHI() || MI.isDebugInstr() || LinxV5::isIsolateInstr(MI))
        continue;
      for (auto &MO : MI.operands()) {
        if (!MO.isReg() || !MO.isUse() || MO.getReg().isPhysical())
          continue;
        MachineInstr *DefMI = MRI->getVRegDef(MO.getReg());
        unsigned ConstOpc = DefMI->getOpcode();
        if (ConstOpc == LinxV5::PseudoVBXCONST &&
            DefMI->getOperand(1).isImm() && DefMI->getParent() != &MBB) {
          Register NewReg =
              MRI->createVirtualRegister(MRI->getRegClass(MO.getReg()));
          BuildMI(MBB, MBBI, MI.getDebugLoc(), TII->get(ConstOpc), NewReg)
              .addImm(DefMI->getOperand(1).getImm());
          MO.ChangeToRegister(NewReg, false, MO.isImplicit(), MO.isKill(),
                              MO.isDead(), MO.isUndef(), MO.isDebug());
        }
      }
    }
  }
}

void rematAddr(MachineBasicBlock &MBB, MachineInstr &MI,
               const TargetInstrInfo *TII, const TargetRegisterInfo *TRI,
               MachineRegisterInfo *MRI) {
  LLVM_DEBUG(dbgs() << "remat " << MI);
  Register Reg = MI.getOperand(0).getReg();
  for (auto &UO : make_early_inc_range(MRI->use_operands(Reg))) {
    auto *UI = UO.getParent();
    LLVM_DEBUG(dbgs() << "  use " << *UI);
    Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(Reg));
    TII->reMaterialize(*UI->getParent(), UI, NewReg, 0, MI, *TRI);
    UO.setReg(NewReg);
  }
}

bool isConstReg(Register Reg, MachineRegisterInfo *MRI) {
  if (Reg == LinxV5::SIMT_LC0)
    return true;
  if (auto *MO = MRI->getOneDef(Reg)) {
    auto *MI = MO->getParent();
    if (MI->isCopy() && isConstReg(MI->getOperand(1).getReg(), MRI))
      return true;
  }
  return false;
}

void rematAddrs(MachineFunction &MF) {
  const auto *TII = MF.getSubtarget().getInstrInfo();
  const auto *TRI = MF.getSubtarget().getRegisterInfo();
  auto *MRI = &MF.getRegInfo();
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : make_early_inc_range(reverse(MBB))) {
      switch (MI.getOpcode()) {
      case LinxV5::SIMT_ADD:
        if (isConstReg(MI.getOperand(2).getReg(), MRI) ||
            isConstReg(MI.getOperand(4).getReg(), MRI))
          rematAddr(MBB, MI, TII, TRI, MRI);
        break;
      case LinxV5::L_ADD_LI:
      case LinxV5::LUI:
      case LinxV5::SIMT_ADDI_SCAR:
        rematAddr(MBB, MI, TII, TRI, MRI);
        break;
      default:
        break;
      }
    }
  }
}

bool LinxV5CanonicalizeBlock::runOnMachineFunction(MachineFunction &Fn) {
  // if (RematAddr)
  //   rematAddrs(Fn);
  if (Fn.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  bool Changed = false;
  TII = static_cast<const LinxV5InstrInfo *>(Fn.getSubtarget().getInstrInfo());
  MRI = &Fn.getRegInfo();
  DT = &getAnalysis<MachineDominatorTree>();

  if (DupConstOnly) {
    // This is a temporary solution to make more combine patterns and to avoid
    // `ADD %fi, reg`. Remove me after motivate to ISel.
    dupConstInterBlock(Fn);
    return true;
  }

  LLVM_DEBUG(dbgs() << "Initial mir: " << Fn.getName() << "\n"; Fn.dump());
  Changed = canonicalizeBlockWithTerminator(Fn);
  Changed |= canonicalizeBlockWithSizeLimitation(Fn);
  LLVM_DEBUG(dbgs() << "After canonicalize block with terminator or size: "
                    << Fn.getName() << "\n";
             Fn.dump());
  if (!DisableDupConst) {
    dupConstInterBlock(Fn);
    LLVM_DEBUG(dbgs() << "After duplicating constant between blocks: "
                      << Fn.getName() << "\n";
               Fn.dump());
  }
  return Changed;
}

FunctionPass *llvm::createLinxV5CanonicalizeBlockPass(bool dupConstOnly) {
  return new LinxV5CanonicalizeBlock(dupConstOnly);
}

namespace {
class LinxV5IsolateInlineASMBlock : public MachineFunctionPass {
public:
  static char ID;
  LinxV5IsolateInlineASMBlock() : MachineFunctionPass(ID) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  bool runOnMachineFunction(MachineFunction &MF) override;

  MachineInstr *hasInlineASM(MachineBasicBlock &MBB);
  StringRef getPassName() const override { return LINX_ISOLATE_INLINE_NAME; }
};
} // namespace

char LinxV5IsolateInlineASMBlock::ID = 0;
INITIALIZE_PASS(LinxV5IsolateInlineASMBlock, "linxv5-isol-asm",
                LINX_ISOLATE_INLINE_NAME, false, false)

FunctionPass *llvm::createLinxV5IsolateInlineASMBlockPass() {
  return new LinxV5IsolateInlineASMBlock();
}

static void updateSlotIndexes(MachineInstr *MI, SlotIndexes *Indexes,
                              DenseSet<Register> &RegsToUpdate) {
  Indexes->removeMachineInstrFromMaps(*MI);
  Indexes->insertMachineInstrInMaps(*MI);
  for (auto &MO : MI->operands())
    if (MO.isReg())
      RegsToUpdate.insert(MO.getReg());
}

static void updateSlotIndexes(SmallVectorImpl<MachineInstr *> &Instrs,
                              SlotIndexes *Indexes,
                              DenseSet<Register> &RegsToUpdate) {
  for (auto *MI : Instrs) {
    updateSlotIndexes(MI, Indexes, RegsToUpdate);
  }
}

MachineInstr *
LinxV5IsolateInlineASMBlock::hasInlineASM(MachineBasicBlock &MBB) {
  for (auto &MI : MBB)
    if (MI.isInlineAsm())
      return &MI;

  return nullptr;
}

bool LinxV5IsolateInlineASMBlock::runOnMachineFunction(MachineFunction &MF) {
  if (MF.getSubtarget<LinxV5Subtarget>().isSIMT())
    return false;

  bool Changed = false;
  const auto *TRI = MF.getSubtarget().getRegisterInfo();
  LiveIntervals *LIS = &getAnalysis<LiveIntervals>();
  for (auto &MBB : MF) {
    /// find the ASM block.
    MachineInstr *ASM = hasInlineASM(MBB);

    if (!ASM)
      continue;

    /// gather instrs of ASM block
    SmallVector<MachineInstr *, 2> PredInstrs;
    SmallVector<MachineInstr *, 2> SuccInstrs;
    auto MII = MBB.begin(), MIE = MBB.end();
    for (; MII != MIE && !MII->isInlineAsm(); ++MII) {
      assert(LinxV5::isRAInstrOfInlineASMBlock(*MII) &&
             "unexpected instr in inlineasm block");
      PredInstrs.push_back(&*MII);
    }
    for (++MII; MII != MIE; ++MII) {
      assert(LinxV5::isRAInstrOfInlineASMBlock(*MII) &&
             "unexpected instr in inlineasm block");
      SuccInstrs.push_back(&*MII);
    }

    /// move out
    DenseSet<Register> RegsToUpdate;
    MachineBasicBlock *ASMMBB = &MBB;
    if (!PredInstrs.empty()) {
      // move to the pred
      assert(hasInlineASMPrologue(*ASMMBB) &&
             "must have a single pred to move to");
      MachineBasicBlock *Pred = *ASMMBB->pred_begin();
      auto InsertPos = Pred->getFirstTerminator();
      Pred->splice(InsertPos, ASMMBB, PredInstrs.front(), ASM);
      updateSlotIndexes(PredInstrs, LIS->getSlotIndexes(), RegsToUpdate);
      Changed = true;
    }

    if (!SuccInstrs.empty()) {
      // move to the succ
      assert(hasInlineASMEpilogue(*ASMMBB) &&
             "must have a single succ to move to");
      MachineBasicBlock *Succ = *ASMMBB->succ_begin();
      auto InsertPos = Succ->getFirstNonDebugInstr();
      Succ->splice(InsertPos, ASMMBB, SuccInstrs.front(), ASMMBB->end());
      updateSlotIndexes(SuccInstrs, LIS->getSlotIndexes(), RegsToUpdate);
      Changed = true;
    }

    /// recalc changed regs
    for (Register Reg : RegsToUpdate) {
      if (Reg.isVirtual()) {
        LIS->removeInterval(Reg);
        LIS->getInterval(Reg);
      } else {
        MCRegUnitIterator RUI(Reg, TRI);
        LIS->removeRegUnit(*RUI);
        LIS->getRegUnit(*RUI);
      }
    }
  }

  return Changed;
}

namespace {
class LinxV5FixSGPRCopies : public MachineFunctionPass {
public:
  static char ID;
  const LinxV5InstrInfo *TII;
  MachineRegisterInfo *MRI;
  const TargetRegisterInfo *TRI;
  LinxV5FixSGPRCopies() : MachineFunctionPass(ID), TII(nullptr), MRI(nullptr), TRI(nullptr) {}
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    AU.addPreserved<MachineDominatorTree>();
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  bool runOnMachineFunction(MachineFunction &MF) override;


  void legalizeOperands(MachineInstr &MI);
  void legalizeGenericOperand(MachineBasicBlock &InsertMBB,
                              MachineBasicBlock::iterator I,
                              const TargetRegisterClass *DstRC,
                              MachineOperand &Op, MachineRegisterInfo &MRI,
                              const DebugLoc &DL,
                              MachineInstr &MI) const;
  /// Return the correct register class for \p OpNo.  For target-specific
  /// instructions, this will return the register class that has been defined
  /// in tablegen.  For generic instructions, like REG_SEQUENCE it will return
  /// the register class of its machine operand.
  /// to infer the correct register class base on the other operands.
  const TargetRegisterClass *getOpRegClass(const MachineInstr &MI,
                                           unsigned OpNo) const;
  static bool hasVectorRegisters(const TargetRegisterClass *RC) {
    return RC == &LinxV5::SIMTCGVRegClass || RC == &LinxV5::SIMTCGVLRegClass;
  }

  static bool isSGPRClass(const TargetRegisterClass *RC) {
    return RC == &LinxV5::SIMTCGSRegClass || RC == &LinxV5::SIMTCGSLRegClass || RC == &LinxV5::CGSLRegClass;
  }
};
} // namespace

char LinxV5FixSGPRCopies::ID = 0;
INITIALIZE_PASS(LinxV5FixSGPRCopies, "linx-fix-sgpr-copies",
                LINX_FIXSGPRCOPIES_NAME, false, false)

FunctionPass *llvm::createLinxV5FixSGPRCopiesPass() {
  return new LinxV5FixSGPRCopies();
}


bool LinxV5FixSGPRCopies::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());
  MRI = &MF.getRegInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  for (MachineFunction::iterator BI = MF.begin(), BE = MF.end();
                                                  BI != BE; ++BI) {
    MachineBasicBlock *MBB = &*BI;
    for (MachineBasicBlock::iterator I = MBB->begin(), E = MBB->end(); I != E;
         ++I) {
      MachineInstr &MI = *I;

      if (MI.getOpcode() != LinxV5::PHI)
        continue;
        legalizeOperands(MI);
        Changed = true;
      }
    }

  return Changed;
}


void LinxV5FixSGPRCopies::legalizeOperands(MachineInstr &MI) {
    const TargetRegisterClass *RC = nullptr, *SRC = nullptr, *VRC = nullptr;
    for (unsigned i = 1, e = MI.getNumOperands(); i != e; i += 2) {
      if (!MI.getOperand(i).isReg() || !MI.getOperand(i).getReg().isVirtual())
        continue;
      const TargetRegisterClass *OpRC =
          MRI->getRegClass(MI.getOperand(i).getReg());
      if (hasVectorRegisters(OpRC)) {
        VRC = OpRC;
      } else {
        SRC = OpRC;
      }
    }

    // If any of the operands are VGPR registers, then they all most be
    // otherwise we will create illegal VGPR->SGPR copies when legalizing
    // them.
    if (VRC || !isSGPRClass(getOpRegClass(MI, 0))) {
      if (!VRC) {
        assert(SRC);
        VRC = getOpRegClass(MI, 0);
      }
      RC = VRC;
    } else {
      RC = SRC;
    }

    // Update all the operands so they have the same type.
    for (unsigned I = 1, E = MI.getNumOperands(); I != E; I += 2) {
      MachineOperand &Op = MI.getOperand(I);
      if (!Op.isReg() || !Op.getReg().isVirtual())
        continue;

      // MI is a PHI instruction.
      MachineBasicBlock *InsertBB = MI.getOperand(I + 1).getMBB();
      MachineBasicBlock::iterator Insert = InsertBB->getFirstTerminator();

      // Avoid creating no-op copies with the same src and dst reg class.  These
      // confuse some of the machine passes.
      legalizeGenericOperand(*InsertBB, Insert, RC, Op, *MRI, MI.getDebugLoc(), MI);
    }
}

void LinxV5FixSGPRCopies::legalizeGenericOperand(MachineBasicBlock &InsertMBB,
                                         MachineBasicBlock::iterator I,
                                         const TargetRegisterClass *DstRC,
                                         MachineOperand &Op,
                                         MachineRegisterInfo &MRI,
                                         const DebugLoc &DL,
                                         MachineInstr &MI) const {
  Register OpReg = Op.getReg();

  const TargetRegisterClass *OpRC = MRI.getRegClass(OpReg);

  // Check if operand is already the correct register class.
  if (DstRC == OpRC)
    return;

  SetVector<const MachineInstr *> worklist;
  SmallSet<const MachineInstr *, 4> Visited;
  MachineInstr *DefMI = MRI.getVRegDef(OpReg);
  worklist.insert(DefMI);
  Visited.insert(DefMI);

  while (!worklist.empty()) {
    const MachineInstr *Instr = worklist.pop_back_val();
    Register Reg = Instr->getOperand(0).getReg();
    MRI.setRegClass(Reg, DstRC);
    if (Instr->isCopy()) {
      Register CopyFromReg = Instr->getOperand(1).getReg();
      if (CopyFromReg.isPhysical())
        continue;
      MachineInstr *DefMI = MRI.getVRegDef(CopyFromReg);
      if (DefMI && DefMI->getOpcode() == LinxV5::LinxV5ImplicitSDef) {
        DefMI->setDesc(TII->get(LinxV5::LinxV5ImplicitDef));
        MRI.setRegClass(CopyFromReg, DstRC);
        continue;
      }
      if (DefMI && Visited.insert(DefMI).second)
        worklist.insert(DefMI);
    }
  }
}

const TargetRegisterClass *LinxV5FixSGPRCopies::getOpRegClass(const MachineInstr &MI,
                                                      unsigned OpNo) const {
  return MRI->getRegClass(MI.getOperand(OpNo).getReg());
}
