//===-- LinxV5ExpandPseudoInsts.cpp - Expand pseudo instructions
//-----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "LinxV5TargetMachine.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-expand-pseudo"
#define LINX_EXPAND_PSEUDO_NAME "LinxV5 pseudo instruction expansion pass"

namespace {

class LinxV5ExpandPseudo : public MachineFunctionPass {
public:
  static char ID;

  LinxV5ExpandPseudo() : MachineFunctionPass(ID), TII(nullptr) {
    initializeLinxV5ExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return LINX_EXPAND_PSEUDO_NAME; }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  const LinxV5InstrInfo *TII;
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandRET(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandMAMULBAC(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandMAMULBMXAC(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI);
  bool expandMAMULBMXBAC(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandLAhi(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandBRCond(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  bool expandSETCTGT(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     unsigned Opc);
  bool expandPseudoCopy(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                     unsigned Opc);
  bool expandImplicitDef(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
  MachineBasicBlock::iterator calcSETCPos(MachineBasicBlock::iterator UseMI);
  bool mergeCompareArith(MachineFunction &MF);
};

char LinxV5ExpandPseudo::ID = 0;

static bool isLinxV5BlockTerminator(MachineInstr *MI) {
  return MI->isTerminator() || LinxV5::isIsolateInstr(*MI);
}

template <typename T> inline T prev_nodbg_cfi(T It, T Begin) {
  --It;
  while (It != Begin &&
         (It->isDebugInstr() || It->isPseudoProbe() || It->isCFIInstruction()))
    --It;
  return It;
}

static MachineInstr *hasOnlyOneUse(Register Reg, MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator Def) {
  unsigned count = 0;
  for (auto MBBI = std::next(Def), MBBE = MBB.end(); MBBI != MBBE; ++MBBI) {
    MachineInstr &MI = *MBBI;
    if (MI.readsRegister(Reg))
      ++count;
    if (MI.definesRegister(Reg))
      return count == 1 ? &*Def : nullptr;
  }
  return count == 1 ? &*Def : nullptr;
}

static MachineInstr *matchOpc(unsigned Opc, Register Reg, Register Clobber,
                              MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator Before) {
  if (!LinxV5::LTRRegClass.contains(Reg) && !LinxV5::LURRegClass.contains(Reg))
    return nullptr;
  for (auto MBBI = Before, MBBB = MBB.begin(); MBBI != MBBB; --MBBI) {
    MachineInstr &MI = *std::prev(MBBI);
    if (MI.getOpcode() == Opc && MI.getOperand(0).getReg() == Reg) {
      return hasOnlyOneUse(Reg, MBB, &MI);
    } else if (MI.definesRegister(Reg) || MI.definesRegister(Clobber) ||
               MI.readsRegister(Clobber))
      return nullptr;
  }
  return nullptr;
}

static MachineInstr *matchANDI(Register Reg, Register Clobber,
                               MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator Before) {
  return matchOpc(LinxV5::ANDI, Reg, Clobber, MBB, Before);
}

static MachineInstr *matchBIC(Register Reg, Register Clobber,
                              MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator Before) {
  return matchOpc(LinxV5::BIC, Reg, Clobber, MBB, Before);
}

static MachineInstr *matchORI(Register Reg, Register Clobber,
                              MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator Before) {
  return matchOpc(LinxV5::ORI, Reg, Clobber, MBB, Before);
}

static MachineInstr *matchAND(Register Reg, Register Clobber,
                              MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator Before) {
  auto *Ret = matchOpc(LinxV5::AND, Reg, Clobber, MBB, Before);
  if (Ret && Ret->getOperand(3).getImm() != LinxV5Op::SrcRType::NONE)
    Ret = nullptr;
  return Ret;
}

static MachineInstr *matchOR(Register Reg, Register Clobber,
                             MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator Before) {
  auto *Ret = matchOpc(LinxV5::OR, Reg, Clobber, MBB, Before);
  if (Ret && Ret->getOperand(3).getImm() != LinxV5Op::SrcRType::NONE)
    Ret = nullptr;
  return Ret;
}

static bool isCMPNZ(MachineInstr &MI) {
  return (MI.getOpcode() == LinxV5::CMP_NE &&
          (MI.getOperand(2).getReg() == LinxV5::R0)) ||
         (MI.getOpcode() == LinxV5::CMP_NEI &&
          (MI.getOperand(2).getImm() == 0));
}

static bool isSETCNZ(MachineInstr &MI) {
  return (MI.getOpcode() == LinxV5::SETC_NE &&
          (MI.getOperand(1).getReg() == LinxV5::R0)) ||
         (MI.getOpcode() == LinxV5::SETC_NEI &&
          (MI.getOperand(1).getImm() == 0));
}

static void mergeCompareArith1(MachineBasicBlock &MBB) {
  const auto *TII = MBB.getParent()->getSubtarget().getInstrInfo();
  for (auto &MI : make_early_inc_range(MBB)) {
    if (isSETCNZ(MI)) {
      LLVM_DEBUG(dbgs() << "is setcnz " << MI);
      MachineInstr *Def = matchANDI(MI.getOperand(0).getReg(),
                                    MCRegister::NoRegister, MBB, &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::SETC_ANDI))
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def =
          matchBIC(MI.getOperand(0).getReg(), MCRegister::NoRegister, MBB, &MI);
      if (Def) {
        int64_t M = Def->getOperand(2).getImm();
        int64_t N = Def->getOperand(3).getImm();
        if ((M + N) == 64) {
          uint64_t Mask = (-1ull) >> N;
          if (isInt<12>(Mask)) {
            BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::SETC_ANDI))
                .add(Def->getOperand(1))
                .addImm(Mask);
            Def->removeFromParent();
            MI.removeFromParent();
            continue;
          }
        }
      }

      Def =
          matchORI(MI.getOperand(0).getReg(), MCRegister::NoRegister, MBB, &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::SETC_ORI))
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def =
          matchAND(MI.getOperand(0).getReg(), MCRegister::NoRegister, MBB, &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::SETC_AND))
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def =
          matchOR(MI.getOperand(0).getReg(), MCRegister::NoRegister, MBB, &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::SETC_OR))
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }
    } else if (isCMPNZ(MI)) {
      LLVM_DEBUG(dbgs() << "is cmpnz " << MI);
      MachineInstr *Def = matchANDI(MI.getOperand(1).getReg(),
                                    MI.getOperand(0).getReg(), MBB, &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::CMP_ANDI),
                MI.getOperand(0).getReg())
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def = matchBIC(MI.getOperand(1).getReg(), MI.getOperand(0).getReg(), MBB,
                     &MI);
      if (Def) {
        int64_t M = Def->getOperand(2).getImm();
        int64_t N = Def->getOperand(3).getImm();
        if ((M + N) == 64) {
          uint64_t Mask = (-1ull) >> N;
          if (isInt<12>(Mask)) {
            BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::CMP_ANDI),
                    MI.getOperand(0).getReg())
                .add(Def->getOperand(1))
                .addImm(Mask);
            Def->removeFromParent();
            MI.removeFromParent();
            continue;
          }
        }
      }

      Def = matchORI(MI.getOperand(1).getReg(), MI.getOperand(0).getReg(), MBB,
                     &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::CMP_ORI),
                MI.getOperand(0).getReg())
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def = matchAND(MI.getOperand(1).getReg(), MI.getOperand(0).getReg(), MBB,
                     &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::CMP_AND),
                MI.getOperand(0).getReg())
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }

      Def = matchOR(MI.getOperand(1).getReg(), MI.getOperand(0).getReg(), MBB,
                    &MI);
      if (Def) {
        BuildMI(MBB, *Def, Def->getDebugLoc(), TII->get(LinxV5::CMP_OR),
                MI.getOperand(0).getReg())
            .add(Def->getOperand(1))
            .add(Def->getOperand(2));
        Def->removeFromParent();
        MI.removeFromParent();
        continue;
      }
    }
  }
}

