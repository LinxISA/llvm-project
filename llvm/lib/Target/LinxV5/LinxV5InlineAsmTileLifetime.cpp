#include "LinxV5.h"
#include "LinxV5RegisterInfo.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/InlineAsm.h"
#include <map>
#include <string>

using namespace llvm;

#define DEBUG_TYPE "linxv5-inline-asm-tile-lifetime"

static cl::opt<bool> EnableInlineAsmTileLifetime(
    "linxv5-enable-inline-asm-tile-lifetime", cl::init(true), cl::Hidden,
    cl::desc("Infer source lifetimes in recognized Tile inline assembly"));

namespace {
using RegisterSet = DenseSet<unsigned>;

bool isTile(Register Reg) {
  return Reg.isPhysical() &&
         (LinxV5::Shared_ABSRegClass.contains(Reg) ||
          LinxV5::Tile_ABSRegClass.contains(Reg));
}

void transfer(const MachineInstr &MI, RegisterSet &Live) {
  for (const MachineOperand &Operand : MI.operands())
    if (Operand.isReg() && Operand.isDef() && isTile(Operand.getReg()))
      Live.erase(Operand.getReg());
  for (const MachineOperand &Operand : MI.operands())
    if (Operand.isReg() && Operand.isUse() && isTile(Operand.getReg()))
      Live.insert(Operand.getReg());
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

struct Reference {
  size_t Begin;
  size_t End;
  unsigned Group;
  Register Reg;
  bool Source;
  bool Explicit;
  bool Safe;
  unsigned Bundle;
};

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

bool rewrite(MachineInstr &MI, const RegisterSet &LiveAfter,
             const RegisterSet &Escaped, bool HasCall,
             bool AnalyzeOnly = false) {
  if (MI.getOpcode() != TargetOpcode::INLINEASM)
    return false;
  StringRef Text = MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName();
  if (Text.contains(';') || Text.contains('|') || Text.contains("$$") ||
      Text.contains("${:uid}") || Text.contains("#") || Text.contains("//"))
    return false;
  std::string Normalized;
  if (!normalizeConditions(MI, Text, Normalized))
    return false;
  Text = Normalized;
  SmallVector<Reference, 8> References;
  SmallVector<StringRef, 16> Lines;
  Text.split(Lines, '\n');
  size_t Offset = 0;
  bool AllFull = true;
  bool HasLiteralSource = false;
  unsigned BundleNumber = 0;
  for (StringRef Line : Lines) {
    StringRef Trimmed = Line.trim();
    StringRef Opcode = Trimmed.take_until([](char Char) {
      return isSpace(Char);
    });
    bool Binding = Opcode == "B.IOS" || Opcode == "B.IOT";
    if (Opcode.startswith("BSTART."))
      ++BundleNumber;
    if (!Trimmed.empty() && !Binding && Opcode != "BSTART.TLSU" &&
        Opcode != "BSTART.TEPL" && Opcode != "B.DIM" &&
        Opcode != "B.DATR" && Opcode != "B.IOR")
      return false;
    if (Binding) {
      size_t Mask = Line.find("mask=");
      StringRef Value = Mask == StringRef::npos
                            ? StringRef()
                            : Line.substr(Mask + 5).split(',').first.trim();
      if (Value != "1111")
        AllFull = false;
    }
    if (Opcode == "B.IOR")
      AllFull = false;
    if (Binding) {
      StringRef Sources = Trimmed.drop_front(Opcode.size()).trim();
      size_t Mask = Sources.find("mask=");
      Sources = Sources.take_front(Mask).trim(" ,\t");
      SmallVector<StringRef, 4> Tokens;
      Sources.split(Tokens, ',');
      for (StringRef Token : Tokens)
        if (!Token.trim().empty() && !Token.trim().startswith("$")) {
          AllFull = false;
          HasLiteralSource = true;
        }
    }
    if (Opcode == "BSTART.TLSU" &&
        !Trimmed.startswith("BSTART.TLSU TMOV,") &&
        !Trimmed.startswith("BSTART.TLSU TMOV.S2L.BROADCAST,") &&
        !Trimmed.startswith("BSTART.TLSU TMOV.S2L.EXTRACT,") &&
        !Trimmed.startswith("BSTART.TLSU TSTORE,"))
      AllFull = false;
    for (size_t Position = Line.find('$'); Position != StringRef::npos;
         Position = Line.find('$', Position)) {
      size_t Begin = Position++;
      bool Braces = Position < Line.size() && Line[Position] == '{';
      if (Braces)
        ++Position;
      size_t Digits = Position;
      while (Position < Line.size() && isDigit(Line[Position]))
        ++Position;
      unsigned Group;
      if (Digits == Position ||
          Line.slice(Digits, Position).getAsInteger(10, Group))
        return false;
      char Modifier = 0;
      if (Braces) {
        if (Position < Line.size() && Line[Position] == ':') {
          if (++Position == Line.size())
            return false;
          Modifier = Line[Position++];
        }
        if (Position == Line.size() || Line[Position++] != '}')
          return false;
      }
      MachineOperand *Operand = getOperand(MI, Group);
      if (!Operand || !Operand->isReg() || !isTile(Operand->getReg()))
        continue;
      size_t Arrow = Line.find("->");
      bool Source = Binding && (Arrow == StringRef::npos || Begin < Arrow) &&
                    Operand->isUse() && !Operand->isDef();
      bool Explicit = Modifier && Modifier != 'S';
      Explicit |= Line.substr(Position).startswith(".reuse");
      bool Safe = Source && !Operand->isImplicit() &&
                  !MI.isRegTiedToDefOperand(MI.getOperandNo(Operand));
      References.push_back({Offset + Begin, Offset + Position, Group,
                            Operand->getReg(), Source, Explicit, Safe,
                            BundleNumber});
    }
    Offset += Line.size() + 1;
  }
  if (AnalyzeOnly)
    return !HasLiteralSource;
  if (References.empty())
    return false;
  const Function &SourceFunction = MI.getMF()->getFunction();
  if (SourceFunction.hasFnAttribute("linx-tile-pe-domain") &&
      SourceFunction.getFnAttribute("linx-tile-pe-domain")
              .getValueAsString() != "15")
    AllFull = false;
  std::string Result;
  size_t Copied = 0;
  bool Changed = false;
  for (unsigned Index = 0; Index < References.size(); ++Index) {
    const Reference &Ref = References[Index];
    if (!Ref.Source || Ref.Explicit)
      continue;
    bool Retain = EnableGenericElement || !AllFull || !Ref.Safe || HasCall ||
                  LiveAfter.count(Ref.Reg) || Escaped.count(Ref.Reg);
    for (unsigned Other = 0; Other < References.size(); ++Other)
      if (Other != Index && References[Other].Reg == Ref.Reg &&
          (Other > Index || !References[Other].Source ||
           References[Other].Bundle == Ref.Bundle))
        Retain = true;
    for (const MachineOperand &Operand : MI.operands())
      if (Operand.isReg() && Operand.isDef() && Operand.getReg() == Ref.Reg)
        Retain = true;
    Result.append(Text.data() + Copied, Ref.Begin - Copied);
    Result += "${" + std::to_string(Ref.Group) + (Retain ? ":P}" : ":Q}");
    Copied = Ref.End;
    Changed = true;
  }
  if (!Changed)
    return false;
  Result.append(Text.data() + Copied, Text.size() - Copied);
  MI.getOperand(InlineAsm::MIOp_AsmString)
      .ChangeToES(MI.getMF()->createExternalSymbolName(Result));
  return true;
}

class LinxV5InlineAsmTileLifetime : public MachineFunctionPass {
public:
  static char ID;
  LinxV5InlineAsmTileLifetime() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return "LinxV5 inline asm Tile lifetime";
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    if (!EnableInlineAsmTileLifetime)
      return false;
    RegisterSet Escaped;
    bool HasCall = false;
    for (MachineBasicBlock &Block : MF)
      for (MachineInstr &MI : Block) {
        HasCall |= MI.isCall();
        if (MI.isInlineAsm() &&
            !rewrite(MI, RegisterSet(), RegisterSet(), false, true))
          HasCall = true;
        if (MI.isCopy() || (!MI.isInlineAsm() && MI.mayLoadOrStore()))
          for (const MachineOperand &Operand : MI.operands())
            if (Operand.isReg() && isTile(Operand.getReg()))
              Escaped.insert(Operand.getReg());
      }
    std::map<const MachineBasicBlock *, RegisterSet> LiveIns;
    bool Updated;
    do {
      Updated = false;
      for (MachineBasicBlock &Block : llvm::reverse(MF)) {
        RegisterSet Live;
        for (const MachineBasicBlock *Successor : Block.successors())
          Live.insert(LiveIns[Successor].begin(), LiveIns[Successor].end());
        for (const auto &LiveOut : Block.liveouts())
          if (isTile(LiveOut.PhysReg))
            Live.insert(LiveOut.PhysReg);
        for (const MachineInstr &MI : llvm::reverse(Block))
          transfer(MI, Live);
        if (Live != LiveIns[&Block]) {
          LiveIns[&Block] = std::move(Live);
          Updated = true;
        }
      }
    } while (Updated);
    bool Changed = false;
    for (MachineBasicBlock &Block : MF) {
      RegisterSet Live;
      for (const MachineBasicBlock *Successor : Block.successors())
        Live.insert(LiveIns[Successor].begin(), LiveIns[Successor].end());
      for (const auto &LiveOut : Block.liveouts())
        if (isTile(LiveOut.PhysReg))
          Live.insert(LiveOut.PhysReg);
      for (MachineInstr &MI : llvm::reverse(Block)) {
        Changed |= rewrite(MI, Live, Escaped, HasCall);
        transfer(MI, Live);
      }
    }
    return Changed;
  }
};
}

char LinxV5InlineAsmTileLifetime::ID = 0;
INITIALIZE_PASS(LinxV5InlineAsmTileLifetime, DEBUG_TYPE,
                "LinxV5 inline asm Tile lifetime", false, false)

FunctionPass *llvm::createLinxV5InlineAsmTileLifetimePass() {
  return new LinxV5InlineAsmTileLifetime();
}
