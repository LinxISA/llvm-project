//===-- LinxV5SharedReuseFinalize.cpp - Final Shared source aliases --------===//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/InlineAsm.h"
using namespace llvm;
#define DEBUG_TYPE "linxv5-shared-reuse-finalize"
namespace {
static Register plain(Register R) {
  if (R >= LinxV5::Shared_S0_RU && R <= LinxV5::Shared_S63_RU)
    return LinxV5::Shared_S0 + R - LinxV5::Shared_S0_RU;
  return R;
}
static bool shared(Register R) {
  return LinxV5::Shared_ABSRegClass.contains(plain(R));
}
MachineOperand *getOperand(MachineInstr &MI, unsigned Group) {
  for (unsigned Index = InlineAsm::MIOp_FirstOperand;
       Index < MI.getNumOperands();) {
    MachineOperand &Flag = MI.getOperand(Index);
    if (!Flag.isImm())
      return nullptr;
    unsigned Count = InlineAsm::getNumOperandRegisters(Flag.getImm());
    if (!Group)
      return Count == 1 && Index + 1 < MI.getNumOperands()
                 ? &MI.getOperand(Index + 1)
                 : nullptr;
    --Group;
    Index += 1 + Count;
  }
  return nullptr;
}

bool constantValue(MachineInstr &MI, StringRef Text, int64_t &Value) {
  Text = Text.trim();
  if (Text.consume_front("${")) {
    if (!Text.consume_back(":c}"))
      return false;
    unsigned Group;
    if (Text.getAsInteger(10, Group))
      return false;
    MachineOperand *Operand = getOperand(MI, Group);
    if (!Operand || !Operand->isImm())
      return false;
    Value = Operand->getImm();
    return true;
  }
  StringRef Digits = Text;
  if (!Digits.consume_front("-"))
    Digits.consume_front("+");
  if (Digits.empty() || (Digits.size() > 1 && Digits.front() == '0') ||
      !llvm::all_of(Digits, [](char Char) { return isDigit(Char); }))
    return false;
  return !Text.getAsInteger(10, Value);
}

bool constantCondition(MachineInstr &MI, StringRef Text, bool &Value) {
  size_t Equal = Text.find("==");
  int64_t Left;
  int64_t Right;
  if (Equal == StringRef::npos ||
      !constantValue(MI, Text.take_front(Equal), Left) ||
      !constantValue(MI, Text.substr(Equal + 2), Right))
    return false;
  Value = Left == Right;
  return true;
}

bool normalizeConditions(MachineInstr &MI, StringRef Text,
                         std::string &Normalized) {
  struct Conditional {
    bool Parent;
    bool Taken;
    bool Active;
    bool HasElse;
  };
  SmallVector<Conditional, 4> Conditions;
  SmallVector<StringRef, 32> Lines;
  Text.split(Lines, '\n');
  for (StringRef Line : Lines) {
    StringRef Directive = Line.trim();
    StringRef Opcode = Directive.take_until([](char Char) {
      return isSpace(Char);
    });
    bool Active = Conditions.empty() || Conditions.back().Active;
    if (Opcode == ".if") {
      bool Selected;
      if (Conditions.size() == 16 ||
          !constantCondition(MI, Directive.drop_front(3), Selected))
        return false;
      Conditions.push_back({Active, Selected, Active && Selected, false});
    } else if (Opcode == ".elseif") {
      bool Selected;
      if (Conditions.empty() || Conditions.back().HasElse ||
          !constantCondition(MI, Directive.drop_front(7), Selected))
        return false;
      Conditional &Condition = Conditions.back();
      Condition.Active = Condition.Parent && !Condition.Taken && Selected;
      Condition.Taken |= Selected;
    } else if (Opcode == ".else") {
      if (Conditions.empty() || Conditions.back().HasElse ||
          Directive != ".else")
        return false;
      Conditional &Condition = Conditions.back();
      Condition.Active = Condition.Parent && !Condition.Taken;
      Condition.HasElse = true;
    } else if (Opcode == ".endif") {
      if (Conditions.empty() || Directive != ".endif")
        return false;
      Conditions.pop_back();
    } else if (Active) {
      Normalized.append(Line.data(), Line.size());
      if (Line.end() != Text.end())
        Normalized += '\n';
    }
  }
  return Conditions.empty();
}


// Recognize one operation; uncertainty always retains. This inspects operand
// roles and masks, but never rewrites assembly strings or invents modifiers.
static bool safeInlineUse(MachineInstr &MI, MachineOperand &MO) {
  std::string Normalized;
  StringRef Text = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  if (Text.contains(';') || Text.contains('|') || Text.contains("#") ||
      Text.contains("//") || !normalizeConditions(MI, Text, Normalized))
    return false;
  SmallVector<StringRef, 32> Lines;
  StringRef(Normalized).split(Lines, '\n');
  unsigned Headers = 0, Occurrences = 0;
  bool Source = false;
  for (StringRef Line : Lines) {
    Line = Line.trim();
    if (Line.empty()) continue;
    StringRef Opcode = Line.take_until([](char C) { return isSpace(C); });
    bool Binding = Opcode == "B.IOS";
    if (Opcode.startswith("BSTART.")) {
      if (++Headers > 1) return false;
      if (!Line.startswith("BSTART.TLSU TMOV,") &&
          !Line.startswith("BSTART.TLSU TMOV.S2L.BROADCAST,") &&
          !Line.startswith("BSTART.TLSU TMOV.S2L.EXTRACT,") &&
          !Line.startswith("BSTART.TLSU TSTORE,"))
        return false;
    } else if (Opcode == "B.IOR") return false;
    else if (!Binding && Opcode != "B.IOT" && Opcode != "B.DIM" &&
               Opcode != "B.DATR" && Opcode != "B.IOR") return false;
    if (Binding || Opcode == "B.IOT") {
      size_t Mask = Line.find("mask=");
      if (Mask == StringRef::npos ||
          Line.substr(Mask + 5).split(',').first.trim() != "1111")
        return false;
      // A literal source is outside MIR def/use and can alias any Shared slot.
      StringRef Sources = Line.drop_front(Opcode.size()).split("mask=").first;
      for (StringRef Token : llvm::split(Sources, ','))
        if (!Token.trim().empty() && !Token.trim().startswith("$") &&
            !Token.trim().startswith("->")) return false;
    }
    for (size_t Pos = Line.find('$'); Pos != StringRef::npos;
         Pos = Line.find('$', Pos)) {
      size_t Begin = Pos++;
      bool Braced = Pos < Line.size() && Line[Pos] == '{';
      if (Braced) ++Pos;
      size_t Start = Pos;
      while (Pos < Line.size() && isDigit(Line[Pos])) ++Pos;
      unsigned Group;
      if (Start == Pos || Line.slice(Start, Pos).getAsInteger(10, Group))
        return false;
      char Modifier = 0;
      if (Braced) {
        if (Pos < Line.size() && Line[Pos] == ':') {
          if (++Pos == Line.size()) return false;
          Modifier = Line[Pos++];
        }
        if (Pos == Line.size() || Line[Pos++] != '}') return false;
      }
      MachineOperand *Op = getOperand(MI, Group);
      if (!Op || !Op->isReg() || plain(Op->getReg()) != plain(MO.getReg()))
        continue;
      ++Occurrences;
      size_t Arrow = Line.find("->");
      Source = Binding && (Arrow == StringRef::npos || Begin < Arrow) &&
               (!Modifier || Modifier == 'S' || Modifier == 'K') &&
               !Line.substr(Pos).startswith(".reuse");
    }
  }
  return Occurrences == 1 && Source;
}

// References outside MIR operands, unknown control flow, and partial definitions
// cannot be represented by whole-slot physical liveness. Retain throughout the
// function in those cases. Known partial/repeated reads only retain that use.
static bool opaqueInline(MachineInstr &MI) {
  if (MI.getOpcode() == TargetOpcode::INLINEASM_BR)
    return true;
  StringRef Text = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  std::string Normalized;
  if (Text.contains(';') || Text.contains('|') || Text.contains('#') ||
      Text.contains("//") || !normalizeConditions(MI, Text, Normalized))
    return true;
  bool HasDef = llvm::any_of(MI.operands(), [](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && shared(MO.getReg());
  });
  if (HasDef && Text.contains(".if")) return true;
  SmallVector<StringRef, 16> Lines;
  StringRef(Normalized).split(Lines, '\n');
  for (StringRef Line : Lines) {
    Line = Line.trim();
    if (Line.empty()) continue;
    StringRef Opcode = Line.take_until([](char C) { return isSpace(C); });
    if (!Opcode.startswith("BSTART.") && Opcode != "B.IOS" &&
        Opcode != "B.IOT" && Opcode != "B.DIM" && Opcode != "B.DATR" &&
        Opcode != "B.IOR")
      return true;
    for (size_t I = 0; I + 1 < Line.size(); ++I)
      if ((Line[I] == 'S' || Line[I] == 's') && isDigit(Line[I + 1]))
        return true;
    if (Line.contains("shared_s")) return true;
    if (HasDef && (Opcode == "B.IOS" || Opcode == "B.IOR")) {
      size_t Mask = Line.find("mask=");
      if (Mask == StringRef::npos ||
          Line.substr(Mask + 5).split(',').first.trim() != "1111")
        return true;
    }
  }
  return false;
}

class LinxV5SharedReuseFinalize : public MachineFunctionPass {
public:
  static char ID;
  LinxV5SharedReuseFinalize() : MachineFunctionPass(ID) {}
  StringRef getPassName() const override { return "LinxV5 Shared reuse finalize"; }
  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TRI = *MF.getSubtarget().getRegisterInfo();
    SmallVector<MachineOperand *, 32> ForcedRetain;
    for (MachineBasicBlock &B : MF) {
      for (MCPhysReg R : LinxV5::Shared_ABSRegClass)
        B.removeLiveIn(R);
      for (MachineInstr &MI : B)
        for (MachineOperand &MO : MI.operands())
          if (MO.isReg() && MO.isUse() && shared(MO.getReg()) &&
              MO.getReg() != plain(MO.getReg())) {
            ForcedRetain.push_back(&MO);
            MO.setReg(plain(MO.getReg()));
          }
    }
    // Rebuild only Shared live-ins using LLVM's physical liveness transfer.
    // Leave Local queue aliases and their live-in information untouched.
    bool Changed;
    do {
      Changed = false;
      for (MachineBasicBlock &B : llvm::reverse(MF)) {
        LivePhysRegs Live(TRI);
        computeLiveIns(Live, B);
        for (MCPhysReg R : LinxV5::Shared_ABSRegClass) {
          bool Needed = Live.contains(R);
          if (Needed != B.isLiveIn(R)) {
            if (Needed) B.addLiveIn(R); else B.removeLiveIn(R);
            Changed = true;
          }
        }
      }
    } while (Changed);
    SmallVector<MachineOperand *, 32> Retained;
    bool Conservative = EnableGenericElement;
    const Function &F = MF.getFunction();
    if (F.hasFnAttribute("linx-tile-pe-domain") &&
        F.getFnAttribute("linx-tile-pe-domain").getValueAsString() != "15")
      Conservative = true;
    // Opaque assembly and calls can hide architectural Shared slot accesses.
    for (MachineBasicBlock &B : MF)
      for (MachineInstr &MI : B) {
        if (MI.getOpcode() == LinxV5::PseudoV5SharedL2S &&
            (!MI.getOperand(3).isImm() || MI.getOperand(3).getImm() != 15))
          Conservative = true;
        if (MI.isCall() || (MI.isInlineAsm() && opaqueInline(MI)))
          Conservative = true;
        // Escaped Shared identities (e.g. COPY to a GPR) are not tracked by
        // physical Shared liveness.
        if (!MI.isInlineAsm() &&
            MI.getOpcode() != LinxV5::PseudoV5SharedS2L &&
            MI.getOpcode() != LinxV5::PseudoMAMULB_SharedRight_SizeI)
          for (MachineOperand &MO : MI.operands())
            if (MO.isReg() && MO.isUse() && shared(MO.getReg()))
              Conservative = true;
      }
    for (MachineBasicBlock &B : MF) {
      LivePhysRegs Live(TRI);
      Live.addLiveOutsNoPristines(B);
      for (MachineInstr &MI : llvm::reverse(B)) {
        for (MachineOperand &MO : MI.operands()) {
          if (!MO.isReg() || !MO.isUse() || !shared(MO.getReg())) continue;
          Register R = plain(MO.getReg());
          unsigned Uses = 0;
          bool Tied = false;
          for (const MachineOperand &Other : MI.operands())
            if (Other.isReg() && plain(Other.getReg()) == R) {
              Uses += Other.isUse(); Tied |= Other.isDef();
            }
          bool Safe = !Conservative && !MO.isImplicit() && !Tied && Uses == 1 &&
                      !llvm::is_contained(ForcedRetain, &MO);
          unsigned N = MI.getOperandNo(&MO);
          if (MI.isInlineAsm()) Safe &= safeInlineUse(MI, MO);
          else Safe &= (MI.getOpcode() == LinxV5::PseudoV5SharedS2L &&
                        N == 3 && MI.getOperand(4).getImm() == 15) ||
                       (MI.getOpcode() == LinxV5::PseudoMAMULB_SharedRight_SizeI && N == 11);
          bool Kill = Safe && !Live.contains(R);
          bool Supported = MI.isInlineAsm() ||
                           MI.getOpcode() == LinxV5::PseudoV5SharedS2L ||
                           MI.getOpcode() == LinxV5::PseudoMAMULB_SharedRight_SizeI;
          if (!Supported) continue;
          MO.setIsKill(Kill);
          if ((!Kill || MO.getReg() != R) &&
              (MI.isInlineAsm() || MI.getOpcode() == LinxV5::PseudoV5SharedS2L ||
               MI.getOpcode() == LinxV5::PseudoMAMULB_SharedRight_SizeI))
            Retained.push_back(&MO);
        }
        Live.stepBackward(MI);
      }
    }
    Retained.append(ForcedRetain.begin(), ForcedRetain.end());
    // Delay aliases until every liveness query has seen ordinary registers.
    SmallVector<std::pair<MachineInstr *, Register>, 32> Dependencies;
    for (MachineOperand *MO : Retained) {
      // Keep tied operands identical. Explicit R carries their retain request.
      if (MO->isTied()) continue;
      Register R = plain(MO->getReg());
      MachineInstr *MI = MO->getParent();
      MO->setReg(LinxV5::Shared_S0_RU + R - LinxV5::Shared_S0);
      // Encoding aliases are reserved, source-only registers. Preserve the
      // actual data dependence for the verifier and any later MIR observer.
      Dependencies.emplace_back(MI, R);
    }
    for (auto Dep : Dependencies)
      Dep.first->addOperand(MF, MachineOperand::CreateReg(Dep.second, false, true));
    return true;
  }
};
}
char LinxV5SharedReuseFinalize::ID = 0;
INITIALIZE_PASS(LinxV5SharedReuseFinalize, DEBUG_TYPE,
                "LinxV5 Shared reuse finalize", false, false)
FunctionPass *llvm::createLinxV5SharedReuseFinalizePass() {
  return new LinxV5SharedReuseFinalize();
}
