//===-- LinxISAAsmPrinter.cpp - LinxISA Assembly Printer ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISAMCInstLower.h"
#include "LinxISARegisterInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "MCTargetDesc/LinxISAOpcodeTables.h"
#include "TargetInfo/LinxISATargetInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"

#include <array>

using namespace llvm;

namespace {

class LinxISAAsmPrinter : public llvm::AsmPrinter {
  struct TileQueueState {
    std::array<SmallVector<Register, 8>, 4> Hands;

    bool operator==(const TileQueueState &Other) const {
      return Hands == Other.Hands;
    }
  };

  std::unique_ptr<LinxISAMCInstLower> MCInstLowering;
  SmallPtrSet<const MachineBasicBlock *, 32> BodyLabelsEmitted;
  SmallPtrSet<const MachineInstr *, 32> SkippedFusedSetRet;
  DenseMap<const MachineInstr *, SmallVector<unsigned, 8>> InlineAsmTileRanks;

  void computeInlineAsmTileRanks(MachineFunction &MF);

public:
  explicit LinxISAAsmPrinter(TargetMachine &TM,
                             std::unique_ptr<MCStreamer> Streamer)
      : llvm::AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "Linx Assembly Printer"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    MCInstLowering = std::make_unique<LinxISAMCInstLower>(
        OutContext, *this, *MF.getSubtarget().getRegisterInfo());
    BodyLabelsEmitted.clear();
    SkippedFusedSetRet.clear();
    InlineAsmTileRanks.clear();
    computeInlineAsmTileRanks(MF);
    return llvm::AsmPrinter::runOnMachineFunction(MF);
  }

  void emitInstruction(const MachineInstr *MI) override;

  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;

  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;

  static char ID;
};

} // end anonymous namespace

