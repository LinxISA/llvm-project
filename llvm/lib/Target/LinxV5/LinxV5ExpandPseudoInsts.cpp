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

#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/IR/InlineAsm.h"
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
  bool foldInlineAsmDimConstants(MachineFunction &MF);
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

struct InlineAsmDimUse {
  MachineInstr *MI;
  unsigned FlagOperand;
  unsigned ValueOperand;
  unsigned AsmOperand;
  bool KillsRegister;
};

static bool findInlineAsmDimPlaceholder(StringRef Asm, unsigned AsmOperand,
                                        unsigned &LoopReg) {
  std::string Prefix = "B.DIM $" + utostr(AsmOperand) + ", 0, ->lb";
  size_t Pos = Asm.find(Prefix);
  if (Pos == StringRef::npos || Pos + Prefix.size() >= Asm.size())
    return false;
  char Slot = Asm[Pos + Prefix.size()];
  if (Slot < '0' || Slot > '2')
    return false;
  LoopReg = Slot - '0';
  return true;
}

static bool collectInlineAsmDimUses(MachineInstr &MI, Register Reg,
                                    SmallVectorImpl<InlineAsmDimUse> &Uses) {
  if (!MI.isInlineAsm())
    return false;

  StringRef Asm = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  unsigned MatchingRegisterUses = 0;
  unsigned AsmOperand = 0;
  for (unsigned I = InlineAsm::MIOp_FirstOperand, E = MI.getNumOperands();
       I < E; ++AsmOperand) {
    MachineOperand &Flag = MI.getOperand(I);
    if (!Flag.isImm())
      break;
    unsigned FlagValue = Flag.getImm();
    unsigned NumOperands = InlineAsm::getNumOperandRegisters(FlagValue);
    for (unsigned J = 0; J != NumOperands; ++J) {
      MachineOperand &Value = MI.getOperand(I + 1 + J);
      if (!Value.isReg() || !Value.isUse() || Value.getReg() != Reg)
        continue;
      ++MatchingRegisterUses;
      unsigned LoopReg = 0;
      if (InlineAsm::getKind(FlagValue) != InlineAsm::Kind_RegUse ||
          NumOperands != 1 ||
          !findInlineAsmDimPlaceholder(Asm, AsmOperand, LoopReg))
        return false;
      Uses.push_back({&MI, I, I + 1 + J, AsmOperand, Value.isKill()});
    }
    I += 1 + NumOperands;
  }

  unsigned ExplicitRegisterUses = 0;
  for (const MachineOperand &Operand : MI.explicit_operands())
    ExplicitRegisterUses +=
        Operand.isReg() && Operand.isUse() && Operand.getReg() == Reg;
  return MatchingRegisterUses != 0 &&
         MatchingRegisterUses == ExplicitRegisterUses;
}

static bool rewriteInlineAsmDim(StringRef Input, unsigned AsmOperand,
                                int64_t Value, std::string &Output) {
  std::string Prefix = "B.DIM $" + utostr(AsmOperand) + ", 0, ->lb";
  size_t Pos = Input.find(Prefix);
  if (Pos == StringRef::npos || Pos + Prefix.size() >= Input.size())
    return false;
  char Slot = Input[Pos + Prefix.size()];
  if (Slot < '0' || Slot > '2')
    return false;

  Output = Input.str();
  size_t Length = Prefix.size() + 1;
  if (Value == 1) {
    size_t End = Output.find('\n', Pos + Length);
    Output.erase(Pos,
                 End == std::string::npos ? std::string::npos : End - Pos + 1);
  } else {
    std::string Replacement =
        "B.DIM zero, ${" + utostr(AsmOperand) + ":c}, ->lb" + Slot;
    Output.replace(Pos, Length, Replacement);
  }
  return true;
}

static bool omitDefaultInlineAsmDims(MachineInstr &MI, MachineFunction &MF) {
  if (!MI.isInlineAsm())
    return false;

  std::string Asm = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  bool Changed = false;
  unsigned AsmOperand = 0;
  for (unsigned I = InlineAsm::MIOp_FirstOperand, E = MI.getNumOperands();
       I < E; ++AsmOperand) {
    const MachineOperand &Flag = MI.getOperand(I);
    if (!Flag.isImm())
      break;
    unsigned FlagValue = Flag.getImm();
    unsigned NumOperands = InlineAsm::getNumOperandRegisters(FlagValue);
    if (InlineAsm::getKind(FlagValue) == InlineAsm::Kind_Imm &&
        NumOperands == 1 && MI.getOperand(I + 1).isImm() &&
        MI.getOperand(I + 1).getImm() == 1) {
      for (unsigned LoopReg = 0; LoopReg != 3; ++LoopReg) {
        std::string Line = "B.DIM zero, ${" + utostr(AsmOperand) + ":c}, ->lb" +
                           utostr(LoopReg);
        size_t Pos = Asm.find(Line);
        if (Pos == std::string::npos)
          continue;
        size_t End = Asm.find('\n', Pos + Line.size());
        Asm.erase(Pos,
                  End == std::string::npos ? std::string::npos : End - Pos + 1);
        Changed = true;
      }
    }
    I += 1 + NumOperands;
  }
  if (Changed)
    MI.getOperand(InlineAsm::MIOp_AsmString)
        .ChangeToES(MF.createExternalSymbolName(Asm));
  return Changed;
}