bool LinxV5ExpandPseudo::mergeCompareArith(MachineFunction &MF) {
  for (auto &MBB : MF) {
    mergeCompareArith1(MBB);
  }
  return true;
}

bool LinxV5ExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const LinxV5InstrInfo *>(MF.getSubtarget().getInstrInfo());

  bool Modified = false;

  MachineFunction::iterator MFI = MF.begin(), E = MF.end();
  while (MFI != E) {
    MachineFunction::iterator NMFI = std::next(MFI);
    Modified |= expandMBB(*MFI);
    MFI = NMFI;
  }

  if (MF.getSubtarget<LinxV5Subtarget>().enableFoldCmpArith())
    mergeCompareArith(MF);

  return Modified;
}

bool LinxV5ExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;
  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    MachineInstr *MI = &*MBBI;
    if (!MI->isCFIInstruction())
      Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool LinxV5ExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MBBI,
                                  MachineBasicBlock::iterator &NextMBBI) {
  switch (MBBI->getOpcode()) {
  case LinxV5::PseudoMAMULBAC_Higher_SizeI:
    return expandMAMULBAC(MBB, MBBI);
  case LinxV5::PseudoMAMULBMXAC_Higher_SizeI:
    return expandMAMULBMXAC(MBB, MBBI);
  case LinxV5::PseudoMAMULBMXBAC_Higher_SizeI:
    return expandMAMULBMXBAC(MBB, MBBI);
  case LinxV5::PseudoRET:
    return expandRET(MBB, MBBI);
  case LinxV5::PseudoADDTPC_HI:
    return expandLAhi(MBB, MBBI);
  case LinxV5::SETC_EQ_BR:
  case LinxV5::SETC_EQ_SW_BR:
  case LinxV5::SETC_EQ_UW_BR:
  case LinxV5::SETC_EQI_BR:
  case LinxV5::SETC_NE_BR:
  case LinxV5::SETC_NE_SW_BR:
  case LinxV5::SETC_NE_UW_BR:
  case LinxV5::SETC_NEI_BR:
  case LinxV5::SETC_LT_BR:
  case LinxV5::SETC_LT_SW_BR:
  case LinxV5::SETC_LT_UW_BR:
  case LinxV5::SETC_LTI_BR:
  case LinxV5::SETC_GE_BR:
  case LinxV5::SETC_GE_SW_BR:
  case LinxV5::SETC_GE_UW_BR:
  case LinxV5::SETC_GEI_BR:
  case LinxV5::SETC_LTU_BR:
  case LinxV5::SETC_LTU_SW_BR:
  case LinxV5::SETC_LTU_UW_BR:
  case LinxV5::SETC_LTUI_BR:
  case LinxV5::SETC_GEU_BR:
  case LinxV5::SETC_GEU_SW_BR:
  case LinxV5::SETC_GEU_UW_BR:
  case LinxV5::SETC_GEUI_BR:
  case LinxV5::SETC_AND_BR:
  case LinxV5::SETC_AND_SW_BR:
  case LinxV5::SETC_AND_UW_BR:
  case LinxV5::SETC_AND_NOT_BR:
  case LinxV5::SETC_ANDI_BR:
  case LinxV5::SETC_OR_BR:
  case LinxV5::SETC_OR_SW_BR:
  case LinxV5::SETC_OR_UW_BR:
  case LinxV5::SETC_OR_NOT_BR:
  case LinxV5::SETC_ORI_BR:
    return expandBRCond(MBB, MBBI);
  case LinxV5::PseudoBRIND:
    return expandSETCTGT(MBB, MBBI, LinxV5::PseudoBRIndCARG);
  case LinxV5::PseudoCALLInd:
    return expandSETCTGT(MBB, MBBI, LinxV5::PseudoCALLIndCARG);
  case LinxV5::PseudoTAILInd:
    return expandSETCTGT(MBB, MBBI, LinxV5::PseudoTAILIndCARG);
  case LinxV5::LinxV5PseudoCopyFromP:
    return expandPseudoCopy(MBB, MBBI, LinxV5::LinxV5PseudoCopyFromP);
  case LinxV5::LinxV5PseudoCopy2P:
    return expandPseudoCopy(MBB, MBBI, LinxV5::LinxV5PseudoCopy2P);
  case LinxV5::LinxV5PseudoCopy2PTerm:
    return expandPseudoCopy(MBB, MBBI, LinxV5::LinxV5PseudoCopy2PTerm);
  case LinxV5::LinxV5ImplicitDef:
  case LinxV5::LinxV5ImplicitSDef:
    return expandImplicitDef(MBB, MBBI);
  }

  return false;
}