void LinxISAAsmPrinter::computeInlineAsmTileRanks(MachineFunction &MF) {
  const TargetRegisterInfo &TRI = *MF.getSubtarget().getRegisterInfo();
  DenseMap<const MachineBasicBlock *, TileQueueState> Entries;
  DenseMap<const MachineBasicBlock *, bool> Processed;
  SmallVector<const MachineBasicBlock *, 32> Worklist;

  auto tileHand = [&](Register Reg) -> unsigned {
    if (!Reg || !Reg.isPhysical() || !LinxISA::TILERegClass.contains(Reg))
      report_fatal_error("Linx: expected physical tile register in inline asm");
    return (TRI.getEncodingValue(Reg) & 0x1fu) / 8u;
  };

  auto consume = [&](TileQueueState &State, Register Reg, StringRef Context) {
    auto &Queue = State.Hands[tileHand(Reg)];
    auto It = llvm::find(Queue, Reg);
    if (It == Queue.end())
      report_fatal_error(Twine("Linx: cannot consume absent inline-asm tile "
                               "value for ") +
                         Context);
    Queue.erase(It);
  };

  auto push = [&](TileQueueState &State, Register Reg) {
    auto &Queue = State.Hands[tileHand(Reg)];
    if (auto It = llvm::find(Queue, Reg); It != Queue.end())
      Queue.erase(It);
    Queue.insert(Queue.begin(), Reg);
    if (Queue.size() > 8u)
      report_fatal_error(
          "Linx: inline-asm tile queue exceeds architectural depth 8");
  };

  auto transfer = [&](const MachineInstr &MI, TileQueueState &State) {
    if (!MI.isInlineAsm())
      return;

    SmallVector<Register, 4> Consumed;
    SmallVector<Register, 4> Clobbered;
    SmallVector<Register, 4> Defined;
    SmallVector<unsigned, 8> Ranks(MI.getNumOperands(), 0u);
    bool HasTileOperand = false;

    for (unsigned FlagNo = InlineAsm::MIOp_FirstOperand;
         FlagNo < MI.getNumOperands();) {
      const MachineOperand &FlagMO = MI.getOperand(FlagNo);
      if (!FlagMO.isImm())
        break;
      const InlineAsm::Flag Flag(FlagMO.getImm());
      const unsigned NumRegs = Flag.getNumOperandRegisters();
      if (FlagNo + 1u + NumRegs > MI.getNumOperands())
        report_fatal_error("Linx: malformed inline-asm operand descriptor");

      for (unsigned I = 0; I != NumRegs; ++I) {
        const unsigned OpNo = FlagNo + 1u + I;
        const MachineOperand &MO = MI.getOperand(OpNo);
        if (!MO.isReg() || !LinxISA::TILERegClass.contains(MO.getReg()))
          continue;

        HasTileOperand = true;
        const Register Reg = MO.getReg();
        if (Flag.isRegUseKind()) {
          const auto &Queue = State.Hands[tileHand(Reg)];
          auto It = llvm::find(Queue, Reg);
          if (It == Queue.end())
            report_fatal_error("Linx: cannot prove inline-asm tile queue rank");
          const unsigned Rank = static_cast<unsigned>(It - Queue.begin()) + 1u;
          if (Rank > 8u)
            report_fatal_error(
                "Linx: inline-asm tile queue rank exceeds depth 8");
          Ranks[OpNo] = Rank;
          if (MO.isKill() && !llvm::is_contained(Consumed, Reg))
            Consumed.push_back(Reg);
        } else if (Flag.isRegDefKind() || Flag.isRegDefEarlyClobberKind()) {
          if (!llvm::is_contained(Defined, Reg))
            Defined.push_back(Reg);
        } else if (Flag.isClobberKind()) {
          if (!llvm::is_contained(Clobbered, Reg))
            Clobbered.push_back(Reg);
        }
      }
      FlagNo += 1u + NumRegs;
    }

    if (HasTileOperand)
      InlineAsmTileRanks[&MI] = std::move(Ranks);

    // Inputs are read before early-clobber outputs become architecturally
    // visible.  Record every source rank first, then update the queue.
    for (Register Reg : Consumed)
      consume(State, Reg, "inline asm");
    for (Register Reg : Clobbered) {
      auto &Queue = State.Hands[tileHand(Reg)];
      if (auto It = llvm::find(Queue, Reg); It != Queue.end())
        Queue.erase(It);
    }
    for (Register Reg : Defined)
      push(State, Reg);
  };

  for (const MachineBasicBlock &MBB : MF) {
    if (MBB.pred_empty()) {
      Entries.try_emplace(&MBB);
      Worklist.push_back(&MBB);
    }
  }
  if (!MF.empty() && !Entries.count(&MF.front())) {
    Entries.try_emplace(&MF.front());
    Worklist.push_back(&MF.front());
  }

  while (!Worklist.empty()) {
    const MachineBasicBlock *MBB = Worklist.pop_back_val();
    if (Processed.lookup(MBB))
      continue;
    Processed[MBB] = true;
    TileQueueState Exit = Entries.lookup(MBB);
    for (const MachineInstr &MI : *MBB)
      transfer(MI, Exit);

    for (const MachineBasicBlock *Succ : MBB->successors()) {
      auto [It, Inserted] = Entries.try_emplace(Succ, Exit);
      if (!Inserted && !(It->second == Exit))
        report_fatal_error(
            "Linx: inline-asm tile queue is ambiguous at a CFG join");
      if (Inserted)
        Worklist.push_back(Succ);
    }
  }

  for (const MachineBasicBlock &MBB : MF) {
    if (Processed.lookup(&MBB))
      continue;
    for (const MachineInstr &MI : MBB) {
      if (MI.isInlineAsm()) {
        for (const MachineOperand &MO : MI.operands()) {
          if (MO.isReg() && LinxISA::TILERegClass.contains(MO.getReg()))
            report_fatal_error("Linx: inline-asm tile queue is not provable "
                               "through unreachable or cyclic control flow");
        }
      }
    }
  }
}