static MachineBasicBlock *getLinearSuccessor(MachineBasicBlock &MBB) {
  MachineBasicBlock *Successor = nullptr;
  for (MachineBasicBlock *Candidate : MBB.successors()) {
    if (!Successor)
      Successor = Candidate;
    else if (Successor != Candidate)
      return nullptr;
  }
  if (!Successor)
    return nullptr;
  for (MachineBasicBlock *Predecessor : Successor->predecessors())
    if (Predecessor != &MBB)
      return nullptr;
  return Successor;
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

  bool Modified = foldInlineAsmDimConstants(MF);

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

bool LinxV5ExpandPseudo::foldInlineAsmDimConstants(MachineFunction &MF) {
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &Def : make_early_inc_range(MBB)) {
      if (Def.getOpcode() != LinxV5::ADDI || Def.getNumOperands() < 3 ||
          !Def.getOperand(0).isReg() || !Def.getOperand(0).isDef() ||
          !Def.getOperand(1).isReg() ||
          Def.getOperand(1).getReg() != LinxV5::R0 ||
          !Def.getOperand(2).isImm())
        continue;

      Register Reg = Def.getOperand(0).getReg();
      int64_t Value = Def.getOperand(2).getImm();
      if (Value < 1 || !isUInt<8>(Value))
        continue;

      SmallVector<InlineAsmDimUse, 4> Uses;
      SmallVector<MachineBasicBlock *, 8> PropagatedBlocks;
      bool Safe = true;
      bool ReachedEnd = false;
      MachineBasicBlock *Block = &MBB;
      MachineBasicBlock::instr_iterator I = std::next(Def.getIterator());
      while (Safe && !ReachedEnd) {
        for (MachineBasicBlock::instr_iterator E = Block->instr_end(); I != E;
             ++I) {
          if (I->definesRegister(Reg)) {
            ReachedEnd = true;
            break;
          }
          if (!I->readsRegister(Reg))
            continue;
          SmallVector<InlineAsmDimUse, 2> CurrentUses;
          if (!collectInlineAsmDimUses(*I, Reg, CurrentUses)) {
            Safe = false;
            break;
          }
          for (const InlineAsmDimUse &Use : CurrentUses) {
            Uses.push_back(Use);
            ReachedEnd |= Use.KillsRegister;
          }
          if (ReachedEnd)
            break;
        }
        if (!Safe || ReachedEnd)
          break;
        MachineBasicBlock *Successor = getLinearSuccessor(*Block);
        if (!Successor || !Successor->isLiveIn(Reg) ||
            is_contained(PropagatedBlocks, Successor)) {
          Safe = false;
          break;
        }
        Block = Successor;
        PropagatedBlocks.push_back(Block);
        I = Block->instr_begin();
      }
      if (!Safe || !ReachedEnd || Uses.empty())
        continue;

      for (const InlineAsmDimUse &Use : Uses) {
        MachineInstr &InlineAsmMI = *Use.MI;
        StringRef Asm =
            InlineAsmMI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
        std::string Rewritten;
        if (!rewriteInlineAsmDim(Asm, Use.AsmOperand, Value, Rewritten)) {
          Safe = false;
          break;
        }
        InlineAsmMI.getOperand(InlineAsm::MIOp_AsmString)
            .ChangeToES(MF.createExternalSymbolName(Rewritten));
        InlineAsmMI.getOperand(Use.FlagOperand)
            .setImm(InlineAsm::getFlagWord(InlineAsm::Kind_Imm, 1));
        InlineAsmMI.getOperand(Use.ValueOperand).ChangeToImmediate(Value);
      }
      if (!Safe)
        report_fatal_error("failed to rewrite validated inline-asm B.DIM");
      for (MachineBasicBlock *Propagated : PropagatedBlocks)
        Propagated->removeLiveIn(Reg);
      Def.eraseFromParent();
      Changed = true;
    }

    for (MachineInstr &MI : MBB)
      Changed |= omitDefaultInlineAsmDims(MI, MF);
  }
  return Changed;
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