MachineBasicBlock::iterator
LinxV5ExpandPseudo::calcSETCPos(MachineBasicBlock::iterator UseMI) {
  SmallSet<Register, 2> UseRegs;
  for (auto &UseMO : UseMI->explicit_uses()) {
    if (UseMO.isReg()) {
      UseRegs.insert(UseMO.getReg());
    }
  }

  MachineBasicBlock &MBB = *UseMI->getParent();
  auto MBBI = UseMI, MBBB = MBB.begin();
  for (; MBBI != MBBB;) {
    MachineInstr &MI = *(--MBBI);

    if (LinxV5::isIsolateInstr(MI))
      return std::next(MI.getIterator());

    for (auto &DefMO : MI.defs()) {
      if (!DefMO.isReg() || DefMO.isImplicit())
        continue;
      Register Reg = DefMO.getReg();
      if (UseRegs.count(Reg))
        return std::next(MI.getIterator());
    }
  }
  return MBBI;
}

bool LinxV5ExpandPseudo::expandBRCond(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  BuildMI(MBB, calcSETCPos(MBBI), MBBI->getDebugLoc(),
          TII->get(LinxV5::getPseudoMap(MBBI->getOpcode())))
      .add(MBBI->getOperand(0))
      .add(MBBI->getOperand(1));
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::PseudoBRCondCARG))
      .add(MBBI->getOperand(2));
  MBBI->eraseFromParent();
  return true;
}