void LinxISAAsmPrinter::emitInstruction(const MachineInstr *MI) {
  if (SkippedFusedSetRet.contains(MI))
    return;
  if (MI->isDebugInstr())
    return;

  switch (MI->getOpcode()) {
  // Call frame pseudos should have been eliminated by PEI.
  case LinxISA::ADJCALLSTACKDOWN:
  case LinxISA::ADJCALLSTACKUP:
    return;
  default:
    break;
  }

  bool Emitted = false;
  if (MI->getOpcode() == LinxISA::BSTART_STD_CALL &&
      OutStreamer->hasRawTextSupport()) {
    MachineBasicBlock *MBB = const_cast<MachineBasicBlock *>(MI->getParent());
    if (MBB) {
      auto It = const_cast<MachineInstr *>(MI)->getIterator();
      auto NextIt = std::next(It);
      while (NextIt != MBB->end() && NextIt->isDebugInstr())
        ++NextIt;

      if (NextIt != MBB->end() && NextIt->getOpcode() == LinxISA::SETRET) {
        MCInst BStartInst;
        MCInstLowering->Lower(MI, BStartInst);
        MCInst SetRetInst;
        MCInstLowering->Lower(&*NextIt, SetRetInst);

        SmallString<128> Line;
        raw_svector_ostream OS(Line);
        OS << "BSTART\tCALL, ";

        if (BStartInst.getNumOperands() >= 1) {
          const MCOperand &TargetOp = BStartInst.getOperand(0);
          if (TargetOp.isExpr())
            MAI->printExpr(OS, *TargetOp.getExpr());
          else if (TargetOp.isImm())
            OS << TargetOp.getImm();
        }

        OS << ", ra=";
        if (SetRetInst.getNumOperands() >= 1) {
          const MCOperand &RetOp = SetRetInst.getOperand(0);
          if (RetOp.isExpr())
            MAI->printExpr(OS, *RetOp.getExpr());
          else if (RetOp.isImm())
            OS << RetOp.getImm();
        }

        OutStreamer->emitRawText(OS.str());
        SkippedFusedSetRet.insert(&*NextIt);
        Emitted = true;
      }
    }
  }

  if (!Emitted) {
    MCSubtargetInfo STI = getSubtargetInfo();
    MCInst TmpInst;
    MCInstLowering->Lower(MI, TmpInst);
    OutStreamer->emitInstruction(TmpInst, STI);
  }
}

static StringRef linxReg5Name(unsigned Code) {
  static constexpr const char *Names[32] = {
      "zero", "sp",  "a0",  "a1",  "a2",  "a3",  "a4",  "a5",
      "a6",   "a7",  "ra",  "s0",  "s1",  "s2",  "s3",  "s4",
      "s5",   "s6",  "s7",  "s8",  "x0",  "x1",  "x2",  "x3",
      "t#1",  "t#2", "t#3", "t#4", "u#1", "u#2", "u#3", "u#4",
  };
  if (Code < 32)
    return Names[Code];
  return "r?";
}

static void printLinxInlineAsmRegister(raw_ostream &OS, unsigned Reg,
                                       unsigned Enc) {
  if (LinxISA::SHAREDRegClass.contains(Reg)) {
    OS << 'S' << Enc;
    return;
  }
  if (LinxISA::TILERegClass.contains(Reg)) {
    static constexpr char Banks[4] = {'t', 'u', 'm', 'n'};
    OS << Banks[(Enc >> 3) & 0x3] << '#' << ((Enc & 0x7) + 1);
    return;
  }
  OS << linxReg5Name(Enc);
}

static StringRef linxTileDataTypeName(int64_t Value) {
  switch (Value) {
  case 0:
    return "FP64";
  case 1:
    return "FP32";
  case 2:
    return "TF32";
  case 3:
    return "HF32";
  case 4:
    return "FP16";
  case 5:
    return "BF16";
  case 6:
    return "HIF8";
  case 7:
    return "E4M3";
  case 8:
    return "E5M2";
  case 9:
    return "E3M2";
  case 10:
    return "E2M3";
  case 11:
    return "E2M1X2";
  case 12:
    return "E1M2X2";
  case 13:
    return "E8M0";
  case 14:
    return "HIF4X2";
  case 16:
    return "S64";
  case 17:
    return "S32";
  case 18:
    return "S16";
  case 19:
    return "S8";
  case 20:
    return "S4X2";
  case 24:
    return "U64";
  case 25:
    return "U32";
  case 26:
    return "U16";
  case 27:
    return "U8";
  case 28:
    return "U4X2";
  default:
    return {};
  }
}

bool LinxISAAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                        const char *ExtraCode,
                                        raw_ostream &OS) {
  // Clang/GCC use %cN for an immediate without target punctuation. Linx block
  // templates rely on that conventional modifier for selector and dimension
  // fields. %DN prints a tile dtype keyword for attribute commands. %SN
  // prints a compiler-allocated Shared register as S0..S255. %qN
  // prints only a tile register's destination queue bank because
  // canonical B.IOT destinations are written as ->t/u/m/n<Size>, while source
  // operands name a concrete queue slot such as t#1.
  if (ExtraCode && ExtraCode[0] != 0 &&
      !((ExtraCode[0] == 'c' || ExtraCode[0] == 'D' || ExtraCode[0] == 'S' ||
         ExtraCode[0] == 'q') &&
        ExtraCode[1] == 0))
    return true;

  if (!MCInstLowering)
    return true;
  if (OpNo >= MI->getNumOperands())
    return true;

  const MachineOperand &MO = MI->getOperand(OpNo);
  if (MO.isReg()) {
    const unsigned Enc = MCInstLowering->getRegEncoding(MO.getReg());
    if (ExtraCode && ExtraCode[0] == 'S' &&
        !LinxISA::SHAREDRegClass.contains(MO.getReg()))
      return true;
    if (ExtraCode && ExtraCode[0] == 'q') {
      if (!LinxISA::TILERegClass.contains(MO.getReg()))
        return true;
      static constexpr char Banks[4] = {'t', 'u', 'm', 'n'};
      OS << Banks[(Enc >> 3) & 0x3];
      return false;
    }
    if (LinxISA::TILERegClass.contains(MO.getReg())) {
      auto It = InlineAsmTileRanks.find(MI);
      if (It == InlineAsmTileRanks.end() || OpNo >= It->second.size() ||
          It->second[OpNo] == 0u)
        report_fatal_error(
            "Linx: missing proven queue rank for inline-asm tile source");
      static constexpr char Banks[4] = {'t', 'u', 'm', 'n'};
      OS << Banks[(Enc >> 3) & 0x3] << '#' << It->second[OpNo];
      return false;
    }
    printLinxInlineAsmRegister(OS, MO.getReg(), Enc);
    return false;
  }

  if (MO.isImm()) {
    if (ExtraCode && ExtraCode[0] == 'D') {
      StringRef Name = linxTileDataTypeName(MO.getImm());
      if (Name.empty())
        return true;
      OS << Name;
      return false;
    }
    OS << MO.getImm();
    return false;
  }

  if (MO.isCImm()) {
    if (ExtraCode && ExtraCode[0] == 'D') {
      StringRef Name = linxTileDataTypeName(MO.getCImm()->getSExtValue());
      if (Name.empty())
        return true;
      OS << Name;
      return false;
    }
    OS << MO.getCImm()->getSExtValue();
    return false;
  }

  // Symbols and expressions.
  MCOperand MCOp;
  if (MCInstLowering->lowerOperand(MO, MCOp) && MCOp.isExpr()) {
    MAI->printExpr(OS, *MCOp.getExpr());
    return false;
  }

  return true;
}

bool LinxISAAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                              unsigned OpNo,
                                              const char *ExtraCode,
                                              raw_ostream &OS) {
  if (ExtraCode && ExtraCode[0] != 0)
    return true;

  if (!MCInstLowering)
    return true;
  if (OpNo >= MI->getNumOperands())
    return true;

  auto PrintOne = [&](const MachineOperand &MO) -> bool {
    if (MO.isReg()) {
      const unsigned Enc = MCInstLowering->getRegEncoding(MO.getReg());
      printLinxInlineAsmRegister(OS, MO.getReg(), Enc);
      return false;
    }

    if (MO.isImm()) {
      OS << MO.getImm();
      return false;
    }

    if (MO.isCImm()) {
      OS << MO.getCImm()->getSExtValue();
      return false;
    }

    MCOperand MCOp;
    if (MCInstLowering->lowerOperand(MO, MCOp) && MCOp.isExpr()) {
      MAI->printExpr(OS, *MCOp.getExpr());
      return false;
    }

    return true;
  };

  const MachineOperand &BaseMO = MI->getOperand(OpNo);
  OS << "[";
  if (PrintOne(BaseMO))
    return true;

  if (OpNo + 1 < MI->getNumOperands()) {
    const MachineOperand &OffMO = MI->getOperand(OpNo + 1);
    bool EmitOffset = true;
    if (OffMO.isImm() && OffMO.getImm() == 0)
      EmitOffset = false;
    if (OffMO.isCImm() && OffMO.getCImm()->isZero())
      EmitOffset = false;

    if (EmitOffset) {
      OS << ", ";
      if (PrintOne(OffMO))
        return true;
    }
  }

  OS << "]";
  return false;
}

char LinxISAAsmPrinter::ID = 0;

INITIALIZE_PASS(LinxISAAsmPrinter, "linx-asm-printer", "Linx Assembly Printer",
                false, false)

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeLinxISAAsmPrinter() {
  RegisterAsmPrinter<LinxISAAsmPrinter> X32(getTheLinx32Target());
  RegisterAsmPrinter<LinxISAAsmPrinter> X64(getTheLinx64Target());
}