bool LinxV5ExpandPseudo::expandSETCTGT(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       unsigned Opc) {
  BuildMI(MBB, calcSETCPos(MBBI), MBBI->getDebugLoc(),
          TII->get(LinxV5::SETC_TGT))
      .add(MBBI->getOperand(0));
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(Opc));
  std::prev(MBBI)->copyImplicitOps(*MBB.getParent(), *MBBI);
  MBBI->eraseFromParent();
  return true;
}

bool LinxV5ExpandPseudo::expandPseudoCopy(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       unsigned Opc) {
  switch(Opc) {
    default:
      break;
    case LinxV5::LinxV5PseudoCopy2P:
    case LinxV5::LinxV5PseudoCopy2PTerm: {
      BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
          TII->get(LinxV5::SIMT_ADDI_SCAR))
      .addReg(LinxV5::SIMT_P, RegState::Define)
      .addImm(LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
      .add(MBBI->getOperand(0))
      .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD)
      .addImm(0);

      MBBI->eraseFromParent();
      return true;
    }
    case LinxV5::LinxV5PseudoCopyFromP: {
      BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
          TII->get(LinxV5::SIMT_ADDI_SCAR))
      .addReg(MBBI->getOperand(0).getReg(), RegState::Define)
      .addImm(LinxV5Op::SIMT_INT_DST_REG_TYPE_D)
      .addReg(LinxV5::SIMT_P)
      .addImm(LinxV5Op::SIMT_INT_SRC_REG_TYPE_UD)
      .addImm(0);

      MBBI->eraseFromParent();
      return true;
    }
  }
  return true;
}

bool LinxV5ExpandPseudo::expandImplicitDef(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {

  BuildMI(MBB, MBBI, MBBI->getDebugLoc(),
      TII->get(LinxV5::SIMT_ADDI))
  .addReg(MBBI->getOperand(0).getReg(), RegState::Define)
  .add(MBBI->getOperand(1))
  .addReg(LinxV5::R0)
  .add(MBBI->getOperand(1))
  .addImm(0);

  MBBI->eraseFromParent();
  return true;
}

bool LinxV5ExpandPseudo::expandLAhi(MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI) {
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::PseudoLABEL))
      .add(MBBI->getOperand(1));
  if (MBBI->getOperand(0).getReg() == LinxV5::R10)
    report_fatal_error("internal error: addtpc cannot write ra!");
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::ADDTPC))
      .add(MBBI->getOperand(0))
      .add(MBBI->getOperand(2));
  MBBI->eraseFromParent();
  return true;
}

bool LinxV5ExpandPseudo::expandRET(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI) {

  if (MBB.getParent()->getSubtarget<LinxV5Subtarget>().isSIMT()) {
    BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::SIMT_BSTOP));
    MBBI->removeFromParent();
    return true;
  }

  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::C_SETC_TGT))
      .addReg(LinxV5::R10);
  BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(LinxV5::PseudoRETCARG));
  // Return non-void need add implicit a0 for ret-inst.
  std::prev(MBBI)->copyImplicitOps(*MBB.getParent(), *MBBI);
  MBBI->removeFromParent();
  return true;
}

bool LinxV5ExpandPseudo::expandMAMULBAC(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI) {

  if (MBBI->getOperand(12).getReg() == LinxV5::Tile_ACC1) {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBACC_SizeI));
  } else {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBAC_SizeI));
  }
  return true;
}

bool LinxV5ExpandPseudo::expandMAMULBMXAC(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  if (MBBI->getOperand(14).getReg() == LinxV5::Tile_ACC1) {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBMXACC_SizeI));
  } else {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBMXAC_SizeI));
  }
  return true;
}

bool LinxV5ExpandPseudo::expandMAMULBMXBAC(MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MBBI) {
  if (MBBI->getOperand(13).getReg() == LinxV5::Tile_ACC1) {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBMXBACC_SizeI));
  } else {
    MBBI->setDesc(TII->get(LinxV5::PseudoMAMULBMXBAC_SizeI));
  }
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(LinxV5ExpandPseudo, "linxv5-expand-pseudo",
                LINX_EXPAND_PSEUDO_NAME, false, false)
namespace llvm {

FunctionPass *createLinxV5ExpandPseudoPass() {
  return new LinxV5ExpandPseudo();
}

} // end of namespace llvm
