//===-- LinxISABlockify.cpp - Block boundary + T-hand lowering ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxISA.h"
#include "LinxISABaseInfo.h"
#include "LinxISAInstrInfo.h"
#include "LinxISARegisterInfo.h"
#include "MCTargetDesc/LinxISAMCTargetDesc.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/MC/MCContext.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <limits>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "linx-blockify"

namespace {

struct LocalDUInfo {
  unsigned DefCount = 0;
  unsigned UseCount = 0;
  unsigned DefIdx = 0;
  unsigned UseIdx = 0;
  MachineInstr *DefMI = nullptr;
  MachineInstr *UseMI = nullptr;
  unsigned DefOpNo = 0;
  unsigned UseOpNo = 0;
};

static bool isMarkerInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::CBSTART_STD:
  case LinxISA::BSTART_STD_FALL:
  case LinxISA::BSTART_STD_DIRECT:
  case LinxISA::BSTART_STD_COND:
  case LinxISA::BSTART_STD_CALL:
  case LinxISA::BSTART_STD_IND:
  case LinxISA::BSTART_STD_ICALL:
  case LinxISA::BSTART_STD_RET:
  case LinxISA::BSTART_TMA:
  case LinxISA::BSTART_CUBE:
  case LinxISA::BSTART_VPAR:
  case LinxISA::BSTART_VSEQ:
  case LinxISA::BSTOP:
    return true;
  default:
    return false;
  }
}

static bool isFrameMacroInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::FENTRY:
  case LinxISA::FEXIT:
  case LinxISA::FRET_RA:
  case LinxISA::FRET_STK:
  case LinxISA::MCOPY:
  case LinxISA::MSET:
    return true;
  default:
    return false;
  }
}

static bool isTilePseudoInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::PSEUDO_TMA_TLOAD:
  case LinxISA::PSEUDO_TMA_TLOAD_ANY:
  case LinxISA::PSEUDO_TMA_TLOAD_DESC:
  case LinxISA::PSEUDO_TMA_TSTORE:
  case LinxISA::PSEUDO_TMA_TSTORE_DESC:
  case LinxISA::PSEUDO_CUBE_MAMULB:
  case LinxISA::PSEUDO_CUBE_MAMULB_ACC:
  case LinxISA::PSEUDO_VPAR_TADD:
  case LinxISA::PSEUDO_VPAR_TSUB:
  case LinxISA::PSEUDO_VTILE_ADD:
  case LinxISA::PSEUDO_VTILE_SUB:
    return true;
  default:
    return false;
  }
}

static bool isVBlockPseudoInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::PSEUDO_VBLOCK_LAUNCH:
    return true;
  default:
    return false;
  }
}

static bool isTileBlockStartInstr(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  case LinxISA::BSTART_TMA:
  case LinxISA::BSTART_CUBE:
    return true;
  default:
    return false;
  }
}

static bool isStandaloneFrameMacroBlock(const MachineBasicBlock &MBB) {
  const MachineInstr *MacroMI = nullptr;
  for (const MachineInstr &MI : MBB) {
    if (MI.isDebugInstr() || MI.isCFIInstruction())
      continue;
    if (isMarkerInstr(MI))
      continue;
    if (!isFrameMacroInstr(MI))
      return false;
    if (MacroMI)
      return false;
    MacroMI = &MI;
  }
  return MacroMI != nullptr;
}

static bool canEncodeShiftedSignedImm(int64_t Imm, unsigned BaseBits) {
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    int64_t Pow = (1LL << Sh);
    if (Imm % Pow != 0)
      continue;
    int64_t Base = Imm / Pow;
    if (isIntN(BaseBits, Base))
      return true;
  }
  return false;
}

static bool canEncodeShiftedUnsignedImm(int64_t Imm, unsigned BaseBits) {
  if (Imm < 0)
    return false;
  uint64_t UImm = static_cast<uint64_t>(Imm);
  for (unsigned Sh = 0; Sh < 32; ++Sh) {
    uint64_t Pow = (1ULL << Sh);
    if (UImm % Pow != 0)
      continue;
    uint64_t Base = UImm / Pow;
    if (isUIntN(BaseBits, Base))
      return true;
  }
  return false;
}

static Register getTQueueUseReg(unsigned Index) {
  switch (Index) {
  case 1:
    return LinxISA::T1;
  case 2:
    return LinxISA::T2;
  case 3:
    return LinxISA::T3;
  case 4:
    return LinxISA::T4;
  default:
    return Register();
  }
}

static Register getUQueueUseReg(unsigned Index) {
  switch (Index) {
  case 1:
    return LinxISA::U1;
  case 2:
    return LinxISA::U2;
  case 3:
    return LinxISA::U3; // u#3
  case 4:
    return LinxISA::U4; // u#4
  default:
    return Register();
  }
}

class LinxISABlockify : public MachineFunctionPass {
public:
  static char ID;

  LinxISABlockify() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return "Linx Blockify"; }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const auto &TII = *MF.getSubtarget().getInstrInfo();
    const auto &TRI = *MF.getSubtarget().getRegisterInfo();

    const BitVector Reserved = TRI.getReservedRegs(MF);
    bool Changed = false;

    // Per-function empty decoupled-body stub used by tile/vector block headers.
    // This is emitted as a linear snippet that terminates at BSTOP and is
    // referenced via B.TEXT from decoupled headers.
    MachineBasicBlock *EmptyBodyBB = nullptr;
    MCSymbol *EmptyBodySym = nullptr;
    MachineBasicBlock *VTileAddBodyBB = nullptr;
    MCSymbol *VTileAddBodySym = nullptr;
    MachineBasicBlock *VTileSubBodyBB = nullptr;
    MCSymbol *VTileSubBodySym = nullptr;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (MI.getOpcode() != TargetOpcode::EH_LABEL || MI.getNumOperands() < 1)
          continue;
        const MachineOperand &MO = MI.getOperand(0);
        if (!MO.isMCSymbol())
          continue;
        MCSymbol *Sym = MO.getMCSymbol();
        if (!Sym)
          continue;
        if (Sym->getName().starts_with(".__linx_empty_body.")) {
          EmptyBodyBB = &MBB;
          EmptyBodySym = Sym;
        } else if (Sym->getName().starts_with(".__linx_vtile_add_body.")) {
          VTileAddBodyBB = &MBB;
          VTileAddBodySym = Sym;
        } else if (Sym->getName().starts_with(".__linx_vtile_sub_body.")) {
          VTileSubBodyBB = &MBB;
          VTileSubBodySym = Sym;
          break;
        }
      }
      if (EmptyBodyBB && VTileAddBodyBB && VTileSubBodyBB)
        break;
    }

    SmallPtrSet<MachineBasicBlock *, 8> DecoupledBodyBBs;
    if (EmptyBodyBB)
      DecoupledBodyBBs.insert(EmptyBodyBB);
    if (VTileAddBodyBB)
      DecoupledBodyBBs.insert(VTileAddBodyBB);
    if (VTileSubBodyBB)
      DecoupledBodyBBs.insert(VTileSubBodyBB);

    auto getOrCreateEmptyBodySym = [&]() -> MCSymbol * {
      if (EmptyBodySym)
        return EmptyBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_empty_body." << MF.getFunctionNumber();
      EmptyBodySym = Ctx.getOrCreateSymbol(OS.str());

      EmptyBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), EmptyBodyBB);
      EmptyBodyBB->setLabelMustBeEmitted();

      BuildMI(*EmptyBodyBB, EmptyBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(EmptyBodySym);
      BuildMI(*EmptyBodyBB, EmptyBodyBB->end(), DebugLoc(),
              TII.get(LinxISA::BSTOP));

      DecoupledBodyBBs.insert(EmptyBodyBB);
      Changed = true;
      return EmptyBodySym;
    };

    auto getOrCreateVTileAddBodySym = [&]() -> MCSymbol * {
      if (VTileAddBodySym)
        return VTileAddBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_vtile_add_body." << MF.getFunctionNumber();
      VTileAddBodySym = Ctx.getOrCreateSymbol(OS.str());

      VTileAddBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), VTileAddBodyBB);
      VTileAddBodyBB->setLabelMustBeEmitted();

      BuildMI(*VTileAddBodyBB, VTileAddBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(VTileAddBodySym);

      static const char kBodyAsm[] =
          "  v.lw.local [ta, lc0<<2, lc1<<8], ->vt.w\n"
          "  v.lw.local [tb, lc0<<2, lc1<<8], ->vu.w\n"
          "  v.add vt#1.sw, vu#1.sw, ->vt.w\n"
          "  v.sw.local vt#1, [to, lc0<<2, lc1<<8]\n"
          "  C.BSTOP\n";
      BuildMI(*VTileAddBodyBB, VTileAddBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::INLINEASM))
          .addExternalSymbol(kBodyAsm)
          .addImm(InlineAsm::Extra_HasSideEffects);

      DecoupledBodyBBs.insert(VTileAddBodyBB);
      Changed = true;
      return VTileAddBodySym;
    };

    auto getOrCreateVTileSubBodySym = [&]() -> MCSymbol * {
      if (VTileSubBodySym)
        return VTileSubBodySym;

      MCContext &Ctx = MF.getContext();
      SmallString<64> Name;
      raw_svector_ostream OS(Name);
      OS << ".__linx_vtile_sub_body." << MF.getFunctionNumber();
      VTileSubBodySym = Ctx.getOrCreateSymbol(OS.str());

      VTileSubBodyBB = MF.CreateMachineBasicBlock();
      MF.insert(MF.end(), VTileSubBodyBB);
      VTileSubBodyBB->setLabelMustBeEmitted();

      BuildMI(*VTileSubBodyBB, VTileSubBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::EH_LABEL))
          .addSym(VTileSubBodySym);

      static const char kBodyAsm[] =
          "  v.lw.local [ta, lc0<<2, lc1<<8], ->vt.w\n"
          "  v.lw.local [tb, lc0<<2, lc1<<8], ->vu.w\n"
          "  v.sub vt#1.sw, vu#1.sw, ->vt.w\n"
          "  v.sw.local vt#1, [to, lc0<<2, lc1<<8]\n"
          "  C.BSTOP\n";
      BuildMI(*VTileSubBodyBB, VTileSubBodyBB->end(), DebugLoc(),
              TII.get(TargetOpcode::INLINEASM))
          .addExternalSymbol(kBodyAsm)
          .addImm(InlineAsm::Extra_HasSideEffects);

      DecoupledBodyBBs.insert(VTileSubBodyBB);
      Changed = true;
      return VTileSubBodySym;
    };

    auto splitAfterCall = [&](MachineBasicBlock &MBB, MachineInstr &CallMI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *ContBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), ContBB);

      // Move everything after the call into the continuation block.
      auto SplitPt = std::next(CallMI.getIterator());
      ContBB->splice(ContBB->end(), &MBB, SplitPt, MBB.end());

      // Continuation inherits the original CFG edges; call block falls through to
      // the continuation after return.
      ContBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(ContBB);
      return ContBB;
    };

    auto splitAfterInstr = [&](MachineBasicBlock &MBB, MachineInstr &MI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *ContBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), ContBB);

      auto SplitPt = std::next(MI.getIterator());
      ContBB->splice(ContBB->end(), &MBB, SplitPt, MBB.end());

      ContBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(ContBB);
      return ContBB;
    };

    auto splitBeforeInstr = [&](MachineBasicBlock &MBB, MachineInstr &MI)
        -> MachineBasicBlock * {
      MachineFunction &MF = *MBB.getParent();
      auto *TailBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      MF.insert(std::next(MBB.getIterator()), TailBB);

      auto SplitPt = MI.getIterator();
      TailBB->splice(TailBB->end(), &MBB, SplitPt, MBB.end());

      TailBB->transferSuccessorsAndUpdatePHIs(&MBB);
      MBB.addSuccessor(TailBB);
      return TailBB;
    };

    // Ensure PSEUDO_CALL ends a block. This matches BlockISA: the call is the
    // block's outgoing control-flow (encoded in the BSTART header), and the
    // return target is the next block (encoded via SETRET).
    SmallVector<MachineBasicBlock *, 32> CallSplitWorklist;
    CallSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      CallSplitWorklist.push_back(&MBB);

    while (!CallSplitWorklist.empty()) {
      MachineBasicBlock *MBB = CallSplitWorklist.pop_back_val();
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr())
          continue;
        if (MI.getOpcode() != LinxISA::PSEUDO_CALL)
          continue;

        auto Next = std::next(MI.getIterator());
        while (Next != MBB->end() && Next->isDebugInstr())
          ++Next;
        if (Next == MBB->end())
          break; // already ends the block

        MachineBasicBlock *ContBB = splitAfterCall(*MBB, MI);
        CallSplitWorklist.push_back(ContBB);
        Changed = true;
        break;
      }
    }

    // Ensure frame macro instructions are standalone blocks.
    //
    // FENTRY/FEXIT/FRET.* are "block instructions": they already contain the
    // required block markers and micro-ops for stack/register management. Some
    // mid/late CodeGen passes may merge these blocks back into surrounding
    // blocks; re-split here so the final assembly keeps them isolated.
    SmallVector<MachineBasicBlock *, 32> MacroSplitWorklist;
    MacroSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      MacroSplitWorklist.push_back(&MBB);

    while (!MacroSplitWorklist.empty()) {
      MachineBasicBlock *MBB = MacroSplitWorklist.pop_back_val();
      MachineInstr *MacroMI = nullptr;
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isFrameMacroInstr(MI)) {
          MacroMI = &MI;
          break;
        }
      }

      if (!MacroMI || isStandaloneFrameMacroBlock(*MBB))
        continue;

      auto hasRealInstrBefore = [&](const MachineInstr &Anchor) -> bool {
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (&MI == &Anchor)
            return false;
          return true;
        }
        return false;
      };
      auto hasRealInstrAfter = [&](const MachineInstr &Anchor) -> bool {
        bool SeenAnchor = false;
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (!SeenAnchor) {
            SeenAnchor = (&MI == &Anchor);
            continue;
          }
          return true;
        }
        return false;
      };

      switch (MacroMI->getOpcode()) {
      case LinxISA::FENTRY: {
        if (hasRealInstrBefore(*MacroMI))
          report_fatal_error("Linx: FENTRY must be the first instruction in its block");
        if (!hasRealInstrAfter(*MacroMI))
          continue;
        MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *MacroMI);
        MacroSplitWorklist.push_back(ContBB);
        Changed = true;
        break;
      }
      case LinxISA::FEXIT:
      case LinxISA::FRET_RA:
      case LinxISA::FRET_STK: {
        if (hasRealInstrAfter(*MacroMI))
          report_fatal_error("Linx: frame macro must be the last instruction in its block");
        if (!hasRealInstrBefore(*MacroMI))
          continue;
        MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *MacroMI);
        MacroSplitWorklist.push_back(TailBB);
        Changed = true;
        break;
      }
      case LinxISA::MCOPY:
      case LinxISA::MSET: {
        // Template blocks are standalone block start markers and must not be
        // merged with surrounding instructions.
        if (hasRealInstrBefore(*MacroMI)) {
          MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *MacroMI);
          MacroSplitWorklist.push_back(TailBB);
          Changed = true;
          break;
        }
        if (hasRealInstrAfter(*MacroMI)) {
          MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *MacroMI);
          MacroSplitWorklist.push_back(ContBB);
          Changed = true;
          break;
        }
        break;
      }
      default:
        break;
      }
    }

    // Ensure tile/vector pseudo instructions are standalone blocks, then expand
    // them to decoupled-header descriptor sequences.
    //
    // Tile blocks are block-structured ISA units; their headers must be the
    // first real instruction in the block.
    SmallVector<MachineBasicBlock *, 32> TileSplitWorklist;
    TileSplitWorklist.reserve(MF.size());
    for (MachineBasicBlock &MBB : MF)
      TileSplitWorklist.push_back(&MBB);

    while (!TileSplitWorklist.empty()) {
      MachineBasicBlock *MBB = TileSplitWorklist.pop_back_val();
      MachineInstr *PseudoMI = nullptr;
      for (MachineInstr &MI : *MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isTilePseudoInstr(MI) || isVBlockPseudoInstr(MI)) {
          PseudoMI = &MI;
          break;
        }
      }
      if (!PseudoMI)
        continue;

      auto hasRealInstrBefore = [&](const MachineInstr &Anchor) -> bool {
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (&MI == &Anchor)
            return false;
          return true;
        }
        return false;
      };
      auto hasRealInstrAfter = [&](const MachineInstr &Anchor) -> bool {
        bool SeenAnchor = false;
        for (const MachineInstr &MI : *MBB) {
          if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
            continue;
          if (!SeenAnchor) {
            SeenAnchor = (&MI == &Anchor);
            continue;
          }
          return true;
        }
        return false;
      };

      if (hasRealInstrBefore(*PseudoMI)) {
        MachineBasicBlock *TailBB = splitBeforeInstr(*MBB, *PseudoMI);
        TileSplitWorklist.push_back(TailBB);
        Changed = true;
        continue;
      }
      if (hasRealInstrAfter(*PseudoMI)) {
        MachineBasicBlock *ContBB = splitAfterInstr(*MBB, *PseudoMI);
        TileSplitWorklist.push_back(ContBB);
        Changed = true;
        continue;
      }
    }

    // Expand tile pseudos now that they are isolated blocks (and registers are
    // physical after RA).
    auto tileRegId = [&](Register Reg) -> unsigned {
      if (!Reg || !Reg.isPhysical())
        report_fatal_error("Linx: expected physical tile register");
      return TRI.getEncodingValue(Reg) & 0x1fu;
    };

    for (MachineBasicBlock &MBB : MF) {
      MachineInstr *PseudoMI = nullptr;
      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
          continue;
        if (isTilePseudoInstr(MI) || isVBlockPseudoInstr(MI)) {
          PseudoMI = &MI;
          break;
        }
      }
      if (!PseudoMI)
        continue;

      // Strip any stale standard block markers (in case the pass runs twice).
      for (auto It = MBB.begin(); It != MBB.end();) {
        if (It->isDebugInstr() || It->isCFIInstruction()) {
          ++It;
          continue;
        }
        if (It->getOpcode() == LinxISA::CBSTART_STD ||
            It->getOpcode() == LinxISA::BSTART_STD_FALL ||
            It->getOpcode() == LinxISA::BSTART_STD_DIRECT ||
            It->getOpcode() == LinxISA::BSTART_STD_COND ||
            It->getOpcode() == LinxISA::BSTART_STD_CALL ||
            It->getOpcode() == LinxISA::BSTART_STD_IND ||
            It->getOpcode() == LinxISA::BSTART_STD_ICALL ||
            It->getOpcode() == LinxISA::BSTART_STD_RET) {
          It = MBB.erase(It);
          Changed = true;
          continue;
        }
        ++It;
      }

      DebugLoc DL = PseudoMI->getDebugLoc();
      auto InsertPt = PseudoMI->getIterator();

      // Remove any markers that may have been left immediately before the
      // pseudo; tile headers must be first real instruction.
      while (InsertPt != MBB.begin()) {
        auto Prev = std::prev(InsertPt);
        if (Prev->isDebugInstr() || Prev->isCFIInstruction()) {
          InsertPt = Prev;
          continue;
        }
        if (isMarkerInstr(*Prev)) {
          Prev->eraseFromParent();
          Changed = true;
          continue;
        }
        break;
      }

      constexpr unsigned DType_I32 = 0;
      constexpr unsigned TMA_TLOAD = 0;
      constexpr unsigned TMA_TSTORE = 1;
      constexpr unsigned CUBE_MAMULB = 0;
      constexpr unsigned CUBE_MAMULB_ACC = 2;
      constexpr unsigned CUBE_ACCCVT = 8;

      auto emitDim = [&](MachineBasicBlock &DimMBB, MachineBasicBlock::iterator DimInsertPt,
                         unsigned LoopNest, int64_t Imm) {
        if (Imm >= 0 && Imm <= 255) {
          BuildMI(DimMBB, DimInsertPt, DL, TII.get(LinxISA::C_B_DIMI))
              .addImm(LoopNest)
              .addImm(Imm);
          return;
        }
        const unsigned BDimOpc = (LoopNest == 0)   ? LinxISA::B_DIM_LB0
                               : (LoopNest == 1) ? LinxISA::B_DIM_LB1
                               :                  LinxISA::B_DIM_LB2;
        BuildMI(DimMBB, DimInsertPt, DL, TII.get(BDimOpc))
            .addReg(LinxISA::R0)
            .addImm(Imm);
      };

      auto tileKindFromId = [](unsigned ID) -> unsigned {
        if (ID < 8)
          return 0; // t
        if (ID < 16)
          return 1; // u
        if (ID < 24)
          return 2; // m
        return 3; // n
      };

      auto checkTMAArg = [](int64_t Arg) {
        if (Arg < 0 || Arg > 31 || ((Arg & 0x7) > 4))
          report_fatal_error("Linx: TMA B.ARG must encode NORM/ND2NZ/ND2ZN/DN2NZ/DN2ZN");
      };

      switch (PseudoMI->getOpcode()) {
      case LinxISA::PSEUDO_TMA_TLOAD: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();

        const unsigned DstID = tileRegId(Dst);
        if (DstID >= 16)
          report_fatal_error("Linx: TMA.TLOAD dst must be in TILE0..TILE15");

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TLOAD);

        // Canonical descriptor-carrying TLOAD header:
        //   B.DIM(LB0/LB1/LB2) + B.ARG + B.IOR + B.IOT/B.IOTI.
        //
        // The current PTO auto-mode bridge does not pass explicit layout/dim
        // metadata yet, so use bring-up defaults here:
        //   LB0/LB1/LB2 = 0, format=0 (Normal), RegSrc1/2=zero, RegDst=zero.
        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0) // RegDst (bring-up: unused)
            .addReg(LinxISA::R0) // RegSrc0: stride bytes (default 0)
            .addReg(Base)        // RegSrc1: base pointer
            .addReg(LinxISA::R0);// RegSrc2: aux/layout source (default 0)

        // Bring-up v0.3 contract: B.IOTI is the canonical descriptor; encode the
        // tile destination register in the first absent source slot (SrcTile1)
        // and set S0V/S1V to indicate no tile inputs.
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(tileKindFromId(DstID)) // DstTile (hand)
            .addImm(0)      // S0R
            .addImm(1)      // S0V (absent)
            .addImm(0)      // S1R
            .addImm(1)      // S1V (absent)
            .addImm(0)      // SrcTile0
            .addImm(DstID)  // SrcTile1 (dst tile reg id)
            .addImm(Size)   // SizeCode (imm5)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TLOAD_ANY: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();

        const unsigned DstID = tileRegId(Dst);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TLOAD);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(LinxISA::R0)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(tileKindFromId(DstID))
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(DstID)
            .addImm(Size)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TLOAD_DESC: {
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Base = PseudoMI->getOperand(1).getReg();
        const int64_t Layout = PseudoMI->getOperand(2).getImm();
        const int64_t LB0 = PseudoMI->getOperand(3).getImm();
        const int64_t LB1 = PseudoMI->getOperand(4).getImm();
        const int64_t LB2 = PseudoMI->getOperand(5).getImm();
        const int64_t Size = PseudoMI->getOperand(6).getImm();

        const unsigned DstID = tileRegId(Dst);
        if (DstID >= 16)
          report_fatal_error("Linx: TMA.TLOAD dst must be in TILE0..TILE15");

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TLOAD);
        checkTMAArg(Layout);
        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, LB2);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Layout);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(LinxISA::R0)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(tileKindFromId(DstID))
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(1)
            .addImm(0)
            .addImm(DstID)
            .addImm(Size)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TSTORE: {
        const Register Base = PseudoMI->getOperand(0).getReg();
        const Register Src = PseudoMI->getOperand(1).getReg();
        const int64_t Size = PseudoMI->getOperand(2).getImm();

        const unsigned SrcID = tileRegId(Src);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TSTORE);

        // Canonical descriptor-carrying TSTORE header:
        //   B.DIM(LB0/LB1/LB2) + B.ARG + B.IOR + B.IOT/B.IOTI.
        emitDim(MBB, InsertPt, /*LoopNest=*/0, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, /*Imm=*/0);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, /*Imm=*/0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG))
            .addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0) // RegDst: valid mask / flags (default 0)
            .addReg(LinxISA::R0) // RegSrc0: stride bytes (default 0)
            .addReg(Base)        // RegSrc1: base pointer
            .addReg(LinxISA::R0);// RegSrc2: aux/layout source (default 0)

        // Store: encode the source tile in SrcTile0 and mark it present (S0V=0).
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(tileKindFromId(SrcID)) // DstTile (hand hint)
            .addImm(0)      // S0R
            .addImm(0)      // S0V (present)
            .addImm(0)      // S1R
            .addImm(1)      // S1V (absent)
            .addImm(SrcID)  // SrcTile0
            .addImm(0)      // SrcTile1 (unused)
            .addImm(Size)   // SizeCode (imm5)
            .addReg(Src, RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_TMA_TSTORE_DESC: {
        const Register Base = PseudoMI->getOperand(0).getReg();
        const Register Src = PseudoMI->getOperand(1).getReg();
        const int64_t Layout = PseudoMI->getOperand(2).getImm();
        const int64_t LB0 = PseudoMI->getOperand(3).getImm();
        const int64_t LB1 = PseudoMI->getOperand(4).getImm();
        const int64_t LB2 = PseudoMI->getOperand(5).getImm();
        const int64_t Size = PseudoMI->getOperand(6).getImm();
        const unsigned SrcID = tileRegId(Src);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_TMA))
            .addImm(DType_I32)
            .addImm(TMA_TSTORE);
        checkTMAArg(Layout);
        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, LB2);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_ARG)).addImm(Layout);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOR))
            .addReg(LinxISA::R0)
            .addReg(LinxISA::R0)
            .addReg(Base)
            .addReg(LinxISA::R0);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(tileKindFromId(SrcID))
            .addImm(0)
            .addImm(0)
            .addImm(0)
            .addImm(1)
            .addImm(SrcID)
            .addImm(0)
            .addImm(Size)
            .addReg(Src, RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_CUBE_MAMULB: {
        // Expand into two blocks:
        //   BSTART.CUBE(MAMULB) + dims + B.IOT(srcA, srcB) -> ACC
        //   BSTART.CUBE(ACCCVT) + B.IOT(dst)              -> tile
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register SrcA = PseudoMI->getOperand(1).getReg();
        const Register SrcB = PseudoMI->getOperand(2).getReg();
        const int64_t M = PseudoMI->getOperand(3).getImm();
        const int64_t N = PseudoMI->getOperand(4).getImm();
        const int64_t K = PseudoMI->getOperand(5).getImm();

        const unsigned DstID = tileRegId(Dst);
        if (DstID < 16)
          report_fatal_error("Linx: CUBE.ACCCVT dst must be in TILE16..TILE31");
        const unsigned Group = (DstID >> 3) & 0x1u;
        const unsigned Depth = DstID & 0x7u;

        const unsigned AID = tileRegId(SrcA);
        const unsigned BID = tileRegId(SrcB);

        // First block: MAMULB
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_MAMULB);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, M);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, N);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, K);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(4)    // DstTile (acc)
            .addImm(0)    // S0R
            .addImm(0)    // S0V (present)
            .addImm(0)    // S1R
            .addImm(0)    // S1V (present)
            .addImm(AID)  // SrcTile0
            .addImm(BID)  // SrcTile1
            .addImm(8)    // SizeCode (bring-up: 4KiB accumulator)
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit);

        // Second block: ACCCVT into dst tile.
        MachineFunction &MF = *MBB.getParent();
        auto *AccBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
        MF.insert(std::next(MBB.getIterator()), AccBB);
        AccBB->transferSuccessorsAndUpdatePHIs(&MBB);
        MBB.addSuccessor(AccBB);

        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_ACCCVT);

        const unsigned DstKind = tileKindFromId(Depth | (Group << 3) | 16u);
        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(DstKind)
            .addImm(0)       // S0R
            .addImm(1)       // S0V (absent)
            .addImm(0)       // S1R
            .addImm(1)       // S1V (absent)
            .addImm(0)       // SrcTile0 (unused)
            .addImm(16u | (Group << 3) | Depth) // SrcTile1 (dst tile reg id)
            .addImm(8)       // SizeCode (bring-up: 4KiB)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_CUBE_MAMULB_ACC: {
        // Expand into two blocks:
        //   BSTART.CUBE(MAMULB.ACC) + dims + B.IOT(srcA, srcB) -> ACC
        //   BSTART.CUBE(ACCCVT)     + B.IOT(dst)              -> tile
        //
        // The explicit ACC operand is preserved as an implicit use so SSA
        // dependencies are maintained (the emulator models the accumulator as
        // implicit state).
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register Acc = PseudoMI->getOperand(1).getReg();
        const Register SrcA = PseudoMI->getOperand(2).getReg();
        const Register SrcB = PseudoMI->getOperand(3).getReg();
        const int64_t M = PseudoMI->getOperand(4).getImm();
        const int64_t N = PseudoMI->getOperand(5).getImm();
        const int64_t K = PseudoMI->getOperand(6).getImm();

        const unsigned DstID = tileRegId(Dst);
        if (DstID < 16)
          report_fatal_error("Linx: CUBE.ACCCVT dst must be in TILE16..TILE31");
        const unsigned Group = (DstID >> 3) & 0x1u;
        const unsigned Depth = DstID & 0x7u;

        const unsigned AID = tileRegId(SrcA);
        const unsigned BID = tileRegId(SrcB);

        // First block: MAMULB.ACC
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_MAMULB_ACC);

        emitDim(MBB, InsertPt, /*LoopNest=*/0, M);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, N);
        emitDim(MBB, InsertPt, /*LoopNest=*/2, K);

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(4)    // DstTile (acc)
            .addImm(0)    // S0R
            .addImm(0)    // S0V (present)
            .addImm(0)    // S1R
            .addImm(0)    // S1V (present)
            .addImm(AID)  // SrcTile0
            .addImm(BID)  // SrcTile1
            .addImm(8)    // SizeCode (bring-up: 4KiB accumulator)
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit)
            .addReg(Acc, RegState::Implicit);

        // Second block: ACCCVT into dst tile.
        MachineFunction &MF = *MBB.getParent();
        auto *AccBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
        MF.insert(std::next(MBB.getIterator()), AccBB);
        AccBB->transferSuccessorsAndUpdatePHIs(&MBB);
        MBB.addSuccessor(AccBB);

        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::BSTART_CUBE))
            .addImm(DType_I32)
            .addImm(CUBE_ACCCVT);

        const unsigned DstKind = tileKindFromId(Depth | (Group << 3) | 16u);
        BuildMI(*AccBB, AccBB->end(), DL, TII.get(LinxISA::B_IOTI_G1))
            .addImm(DstKind)
            .addImm(0)       // S0R
            .addImm(1)       // S0V (absent)
            .addImm(0)       // S1R
            .addImm(1)       // S1V (absent)
            .addImm(0)       // SrcTile0 (unused)
            .addImm(16u | (Group << 3) | Depth) // SrcTile1 (dst tile reg id)
            .addImm(8)       // SizeCode (bring-up: 4KiB)
            .addReg(Dst, RegState::Define | RegState::Implicit);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_VPAR_TADD:
      case LinxISA::PSEUDO_VPAR_TSUB:
      case LinxISA::PSEUDO_VTILE_ADD:
      case LinxISA::PSEUDO_VTILE_SUB: {
        // Expand into a VPAR decoupled header that binds:
        // - input tiles through TA/TB (first B.IOTI)
        // - output tile through TO (second B.IOTI or B.IOT for in-place)
        //
        // The out-of-line body is a single-lane snippet that executes:
        //   load TA, load TB, add/sub, store TO
        // and terminates at C.BSTOP so QEMU can replay it across LB0/LB1.
        const Register Dst = PseudoMI->getOperand(0).getReg();
        const Register SrcA = PseudoMI->getOperand(1).getReg();
        const Register SrcB = PseudoMI->getOperand(2).getReg();
        const bool IsAdd =
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TADD) ||
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VTILE_ADD);
        const int64_t Size =
            (PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TADD ||
             PseudoMI->getOpcode() == LinxISA::PSEUDO_VPAR_TSUB)
                ? PseudoMI->getOperand(3).getImm()
                : 8; // 4KiB tiles (SizeCode=8)

        const unsigned DstID = tileRegId(Dst);
        const unsigned AID = tileRegId(SrcA);
        const unsigned BID = tileRegId(SrcB);

        // Derive a compact 2-D iteration space for the tile:
        // - LB0=64 elements (256B row stride => lc1<<8)
        // - LB1=bytes/256
        uint64_t Bytes = 0;
        if (Size >= 0 && Size < 60) {
          Bytes = 1ull << (static_cast<unsigned>(Size) + 4u);
        }
        if (Bytes == 0 || (Bytes & 3u) != 0 || Bytes > 4096u ||
            (Bytes % 256u) != 0) {
          report_fatal_error("Linx: VPAR tile binop requires 256B-aligned tile size <=4KB");
        }
        const int64_t LB0 = 64;
        const int64_t LB1 = static_cast<int64_t>(Bytes / 256u);

        MCSymbol *BodySym = IsAdd ? getOrCreateVTileAddBodySym()
                                  : getOrCreateVTileSubBodySym();

        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTART_VPAR)).addImm(0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_TEXT)).addSym(BodySym);

        // Descriptor 0: inputs (TA/TB), group=0.
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G0))
            .addImm(tileKindFromId(DstID)) // DstTile (hand hint)
            .addImm(0)                     // S0R
            .addImm(0)                     // S0V (present)
            .addImm(0)                     // S1R
            .addImm(0)                     // S1V (present)
            .addImm(AID)                   // SrcTile0 (TA)
            .addImm(BID)                   // SrcTile1 (TB)
            .addImm(Size)                  // SizeCode
            .addReg(SrcA, RegState::Implicit)
            .addReg(SrcB, RegState::Implicit);

        // Descriptor 1: output (TO), group=1 (last). If the output register
        // aliases an input tile, avoid B.IOTI allocation/clear by using B.IOT.
        const bool InPlace = (DstID == AID) || (DstID == BID);
        if (InPlace) {
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOT_G1))
              .addImm(tileKindFromId(DstID)) // DstTile (hand hint)
              .addReg(LinxISA::R0)           // RegSrc (unused)
              .addImm(0)                     // S0R
              .addImm(1)                     // S0V (absent)
              .addImm(0)                     // S1R
              .addImm(1)                     // S1V (absent)
              .addImm(0)                     // SrcTile0 (unused)
              .addImm(DstID)                 // SrcTile1 (dst tile id)
              .addReg(Dst, RegState::Define | RegState::Implicit);
        } else {
          BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_IOTI_G1))
              .addImm(tileKindFromId(DstID)) // DstTile (hand hint)
              .addImm(0)                     // S0R
              .addImm(1)                     // S0V (absent)
              .addImm(0)                     // S1R
              .addImm(1)                     // S1V (absent)
              .addImm(0)                     // SrcTile0 (unused)
              .addImm(DstID)                 // SrcTile1 (dst tile id)
              .addImm(Size)                  // SizeCode
              .addReg(Dst, RegState::Define | RegState::Implicit);
        }

        emitDim(MBB, InsertPt, /*LoopNest=*/0, LB0);
        emitDim(MBB, InsertPt, /*LoopNest=*/1, LB1);

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      case LinxISA::PSEUDO_VBLOCK_LAUNCH: {
        // Expand into a decoupled vector block header:
        //   BSTART.VSEQ/VPAR + B.TEXT empty_body + B.DIM(LB0..2)
        const int64_t VKind = PseudoMI->getOperand(0).getImm();
        const int64_t Dim0 = PseudoMI->getOperand(1).getImm();
        const int64_t Dim1 = PseudoMI->getOperand(2).getImm();
        const int64_t Dim2 = PseudoMI->getOperand(3).getImm();
        const int64_t AttrBits = PseudoMI->getOperand(4).getImm();
        (void)AttrBits; // Bring-up: B.ATTR is not wired in the backend yet.

        const unsigned Mode = 0; // bring-up default
        const unsigned BStartOpc = (VKind == 0)   ? LinxISA::BSTART_VSEQ
                                 : (VKind == 1) ? LinxISA::BSTART_VPAR
                                                : 0;
        if (!BStartOpc)
          report_fatal_error("Linx: vblock.launch vkind must be 0(VSEQ) or 1(VPAR)");

        BuildMI(MBB, InsertPt, DL, TII.get(BStartOpc)).addImm(Mode);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_TEXT))
            .addSym(getOrCreateEmptyBodySym());
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_DIM_LB0))
            .addReg(LinxISA::R0)
            .addImm(Dim0);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_DIM_LB1))
            .addReg(LinxISA::R0)
            .addImm(Dim1);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::B_DIM_LB2))
            .addReg(LinxISA::R0)
            .addImm(Dim2);
        BuildMI(MBB, InsertPt, DL, TII.get(LinxISA::BSTOP));

        PseudoMI->eraseFromParent();
        Changed = true;
        break;
      }

      default:
        break;
      }
    }

    // Discover physical registers that are constant at each MBB entry.
    //
    // This is used by late peepholes to recognize patterns like:
    //   sll tmp, shreg; srl tmp, shreg   (where shreg == 32)
    // even when the shift amount was materialized in a predecessor block.
    //
    // We keep this conservative: only track constants defined by a small set of
    // immediate materializations, and drop any register that is overwritten by
    // an unrecognized def.
    SmallVector<DenseMap<Register, int64_t>, 0> EntryConstRegs;

    auto tryGetConstDef = [&](const MachineInstr &MI) -> std::optional<int64_t> {
      auto isFromZero = [&](unsigned OpNo) -> bool {
        if (OpNo >= MI.getNumOperands())
          return false;
        const MachineOperand &MO = MI.getOperand(OpNo);
        return MO.isReg() && MO.getReg() == LinxISA::R0;
      };

      switch (MI.getOpcode()) {
      case LinxISA::ADDIri:
      case LinxISA::ADDIWri:
        if (!isFromZero(/*OpNo=*/1) || MI.getNumOperands() < 3 ||
            !MI.getOperand(2).isImm())
          return std::nullopt;
        return MI.getOperand(2).getImm();
      case LinxISA::SUBIri:
      case LinxISA::SUBIWri:
        if (!isFromZero(/*OpNo=*/1) || MI.getNumOperands() < 3 ||
            !MI.getOperand(2).isImm())
          return std::nullopt;
        return -MI.getOperand(2).getImm();
      case LinxISA::LUI:
        if (MI.getNumOperands() < 2 || !MI.getOperand(1).isImm())
          return std::nullopt;
        return MI.getOperand(1).getImm() << 12;
      default:
        return std::nullopt;
      }
    };

    unsigned MaxMBBNumber = 0;
    for (const MachineBasicBlock &MBB : MF)
      MaxMBBNumber = std::max(MaxMBBNumber, unsigned(MBB.getNumber()));
    EntryConstRegs.resize(MaxMBBNumber + 1);
    SmallVector<DenseMap<Register, int64_t>, 0> ExitConstRegs(MaxMBBNumber + 1);
    SmallVector<bool, 0> InWorklist(MaxMBBNumber + 1, false);

    auto meetPreds = [&](const MachineBasicBlock &MBB,
                         DenseMap<Register, int64_t> &Out) -> void {
      Out.clear();
      if (MBB.pred_empty())
        return;

      // Start from the first predecessor's exit constants.
      const MachineBasicBlock *FirstPred = *MBB.pred_begin();
      Out = ExitConstRegs[FirstPred->getNumber()];

      // Intersect with remaining preds.
      for (auto PI = std::next(MBB.pred_begin()), PE = MBB.pred_end(); PI != PE;
           ++PI) {
        const DenseMap<Register, int64_t> &POut = ExitConstRegs[(*PI)->getNumber()];
        SmallVector<Register, 16> ToErase;
        for (auto &KV : Out) {
          auto It = POut.find(KV.first);
          if (It == POut.end() || It->second != KV.second)
            ToErase.push_back(KV.first);
        }
        for (Register R : ToErase)
          Out.erase(R);
      }
    };

	    auto transferBlock = [&](const MachineBasicBlock &MBB,
	                             const DenseMap<Register, int64_t> &In,
	                             DenseMap<Register, int64_t> &Out) -> void {
	      Out = In;
	      for (const MachineInstr &MI : MBB) {
	        if (MI.isDebugInstr() || MI.isCFIInstruction() || isMarkerInstr(MI))
	          continue;
	        for (const MachineOperand &MO : MI.operands()) {
	          if (!MO.isReg() || !MO.isDef() || MO.isImplicit())
	            continue;
	          Register Reg = MO.getReg();
	          if (!Reg.isPhysical())
	            continue;
	          if (Reserved.test(Reg))
	            continue;
	          if (auto V = tryGetConstDef(MI)) {
	            Out[Reg] = *V;
	            continue;
	          }

	          auto tryGetCopyConst = [&](const MachineInstr &MI) -> std::optional<int64_t> {
	            const unsigned Opc = MI.getOpcode();
	            const bool IsAddCopy = (Opc == LinxISA::ADDrr || Opc == LinxISA::ADDWrr);
	            if (!IsAddCopy)
	              return std::nullopt;
	            if (MI.getNumOperands() < 3 || !MI.getOperand(1).isReg() ||
	                !MI.getOperand(2).isReg())
	              return std::nullopt;
	            const Register A = MI.getOperand(1).getReg();
	            const Register B = MI.getOperand(2).getReg();
	            if (A == LinxISA::R0) {
	              auto It = Out.find(B);
	              if (It != Out.end())
	                return It->second;
	            }
	            if (B == LinxISA::R0) {
	              auto It = Out.find(A);
	              if (It != Out.end())
	                return It->second;
	            }
	            return std::nullopt;
	          };

	          if (auto V = tryGetCopyConst(MI)) {
	            Out[Reg] = *V;
	            continue;
	          }

	          Out.erase(Reg);
	        }
	      }
	    };

    // Worklist solver.
    SmallVector<const MachineBasicBlock *, 64> Worklist;
    for (const MachineBasicBlock &MBB : MF) {
      Worklist.push_back(&MBB);
      InWorklist[MBB.getNumber()] = true;
    }

    while (!Worklist.empty()) {
      const MachineBasicBlock *MBB = Worklist.pop_back_val();
      InWorklist[MBB->getNumber()] = false;

      DenseMap<Register, int64_t> NewIn;
      meetPreds(*MBB, NewIn);

      if (NewIn != EntryConstRegs[MBB->getNumber()]) {
        EntryConstRegs[MBB->getNumber()] = NewIn;
      }

      DenseMap<Register, int64_t> NewOut;
      transferBlock(*MBB, EntryConstRegs[MBB->getNumber()], NewOut);
      if (NewOut != ExitConstRegs[MBB->getNumber()]) {
        ExitConstRegs[MBB->getNumber()] = NewOut;
        for (const MachineBasicBlock *Succ : MBB->successors()) {
          unsigned N = Succ->getNumber();
          if (!InWorklist[N]) {
            Worklist.push_back(Succ);
            InWorklist[N] = true;
          }
        }
      }
    }

    auto findSetcInsertPt = [&](MachineBasicBlock &MBB, MachineInstr &Anchor,
                                Register LHS, Register RHS)
        -> MachineBasicBlock::iterator {
      MachineInstr *InsertAfter = nullptr;
      for (MachineInstr &MI : MBB) {
        if (&MI == &Anchor)
          break;
        if (MI.isDebugInstr() || isMarkerInstr(MI))
          continue;
        if ((LHS && MI.definesRegister(LHS, &TRI)) ||
            (RHS && MI.definesRegister(RHS, &TRI))) {
          InsertAfter = &MI;
        }
      }

      if (InsertAfter)
        return std::next(InsertAfter->getIterator());
      return Anchor.getIterator();
    };

    for (MachineBasicBlock &MBB : MF) {
      enum class ExitKind {
        Fall,
        Direct,
        Cond,
        Call,
        Ret,
        Ind,
        ICall,
      };

      // Decoupled out-of-line bodies are linear snippets referenced via B.TEXT
      // and must not be wrapped or rewritten by Blockify.
      if (DecoupledBodyBBs.contains(&MBB))
        continue;

      // Frame prologue/epilogue macros (FENTRY/FEXIT/FRET.*) are standalone
      // blocks in LinxISA: they already contain the required block markers and
      // micro-ops for stack/register management. Do not surround them with
      // BSTART/BSTOP or attempt to rewrite their control-flow.
		      if (isStandaloneFrameMacroBlock(MBB)) {
		        // If the pass runs twice, strip any stale explicit markers.
		        for (auto It = MBB.begin(); It != MBB.end();) {
		          if (isMarkerInstr(*It)) {
		            It = MBB.erase(It);
		            Changed = true;
		            continue;
		          }
		          ++It;
		        }
		        continue;
		      }

      // Tile blocks (TAU) have their own BSTART.TMA/BSTART.CUBE headers and
      // must not be wrapped by standard BSTART.STD or have T/U-hand queue
      // remapping applied.
      auto isStdBStartOpcode = [&](unsigned Opc) -> bool {
        switch (Opc) {
        case LinxISA::CBSTART_STD:
        case LinxISA::BSTART_STD_FALL:
        case LinxISA::BSTART_STD_DIRECT:
        case LinxISA::BSTART_STD_COND:
        case LinxISA::BSTART_STD_CALL:
        case LinxISA::BSTART_STD_IND:
        case LinxISA::BSTART_STD_ICALL:
        case LinxISA::BSTART_STD_RET:
          return true;
        default:
          return false;
        }
      };

      bool IsTileBlock = false;
      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || MI.isCFIInstruction() || MI.isPHI())
          continue;
        // Be robust if the pass runs twice: skip any stale standard markers
        // and detect the tile header that follows.
        if (isStdBStartOpcode(MI.getOpcode()))
          continue;
        IsTileBlock = isTileBlockStartInstr(MI);
        break;
      }

			      auto isPhysRegLiveOutOfBlock = [&](Register Reg) -> bool {
		        // Physical register live-in sets only track "use before def" within a
		        // block, so a reg that is merely live-through a successor (not used
	        // until later) may not appear as live-in to an immediate successor.
	        // Conservatively walk successors and detect any reachable use of Reg
	        // before it is redefined.
	        SmallVector<const MachineBasicBlock *, 8> Worklist;
	        SmallPtrSet<const MachineBasicBlock *, 16> Visited;

	        for (const MachineBasicBlock *Succ : MBB.successors()) {
	          if (Succ)
	            Worklist.push_back(Succ);
	        }

	        while (!Worklist.empty()) {
	          const MachineBasicBlock *Succ = Worklist.pop_back_val();
	          if (!Visited.insert(Succ).second)
	            continue;

	          // If the successor explicitly records Reg as live-in, we are done.
	          if (Succ->isLiveIn(Reg))
	            return true;

	          bool DefinedInSucc = false;
	          for (const MachineInstr &MI : *Succ) {
	            if (MI.isDebugInstr() || MI.isCFIInstruction() ||
	                isMarkerInstr(MI))
	              continue;

	            if (MI.readsRegister(Reg, &TRI))
	              return true;

	            if (MI.definesRegister(Reg, &TRI)) {
	              DefinedInSucc = true;
	              break;
	            }
	          }

	          // No read and no def: Reg is live-through this successor, so keep
	          // searching down the CFG.
	          if (!DefinedInSucc) {
	            for (const MachineBasicBlock *Succ2 : Succ->successors()) {
	              if (Succ2)
	                Worklist.push_back(Succ2);
	            }
	          }
	        }

		        return false;
		      };

	      auto hasSingleNonDbgUseInMBB =
	          [&](Register Reg, const MachineInstr *UserMI,
	              const MachineInstr *IgnoreMI) -> bool {
	        unsigned Count = 0;
	        for (const MachineInstr &MI : MBB) {
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (&MI == IgnoreMI)
	            continue;
	          for (const MachineOperand &MO : MI.operands()) {
	            if (!MO.isReg() || MO.isImplicit() || MO.isDef())
	              continue;
	            if (MO.getReg() != Reg)
	              continue;
	            ++Count;
	            if (&MI != UserMI || Count > 1)
	              return false;
	          }
	        }
	        return Count == 1;
	      };

	      auto getPhysRegConstAtMBBEntry = [&](Register Reg) -> std::optional<int64_t> {
	        if (!Reg || !Reg.isPhysical())
	          return std::nullopt;
	        auto &Map = EntryConstRegs[MBB.getNumber()];
	        auto It = Map.find(Reg);
	        if (It == Map.end())
	          return std::nullopt;
	        return It->second;
	      };

	      // Match a 32-bit zero-extend implemented as a shift pair:
	      //   tmp1 = sll x, shamt
	      //   tmp2 = srl tmp1, shamt
	      // with shamt == 32 (constant). Returns the original (pre-zext) source.
	      auto matchZextWByShiftPair =
	          [&](MachineInstr &UseMI, Register ZextReg, Register &OrigSrc,
	              MachineInstr *&SllMIOut, MachineInstr *&SrlMIOut) -> bool {
	        OrigSrc = Register();
	        SllMIOut = nullptr;
	        SrlMIOut = nullptr;
	        if (!ZextReg)
	          return false;

	        // Find defining SRLrr in this block.
	        MachineInstr *SrlMI = nullptr;
	        for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	          --It;
	          MachineInstr &MI = *It;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (!MI.definesRegister(ZextReg, &TRI))
	            continue;
	          if (MI.getOpcode() != LinxISA::SRLrr || MI.getNumOperands() < 3 ||
	              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
	            return false;
	          SrlMI = &MI;
	          break;
	        }
	        if (!SrlMI)
	          return false;

	        const Register Tmp1 = SrlMI->getOperand(1).getReg();
	        const Register ShAmtReg = SrlMI->getOperand(2).getReg();
	        if (!Tmp1 || !ShAmtReg)
	          return false;

	        // Resolve constant shift amount (local def in block; else function-constant).
	        auto getConstShiftAmt = [&](MachineInstr &Anchor) -> std::optional<int64_t> {
	          for (auto DI = Anchor.getIterator(); DI != MBB.begin();) {
	            --DI;
	            MachineInstr &DefMI = *DI;
	            if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
	              continue;
	            if (!DefMI.definesRegister(ShAmtReg, &TRI))
	              continue;
	            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
	                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
	                !DefMI.getOperand(2).isImm())
	              break;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              return DefMI.getOperand(2).getImm();
	            default:
	              break;
	            }
	            break;
	          }
	          if (ShAmtReg.isPhysical())
	            return getPhysRegConstAtMBBEntry(ShAmtReg);
	          return std::nullopt;
	        };

	        auto ShAmtC = getConstShiftAmt(*SrlMI);
	        if (!ShAmtC || *ShAmtC != 32)
	          return false;

	        // Find defining SLLrr of Tmp1.
	        MachineInstr *SllMI = nullptr;
	        for (auto It = SrlMI->getIterator(); It != MBB.begin();) {
	          --It;
	          MachineInstr &MI = *It;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (!MI.definesRegister(Tmp1, &TRI))
	            continue;
	          if (MI.getOpcode() != LinxISA::SLLrr || MI.getNumOperands() < 3 ||
	              !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
	            return false;
	          if (MI.getOperand(2).getReg() != ShAmtReg)
	            return false;
	          SllMI = &MI;
	          break;
	        }
	        if (!SllMI)
	          return false;

	        const Register Src = SllMI->getOperand(1).getReg();
	        if (!Src)
	          return false;

	        if (!hasSingleNonDbgUseInMBB(Tmp1, SrlMI, SllMI))
	          return false;
	        if (!hasSingleNonDbgUseInMBB(ZextReg, &UseMI, SrlMI))
	          return false;
	        if ((Tmp1.isPhysical() && isPhysRegLiveOutOfBlock(Tmp1)) ||
	            (ZextReg.isPhysical() && isPhysRegLiveOutOfBlock(ZextReg)))
	          return false;

	        OrigSrc = Src;
	        SllMIOut = SllMI;
	        SrlMIOut = SrlMI;
	        return true;
	      };

	      // Pre-blockify peepholes (run before inserting block markers and T/U
	      // remapping).
	      //
	      // Fold `and/or` feeding a nonzero compare into CMP.AND/CMP.OR:
	      //   tmp = and/or x, y
	      //   tmp2 = addw tmp, zero        (optional)
	      //   dst = cmp.nei tmp2, 0
	      // =>
	      //   dst = cmp.and/or x, y
	      //
		      // and similarly for immediate ANDI/ORI.
		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &LogicMI = *It;
	        if (LogicMI.isDebugInstr() || isMarkerInstr(LogicMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned LogicOpc = LogicMI.getOpcode();
	        const bool IsAnd =
	            (LogicOpc == LinxISA::ANDrr || LogicOpc == LinxISA::ANDWrr ||
	             LogicOpc == LinxISA::ANDIri || LogicOpc == LinxISA::ANDIWri ||
	             LogicOpc == LinxISA::HLANDIri || LogicOpc == LinxISA::HLANDIWri);
	        const bool IsOr =
	            (LogicOpc == LinxISA::ORrr || LogicOpc == LinxISA::ORWrr ||
	             LogicOpc == LinxISA::ORIri || LogicOpc == LinxISA::ORIWri ||
	             LogicOpc == LinxISA::HLORIri || LogicOpc == LinxISA::HLORIWri);
	        if (!IsAnd && !IsOr) {
	          ++It;
	          continue;
	        }
	        if (LogicMI.getNumOperands() < 3 || !LogicMI.getOperand(0).isReg() ||
	            !LogicMI.getOperand(0).isDef()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = LogicMI.getOperand(0).getReg();
	        if (!Tmp || !Tmp.isPhysical()) {
	          ++It;
	          continue;
	        }

	        auto nextNonMarker = [&](MachineBasicBlock::iterator Pos)
	            -> MachineBasicBlock::iterator {
	          auto NI = Pos;
	          while (NI != MBB.end() && (NI->isDebugInstr() || isMarkerInstr(*NI)))
	            ++NI;
	          return NI;
	        };

	        MachineInstr *CopyMI = nullptr;
	        Register CmpSrc = Tmp;
	        bool InPlaceCopy = false;

	        auto NI = nextNonMarker(std::next(It));
	        if (NI == MBB.end()) {
	          ++It;
	          continue;
	        }

	        if (NI->getOpcode() == LinxISA::ADDWrr && NI->getNumOperands() >= 3 &&
	            NI->getOperand(0).isReg() && NI->getOperand(0).isDef() &&
	            NI->getOperand(1).isReg() && NI->getOperand(2).isReg()) {
	          const Register CDst = NI->getOperand(0).getReg();
	          const Register A = NI->getOperand(1).getReg();
	          const Register B = NI->getOperand(2).getReg();
	          if (CDst && CDst.isPhysical() &&
	              ((A == Tmp && B == LinxISA::R0) ||
	               (B == Tmp && A == LinxISA::R0))) {
	            CopyMI = &*NI;
	            InPlaceCopy = (CDst == Tmp);
	            if (!InPlaceCopy)
	              CmpSrc = CDst;
	            NI = nextNonMarker(std::next(NI));
	            if (NI == MBB.end()) {
	              ++It;
	              continue;
	            }
	          }
	        }

	        MachineInstr &CmpMI = *NI;
	        if (CmpMI.getOpcode() != LinxISA::CMPNEI || CmpMI.getNumOperands() < 3 ||
	            !CmpMI.getOperand(1).isReg() || CmpMI.getOperand(1).getReg() != CmpSrc ||
	            !CmpMI.getOperand(2).isImm() || CmpMI.getOperand(2).getImm() != 0) {
	          ++It;
	          continue;
	        }

	        auto onlyUsedBy = [&](Register Reg, const MachineInstr *MI1,
	                              const MachineInstr *MI2,
	                              const MachineInstr *IgnoreMI) -> bool {
	          for (const MachineInstr &MI : MBB) {
	            if (MI.isDebugInstr() || isMarkerInstr(MI))
	              continue;
	            if (&MI == IgnoreMI)
	              continue;
	            for (const MachineOperand &MO : MI.operands()) {
	              if (!MO.isReg() || MO.isImplicit() || MO.isDef())
	                continue;
	              if (MO.getReg() != Reg)
	                continue;
	              if (&MI != MI1 && &MI != MI2)
	                return false;
	            }
	          }
	          return true;
	        };

	        if (CopyMI && InPlaceCopy) {
	          // tmp is used by both the in-place ADDW and the compare.
	          if (!onlyUsedBy(Tmp, CopyMI, &CmpMI, &LogicMI)) {
	            ++It;
	            continue;
	          }
	        } else {
	          if (!hasSingleNonDbgUseInMBB(CmpSrc, &CmpMI,
	                                       CopyMI ? CopyMI : &LogicMI)) {
	            ++It;
	            continue;
	          }
	          if (!hasSingleNonDbgUseInMBB(Tmp, CopyMI ? CopyMI : &CmpMI, &LogicMI)) {
	            ++It;
	            continue;
	          }
	        }

	        const Register Dst = CmpMI.getOperand(0).getReg();
	        unsigned NewOpc = 0;
	        if (LogicOpc == LinxISA::ANDrr || LogicOpc == LinxISA::ANDWrr)
	          NewOpc = LinxISA::CMPAND;
	        else if (LogicOpc == LinxISA::ORrr || LogicOpc == LinxISA::ORWrr)
	          NewOpc = LinxISA::CMPOR;
	        else if (LogicOpc == LinxISA::ANDIri || LogicOpc == LinxISA::ANDIWri)
	          NewOpc = LinxISA::CMPANDI;
	        else if (LogicOpc == LinxISA::ORIri || LogicOpc == LinxISA::ORIWri)
	          NewOpc = LinxISA::CMPORI;
	        else if (LogicOpc == LinxISA::HLANDIri || LogicOpc == LinxISA::HLANDIWri)
	          NewOpc = LinxISA::HLCMPANDI;
	        else if (LogicOpc == LinxISA::HLORIri || LogicOpc == LinxISA::HLORIWri)
	          NewOpc = LinxISA::HLCMPORI;
	        else {
	          ++It;
	          continue;
	        }

	        if (NewOpc == LinxISA::CMPAND || NewOpc == LinxISA::CMPOR) {
	          BuildMI(MBB, CmpMI.getIterator(), CmpMI.getDebugLoc(), TII.get(NewOpc), Dst)
	              .addReg(LogicMI.getOperand(1).getReg())
	              .addReg(LogicMI.getOperand(2).getReg());
	        } else {
	          if (!LogicMI.getOperand(2).isImm()) {
	            ++It;
	            continue;
	          }
	          BuildMI(MBB, CmpMI.getIterator(), CmpMI.getDebugLoc(), TII.get(NewOpc), Dst)
	              .addReg(LogicMI.getOperand(1).getReg())
	              .addImm(LogicMI.getOperand(2).getImm());
	        }

	        CmpMI.eraseFromParent();
	        if (CopyMI)
	          CopyMI->eraseFromParent();
	        LogicMI.eraseFromParent();
		        Changed = true;
		        It = MBB.begin();
		      }

		      // Pre-blockify peephole: fold a local `sext.w` legalization feeding a
		      // compressed commit condition into a single 32-bit SETC with a SrcR
		      // modifier. This reduces dynamic instruction count while keeping code
		      // size roughly flat (16b+16b -> 32b).
		      //
		      // Pattern (common in Linux):
		      //   tmp = addw src, zero    ; sext.w(src)
		      //   c.setc.{eq,ne} tmp, zero
		      // =>
		      //   setc.{eq,ne} zero, src.sw
		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &SetcMI = *It;
		        if (SetcMI.isDebugInstr() || isMarkerInstr(SetcMI)) {
		          ++It;
		          continue;
		        }
		        const unsigned Opc = SetcMI.getOpcode();
		        if ((Opc != LinxISA::CSETC_EQ && Opc != LinxISA::CSETC_NE) ||
		            SetcMI.getNumOperands() < 2 || !SetcMI.getOperand(0).isReg() ||
		            !SetcMI.getOperand(1).isReg()) {
		          ++It;
		          continue;
		        }

		        const Register A = SetcMI.getOperand(0).getReg();
		        const Register B = SetcMI.getOperand(1).getReg();
		        const bool AIsZero = (A == LinxISA::R0);
		        const bool BIsZero = (B == LinxISA::R0);
		        if (AIsZero == BIsZero) { // require exactly one side is zero
		          ++It;
		          continue;
		        }

		        const Register SextReg = AIsZero ? B : A;

		        auto matchSextWByAddwZero =
		            [&](Register Reg, Register &OrigSrc,
		                MachineInstr *&DefMIOut) -> bool {
		          OrigSrc = Register();
		          DefMIOut = nullptr;
		          if (!Reg)
		            return false;

		          // Find defining ADDWrr in this block.
		          MachineInstr *DefMI = nullptr;
		          for (auto DI = SetcMI.getIterator(); DI != MBB.begin();) {
		            --DI;
		            MachineInstr &MI = *DI;
		            if (MI.isDebugInstr() || isMarkerInstr(MI))
		              continue;
		            if (!MI.definesRegister(Reg, &TRI))
		              continue;
		            DefMI = &MI;
		            break;
		          }
		          if (!DefMI)
		            return false;
		          if (DefMI->getOpcode() != LinxISA::ADDWrr ||
		              DefMI->getNumOperands() < 3 || !DefMI->getOperand(1).isReg() ||
		              !DefMI->getOperand(2).isReg())
		            return false;

		          const Register X = DefMI->getOperand(1).getReg();
		          const Register Y = DefMI->getOperand(2).getReg();
		          if (X == LinxISA::R0 && Y != LinxISA::R0)
		            OrigSrc = Y;
		          else if (Y == LinxISA::R0 && X != LinxISA::R0)
		            OrigSrc = X;
		          else
		            return false;

		          if (!hasSingleNonDbgUseInMBB(Reg, &SetcMI, DefMI))
		            return false;
		          if (Reg.isPhysical() && isPhysRegLiveOutOfBlock(Reg))
		            return false;

		          DefMIOut = DefMI;
		          return true;
		        };

		        Register OrigSrc = Register();
		        MachineInstr *AddwMI = nullptr;
		        if (!matchSextWByAddwZero(SextReg, OrigSrc, AddwMI)) {
		          ++It;
		          continue;
		        }

		        const unsigned NewSetcOpc =
		            (Opc == LinxISA::CSETC_EQ) ? LinxISA::SETC_EQ : LinxISA::SETC_NE;

		        MachineInstr *NewMI =
		            BuildMI(MBB, It, SetcMI.getDebugLoc(), TII.get(NewSetcOpc))
		                .addReg(LinxISA::R0)
		                .addReg(OrigSrc)
		                .getInstr();
		        NewMI->getOperand(1).setTargetFlags(
		            NewMI->getOperand(1).getTargetFlags() | LinxII::MO_SRCR_SW);

		        auto NextIt = std::next(It);
		        SetcMI.eraseFromParent();
		        AddwMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      ExitKind Kind = ExitKind::Fall;
		      MachineBasicBlock *TargetBB = nullptr;   // DIRECT/COND
		      MachineBasicBlock *ReturnBB = nullptr;   // CALL (return target)
	      std::optional<MachineOperand> CallTargetOp; // CALL (callee)
		      std::optional<Register> HeaderSetcTgtReg;   // inserted immediately after BSTART
		      std::optional<Register> ICallSetcTgtReg;    // inserted after callee is computed (ICALL)

      // Identify the last two non-debug, non-marker instructions.
      MachineInstr *Last = nullptr;
      MachineInstr *Prev = nullptr;
      for (auto It = MBB.rbegin(), E = MBB.rend(); It != E; ++It) {
        if (It->isDebugInstr() || isMarkerInstr(*It))
          continue;
        if (!Last) {
          Last = &*It;
          continue;
        }
        Prev = &*It;
        break;
      }

      // Recognize exit shape from the end of the block.
      if (Last) {
        switch (Last->getOpcode()) {
	        case LinxISA::PSEUDO_CALL: {
	          CallTargetOp = Last->getOperand(0);
	          if (CallTargetOp->isReg()) {
	            Kind = ExitKind::ICall;
	            ICallSetcTgtReg = CallTargetOp->getReg();
	          } else {
	            Kind = ExitKind::Call;
	          }
          /*
           * Only emit SETRET (and thus a concrete return target) when the call
           * has a real CFG successor representing the continuation block.
           *
           * For noreturn calls, LLVM can leave the block with no successors;
           * in that case, fabricating a "next block" return target produces
           * invalid/unemitted labels (e.g. when the next block is an internal
           * EH_LABEL-only decoupled body stub).
           */
          if (!MBB.succ_empty())
            ReturnBB = *MBB.succ_begin();
          if (EmptyBodyBB && ReturnBB == EmptyBodyBB)
            ReturnBB = nullptr;
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::PSEUDO_RET: {
          Kind = ExitKind::Ret;
          // Return target is always `ra`; place SETC.TGT right after the BSTART
          // marker for readability.
          HeaderSetcTgtReg = LinxISA::R10;
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::JR: {
          const Register Reg = Last->getOperand(0).getReg();
          Kind = (Reg == LinxISA::R10) ? ExitKind::Ret : ExitKind::Ind;
          if (Reg == LinxISA::R10) {
            HeaderSetcTgtReg = Reg;
          } else {
            BuildMI(MBB, Last->getIterator(), DebugLoc(),
                    TII.get(LinxISA::CSETC_TGT))
                .addReg(Reg);
          }
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::JUMP: {
          // Common lowering shape: `Bcc ...; JUMP ...` (no fallthrough).
          // In BlockISA we must pick a fallthrough block (the physically next
          // block) and encode the other successor in the BSTART header.
          if (Prev && (Prev->getOpcode() == LinxISA::BEQ ||
                       Prev->getOpcode() == LinxISA::BNE ||
                       Prev->getOpcode() == LinxISA::BLT ||
                       Prev->getOpcode() == LinxISA::BGE ||
                       Prev->getOpcode() == LinxISA::BLTU ||
                       Prev->getOpcode() == LinxISA::BGEU)) {
            MachineBasicBlock *BrTargetBB = Prev->getOperand(2).getMBB();
            MachineBasicBlock *JumpTargetBB = Last->getOperand(0).getMBB();
            MachineBasicBlock *FallthroughBB = MBB.getNextNode();
            auto makeTrampoline = [&](MachineBasicBlock *Target) -> MachineBasicBlock * {
              MachineFunction &MF = *MBB.getParent();
              auto *TrampBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
              MF.insert(std::next(MBB.getIterator()), TrampBB);
              TrampBB->addSuccessor(Target);
              BuildMI(*TrampBB, TrampBB->end(), DebugLoc(), TII.get(LinxISA::JUMP))
                  .addMBB(Target);
              return TrampBB;
            };

            unsigned SetcOpc = 0;
            Register LHSReg = Prev->getOperand(0).getReg();
            Register RHSReg = Prev->getOperand(1).getReg();
            auto pickSetc = [&](unsigned BrOpc) -> unsigned {
              switch (BrOpc) {
              case LinxISA::BEQ:
                return LinxISA::CSETC_EQ;
              case LinxISA::BNE:
                return LinxISA::CSETC_NE;
              case LinxISA::BLT:
                return LinxISA::SETC_LT;
              case LinxISA::BGE:
                return LinxISA::SETC_GE;
              case LinxISA::BLTU:
                return LinxISA::SETC_LTU;
              case LinxISA::BGEU:
                return LinxISA::SETC_GEU;
              default:
                llvm_unreachable("Unexpected branch opcode");
              }
            };
	            auto invertBranch = [&](unsigned BrOpc) -> unsigned {
	              switch (BrOpc) {
              case LinxISA::BEQ:
                return LinxISA::BNE;
              case LinxISA::BNE:
                return LinxISA::BEQ;
              case LinxISA::BLT:
                return LinxISA::BGE;
              case LinxISA::BGE:
                return LinxISA::BLT;
              case LinxISA::BLTU:
                return LinxISA::BGEU;
              case LinxISA::BGEU:
                return LinxISA::BLTU;
              default:
                llvm_unreachable("Unexpected branch opcode");
	              }
	            };

	            auto pickSetcImm = [&](unsigned BrOpc) -> unsigned {
	              switch (BrOpc) {
	              case LinxISA::BEQ:
	                return LinxISA::SETC_EQI;
	              case LinxISA::BNE:
	                return LinxISA::SETC_NEI;
	              case LinxISA::BLT:
	                return LinxISA::SETC_LTI;
	              case LinxISA::BGE:
	                return LinxISA::SETC_GEI;
	              case LinxISA::BLTU:
	                return LinxISA::SETC_LTUI;
	              case LinxISA::BGEU:
	                return LinxISA::SETC_GEUI;
	              default:
	                llvm_unreachable("Unexpected branch opcode");
	              }
	            };

	            auto getSingleUseImmFromZero = [&](MachineInstr &UseMI, Register Reg,
	                                               MachineInstr *&DefMIOut)
	                -> std::optional<int64_t> {
	              DefMIOut = nullptr;
	              if (!Reg || !Reg.isPhysical())
	                return std::nullopt;

	              for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	                --It;
	                MachineInstr &MI = *It;
	                if (MI.isDebugInstr() || isMarkerInstr(MI))
	                  continue;
	                if (!MI.definesRegister(Reg, &TRI))
	                  continue;
	                DefMIOut = &MI;
	                break;
	              }
	              if (!DefMIOut)
	                return std::nullopt;

	              MachineInstr &DefMI = *DefMIOut;
	              auto isFromZero = [&](unsigned BaseOpNo) -> bool {
	                if (BaseOpNo >= DefMI.getNumOperands())
	                  return false;
	                const MachineOperand &MO = DefMI.getOperand(BaseOpNo);
	                return MO.isReg() && MO.getReg() == LinxISA::R0;
	              };

	              int64_t Val = 0;
	              switch (DefMI.getOpcode()) {
	              case LinxISA::ADDIri:
	              case LinxISA::ADDIWri:
	                if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                    !DefMI.getOperand(2).isImm())
	                  return std::nullopt;
	                Val = DefMI.getOperand(2).getImm();
	                break;
	              case LinxISA::SUBIri:
	              case LinxISA::SUBIWri:
	                if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                    !DefMI.getOperand(2).isImm())
	                  return std::nullopt;
	                Val = -DefMI.getOperand(2).getImm();
	                break;
	              case LinxISA::LUI:
	                if (DefMI.getNumOperands() < 2 || !DefMI.getOperand(1).isImm())
	                  return std::nullopt;
	                Val = DefMI.getOperand(1).getImm() << 12;
	                break;
		              default:
		                return std::nullopt;
		              }

		              if (!hasSingleNonDbgUseInMBB(Reg, &UseMI, &DefMI))
		                return std::nullopt;
		              if (isPhysRegLiveOutOfBlock(Reg))
		                return std::nullopt;
		              return Val;
		            };

            // Prefer using the already-laid-out next block as fallthrough.
            unsigned BrOpcForSetc = Prev->getOpcode();
            if (FallthroughBB == JumpTargetBB) {
              Kind = ExitKind::Cond;
              TargetBB = BrTargetBB;
              SetcOpc = pickSetc(BrOpcForSetc);
            } else if (FallthroughBB == BrTargetBB) {
              Kind = ExitKind::Cond;
              TargetBB = JumpTargetBB;
              BrOpcForSetc = invertBranch(BrOpcForSetc);
              SetcOpc = pickSetc(BrOpcForSetc);
            } else {
              // Neither successor is laid out as fallthrough. Insert a small
              // trampoline block so we can keep BlockISA's "conditional +
              // implicit fallthrough" shape without requiring global block
              // reordering.
              //
              //   Bcc BrTarget; JUMP JumpTarget
              // becomes:
              //   (block header encodes conditional jump to BrTarget)
              //   fallthrough -> tramp
              //   tramp: JUMP JumpTarget
              MachineBasicBlock *TrampBB = makeTrampoline(JumpTargetBB);

              // Fix up the Machine-CFG: JumpTarget is no longer reached
              // directly from MBB. Update PHIs and edge lists.
              if (MBB.isSuccessor(JumpTargetBB)) {
                JumpTargetBB->replacePhiUsesWith(&MBB, TrampBB);
                MBB.removeSuccessor(JumpTargetBB);
              }
              if (!MBB.isSuccessor(TrampBB))
                MBB.addSuccessor(TrampBB);

              Kind = ExitKind::Cond;
              TargetBB = BrTargetBB;
              SetcOpc = pickSetc(BrOpcForSetc);
            }

	            bool EmittedImmSetc = false;
	            auto tryEmitSetcImm = [&](unsigned BrOpc, Register SrcReg,
	                                      int64_t ImmVal, MachineInstr *DefMI) -> bool {
	              unsigned ImmOpc = pickSetcImm(BrOpc);
	              const bool UnsignedImm =
	                  (ImmOpc == LinxISA::SETC_LTUI) || (ImmOpc == LinxISA::SETC_GEUI);
	              if (UnsignedImm) {
	                if (!canEncodeShiftedUnsignedImm(ImmVal, /*BaseBits=*/12))
	                  return false;
	              } else {
	                if (!canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
	                  return false;
	              }

	              auto SetcIt = findSetcInsertPt(MBB, *Prev, SrcReg, Register());
	              BuildMI(MBB, SetcIt, DebugLoc(), TII.get(ImmOpc))
	                  .addReg(SrcReg)
	                  .addImm(ImmVal);
	              if (DefMI)
	                DefMI->eraseFromParent();
	              return true;
	            };

	            MachineInstr *RHSDefMI = nullptr;
	            if (auto Imm = getSingleUseImmFromZero(*Prev, RHSReg, RHSDefMI)) {
	              EmittedImmSetc =
	                  tryEmitSetcImm(BrOpcForSetc, /*SrcReg=*/LHSReg, *Imm, RHSDefMI);
	            } else {
	              MachineInstr *LHSDefMI = nullptr;
	              if (auto Imm = getSingleUseImmFromZero(*Prev, LHSReg, LHSDefMI)) {
	                // If the constant is on the LHS, rewrite to keep the variable
	                // operand as SrcL for the immediate SETC forms.
	                const int64_t C = *Imm;
	                switch (BrOpcForSetc) {
	                case LinxISA::BEQ:
	                case LinxISA::BNE:
	                  EmittedImmSetc = tryEmitSetcImm(BrOpcForSetc, /*SrcReg=*/RHSReg,
	                                                  C, LHSDefMI);
	                  break;
	                case LinxISA::BLT:
	                  if (C != std::numeric_limits<int64_t>::max())
	                    EmittedImmSetc =
	                        tryEmitSetcImm(LinxISA::BGE, /*SrcReg=*/RHSReg, C + 1,
	                                       LHSDefMI);
	                  break;
	                case LinxISA::BGE:
	                  if (C != std::numeric_limits<int64_t>::max())
	                    EmittedImmSetc =
	                        tryEmitSetcImm(LinxISA::BLT, /*SrcReg=*/RHSReg, C + 1,
	                                       LHSDefMI);
	                  break;
	                case LinxISA::BLTU: {
	                  const uint64_t CU = static_cast<uint64_t>(C);
	                  if (CU != std::numeric_limits<uint64_t>::max())
	                    EmittedImmSetc = tryEmitSetcImm(LinxISA::BGEU,
	                                                    /*SrcReg=*/RHSReg,
	                                                    static_cast<int64_t>(CU + 1),
	                                                    LHSDefMI);
	                  break;
	                }
	                case LinxISA::BGEU: {
	                  const uint64_t CU = static_cast<uint64_t>(C);
	                  if (CU != std::numeric_limits<uint64_t>::max())
	                    EmittedImmSetc = tryEmitSetcImm(LinxISA::BLTU,
	                                                    /*SrcReg=*/RHSReg,
	                                                    static_cast<int64_t>(CU + 1),
	                                                    LHSDefMI);
	                  break;
	                }
	                default:
	                  break;
	                }
	              }
		            }

			            if (!EmittedImmSetc) {
			              auto tryEmitZextWSetcUW = [&]() -> bool {
			                if (BrOpcForSetc != LinxISA::BEQ && BrOpcForSetc != LinxISA::BNE)
			                  return false;
			                if ((LHSReg == LinxISA::R0) == (RHSReg == LinxISA::R0))
			                  return false;

			                const Register ZextReg =
			                    (LHSReg == LinxISA::R0) ? RHSReg : LHSReg;
			                Register OrigSrc;
			                MachineInstr *SllMI = nullptr;
			                MachineInstr *SrlMI = nullptr;
			                if (!matchZextWByShiftPair(*Prev, ZextReg, OrigSrc, SllMI, SrlMI))
			                  return false;

			                const unsigned NewSetcOpc =
			                    (BrOpcForSetc == LinxISA::BEQ) ? LinxISA::SETC_EQ
			                                                  : LinxISA::SETC_NE;
			                auto SetcIt = findSetcInsertPt(MBB, *Prev, LinxISA::R0, OrigSrc);
			                MachineInstr *NewMI =
			                    BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
			                        .addReg(LinxISA::R0)
			                        .addReg(OrigSrc)
			                        .getInstr();
			                NewMI->getOperand(1).setTargetFlags(
			                    NewMI->getOperand(1).getTargetFlags() | LinxII::MO_SRCR_UW);

			                SrlMI->eraseFromParent();
			                SllMI->eraseFromParent();
			                return true;
			              };

			              if (tryEmitZextWSetcUW()) {
			                EmittedImmSetc = true;
			              }

			              // Peephole: `and/or` feeding a nonzero branch against zero:
			              //   tmp = and/or x, y
			              //   bne tmp, zero, label
			              // =>
		              //   setc.and/or x, y
		              //
		              // and similarly for immediate ANDI/ORI:
		              //   tmp = andi/ori x, imm
		              //   bne tmp, zero, label
		              // =>
		              //   setc.andi/ori x, imm
		              auto tryEmitLogicSetcNZ = [&]() -> bool {
		                if (BrOpcForSetc != LinxISA::BNE)
		                  return false;

		                Register ZeroSide = Register();
		                Register ValSide = Register();
		                if (LHSReg == LinxISA::R0 && RHSReg != LinxISA::R0) {
		                  ZeroSide = LHSReg;
		                  ValSide = RHSReg;
		                } else if (RHSReg == LinxISA::R0 && LHSReg != LinxISA::R0) {
		                  ZeroSide = RHSReg;
		                  ValSide = LHSReg;
		                } else {
		                  return false;
		                }
		                (void)ZeroSide;

		                // Find defining instruction of ValSide (nearest preceding def).
		                MachineInstr *DefMI = nullptr;
		                for (auto It = Prev->getIterator(); It != MBB.begin();) {
		                  --It;
		                  MachineInstr &MI = *It;
		                  if (MI.isDebugInstr() || isMarkerInstr(MI))
		                    continue;
		                  if (MI.definesRegister(ValSide, &TRI)) {
		                    DefMI = &MI;
		                    break;
		                  }
		                }
		                if (!DefMI)
		                  return false;
		                if (isPhysRegLiveOutOfBlock(ValSide))
		                  return false;

		                unsigned NewSetcOpc = 0;
		                Register SrcA = Register(), SrcB = Register();
		                int64_t ImmVal = 0;
		                bool IsImm = false;

		                switch (DefMI->getOpcode()) {
		                case LinxISA::ANDrr:
		                case LinxISA::ANDWrr:
		                  if (DefMI->getNumOperands() < 3)
		                    return false;
		                  NewSetcOpc = LinxISA::SETC_AND;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  SrcB = DefMI->getOperand(2).getReg();
		                  break;
		                case LinxISA::ORrr:
		                case LinxISA::ORWrr:
		                  if (DefMI->getNumOperands() < 3)
		                    return false;
		                  NewSetcOpc = LinxISA::SETC_OR;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  SrcB = DefMI->getOperand(2).getReg();
		                  break;
		                case LinxISA::ANDIri:
		                case LinxISA::ANDIWri:
		                case LinxISA::HLANDIri:
		                case LinxISA::HLANDIWri:
		                  if (DefMI->getNumOperands() < 3 || !DefMI->getOperand(2).isImm())
		                    return false;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  ImmVal = DefMI->getOperand(2).getImm();
		                  if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                    NewSetcOpc = LinxISA::SETC_ANDI;
		                  else
		                    NewSetcOpc = LinxISA::HLSETC_ANDI;
		                  IsImm = true;
		                  break;
		                case LinxISA::ORIri:
		                case LinxISA::ORIWri:
		                case LinxISA::HLORIri:
		                case LinxISA::HLORIWri:
		                  if (DefMI->getNumOperands() < 3 || !DefMI->getOperand(2).isImm())
		                    return false;
		                  SrcA = DefMI->getOperand(1).getReg();
		                  ImmVal = DefMI->getOperand(2).getImm();
		                  if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                    NewSetcOpc = LinxISA::SETC_ORI;
		                  else
		                    NewSetcOpc = LinxISA::HLSETC_ORI;
		                  IsImm = true;
		                  break;
		                default:
		                  return false;
		                }

		                if (!hasSingleNonDbgUseInMBB(ValSide, Prev, DefMI))
		                  return false;

		                auto SetcIt = findSetcInsertPt(MBB, *Prev, SrcA, IsImm ? Register() : SrcB);
		                if (IsImm) {
		                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                      .addReg(SrcA)
		                      .addImm(ImmVal);
		                } else {
		                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                      .addReg(SrcA)
		                      .addReg(SrcB);
		                }
		                DefMI->eraseFromParent();
		                EmittedImmSetc = true;
		                return true;
		              };

			              if (!EmittedImmSetc && !tryEmitLogicSetcNZ()) {
			                auto SetcIt = findSetcInsertPt(MBB, *Prev, LHSReg, RHSReg);
			                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(SetcOpc))
			                    .addReg(LHSReg)
			                    .addReg(RHSReg);
			              }
			            }
	            Prev->eraseFromParent();
	            Last->eraseFromParent();
	            Changed = true;
	            break;
	          }

          Kind = ExitKind::Direct;
          TargetBB = Last->getOperand(0).getMBB();
          Last->eraseFromParent();
          Changed = true;
          break;
        }
        case LinxISA::BEQ:
        case LinxISA::BNE:
        case LinxISA::BLT:
        case LinxISA::BGE:
        case LinxISA::BLTU:
        case LinxISA::BGEU: {
          Kind = ExitKind::Cond;
          TargetBB = Last->getOperand(2).getMBB();

          unsigned SetcOpc = 0;
          switch (Last->getOpcode()) {
          case LinxISA::BEQ:
            SetcOpc = LinxISA::CSETC_EQ;
            break;
          case LinxISA::BNE:
            SetcOpc = LinxISA::CSETC_NE;
            break;
          case LinxISA::BLT:
            SetcOpc = LinxISA::SETC_LT;
            break;
          case LinxISA::BGE:
            SetcOpc = LinxISA::SETC_GE;
            break;
          case LinxISA::BLTU:
            SetcOpc = LinxISA::SETC_LTU;
            break;
          case LinxISA::BGEU:
            SetcOpc = LinxISA::SETC_GEU;
            break;
          default:
            llvm_unreachable("Unexpected branch opcode");
          }

	          Register LHSReg = Last->getOperand(0).getReg();
	          Register RHSReg = Last->getOperand(1).getReg();
	          bool EmittedImmSetc = false;
	          auto pickSetcImm = [&](unsigned BrOpc) -> unsigned {
	            switch (BrOpc) {
	            case LinxISA::BEQ:
	              return LinxISA::SETC_EQI;
	            case LinxISA::BNE:
	              return LinxISA::SETC_NEI;
	            case LinxISA::BLT:
	              return LinxISA::SETC_LTI;
	            case LinxISA::BGE:
	              return LinxISA::SETC_GEI;
	            case LinxISA::BLTU:
	              return LinxISA::SETC_LTUI;
	            case LinxISA::BGEU:
	              return LinxISA::SETC_GEUI;
	            default:
	              llvm_unreachable("Unexpected branch opcode");
	            }
	          };

	          auto getSingleUseImmFromZero = [&](MachineInstr &UseMI, Register Reg,
	                                             MachineInstr *&DefMIOut)
	              -> std::optional<int64_t> {
	            DefMIOut = nullptr;
	            if (!Reg || !Reg.isPhysical())
	              return std::nullopt;

	            for (auto It = UseMI.getIterator(); It != MBB.begin();) {
	              --It;
	              MachineInstr &MI = *It;
	              if (MI.isDebugInstr() || isMarkerInstr(MI))
	                continue;
	              if (!MI.definesRegister(Reg, &TRI))
	                continue;
	              DefMIOut = &MI;
	              break;
	            }
	            if (!DefMIOut)
	              return std::nullopt;

	            MachineInstr &DefMI = *DefMIOut;
	            auto isFromZero = [&](unsigned BaseOpNo) -> bool {
	              if (BaseOpNo >= DefMI.getNumOperands())
	                return false;
	              const MachineOperand &MO = DefMI.getOperand(BaseOpNo);
	              return MO.isReg() && MO.getReg() == LinxISA::R0;
	            };

	            int64_t Val = 0;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                  !DefMI.getOperand(2).isImm())
	                return std::nullopt;
	              Val = DefMI.getOperand(2).getImm();
	              break;
	            case LinxISA::SUBIri:
	            case LinxISA::SUBIWri:
	              if (!isFromZero(/*BaseOpNo=*/1) || DefMI.getNumOperands() < 3 ||
	                  !DefMI.getOperand(2).isImm())
	                return std::nullopt;
	              Val = -DefMI.getOperand(2).getImm();
	              break;
	            case LinxISA::LUI:
	              if (DefMI.getNumOperands() < 2 || !DefMI.getOperand(1).isImm())
	                return std::nullopt;
	              Val = DefMI.getOperand(1).getImm() << 12;
	              break;
		            default:
		              return std::nullopt;
		            }

		            if (!hasSingleNonDbgUseInMBB(Reg, &UseMI, &DefMI))
		              return std::nullopt;
		            if (isPhysRegLiveOutOfBlock(Reg))
		              return std::nullopt;
		            return Val;
		          };

	          auto tryEmitSetcImm = [&](unsigned BrOpc, Register SrcReg, int64_t ImmVal,
	                                    MachineInstr *DefMI) -> bool {
	            unsigned ImmOpc = pickSetcImm(BrOpc);
	            const bool UnsignedImm =
	                (ImmOpc == LinxISA::SETC_LTUI) || (ImmOpc == LinxISA::SETC_GEUI);
	            if (UnsignedImm) {
	              if (!canEncodeShiftedUnsignedImm(ImmVal, /*BaseBits=*/12))
	                return false;
	            } else {
	              if (!canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
	                return false;
	            }

	            auto SetcIt = findSetcInsertPt(MBB, *Last, SrcReg, Register());
	            BuildMI(MBB, SetcIt, DebugLoc(), TII.get(ImmOpc))
	                .addReg(SrcReg)
	                .addImm(ImmVal);
	            if (DefMI)
	              DefMI->eraseFromParent();
	            return true;
	          };

	          MachineInstr *RHSDefMI = nullptr;
	          if (auto Imm = getSingleUseImmFromZero(*Last, RHSReg, RHSDefMI)) {
	            EmittedImmSetc =
	                tryEmitSetcImm(Last->getOpcode(), /*SrcReg=*/LHSReg, *Imm, RHSDefMI);
	          } else {
	            MachineInstr *LHSDefMI = nullptr;
	            if (auto Imm = getSingleUseImmFromZero(*Last, LHSReg, LHSDefMI)) {
	              const int64_t C = *Imm;
	              switch (Last->getOpcode()) {
	              case LinxISA::BEQ:
	              case LinxISA::BNE:
	                EmittedImmSetc =
	                    tryEmitSetcImm(Last->getOpcode(), /*SrcReg=*/RHSReg, C, LHSDefMI);
	                break;
	              case LinxISA::BLT:
	                if (C != std::numeric_limits<int64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BGE, /*SrcReg=*/RHSReg, C + 1, LHSDefMI);
	                break;
	              case LinxISA::BGE:
	                if (C != std::numeric_limits<int64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BLT, /*SrcReg=*/RHSReg, C + 1, LHSDefMI);
	                break;
	              case LinxISA::BLTU: {
	                const uint64_t CU = static_cast<uint64_t>(C);
	                if (CU != std::numeric_limits<uint64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BGEU, /*SrcReg=*/RHSReg,
	                      static_cast<int64_t>(CU + 1), LHSDefMI);
	                break;
	              }
	              case LinxISA::BGEU: {
	                const uint64_t CU = static_cast<uint64_t>(C);
	                if (CU != std::numeric_limits<uint64_t>::max())
	                  EmittedImmSetc = tryEmitSetcImm(
	                      LinxISA::BLTU, /*SrcReg=*/RHSReg,
	                      static_cast<int64_t>(CU + 1), LHSDefMI);
	                break;
	              }
	              default:
	                break;
	              }
	            }
	          }

			          if (!EmittedImmSetc) {
			            auto tryEmitZextWSetcUW = [&]() -> bool {
			              if (Last->getOpcode() != LinxISA::BEQ &&
			                  Last->getOpcode() != LinxISA::BNE)
			                return false;
			              if ((LHSReg == LinxISA::R0) == (RHSReg == LinxISA::R0))
			                return false;

			              const Register ZextReg =
			                  (LHSReg == LinxISA::R0) ? RHSReg : LHSReg;
			              Register OrigSrc;
			              MachineInstr *SllMI = nullptr;
			              MachineInstr *SrlMI = nullptr;
			              if (!matchZextWByShiftPair(*Last, ZextReg, OrigSrc, SllMI, SrlMI))
			                return false;

			              const unsigned NewSetcOpc =
			                  (Last->getOpcode() == LinxISA::BEQ) ? LinxISA::SETC_EQ
			                                                     : LinxISA::SETC_NE;
			              auto SetcIt = findSetcInsertPt(MBB, *Last, LinxISA::R0, OrigSrc);
			              MachineInstr *NewMI =
			                  BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
			                      .addReg(LinxISA::R0)
			                      .addReg(OrigSrc)
			                      .getInstr();
			              NewMI->getOperand(1).setTargetFlags(
			                  NewMI->getOperand(1).getTargetFlags() | LinxII::MO_SRCR_UW);

			              SrlMI->eraseFromParent();
			              SllMI->eraseFromParent();
			              return true;
			            };

			            if (tryEmitZextWSetcUW())
			              EmittedImmSetc = true;

			            auto tryEmitLogicSetcNZ = [&]() -> bool {
			              if (Last->getOpcode() != LinxISA::BNE)
			                return false;

		              Register ValSide = Register();
		              if (LHSReg == LinxISA::R0 && RHSReg != LinxISA::R0)
		                ValSide = RHSReg;
		              else if (RHSReg == LinxISA::R0 && LHSReg != LinxISA::R0)
		                ValSide = LHSReg;
		              else
		                return false;

		              MachineInstr *DefMI = nullptr;
		              for (auto It = Last->getIterator(); It != MBB.begin();) {
		                --It;
		                MachineInstr &MI = *It;
		                if (MI.isDebugInstr() || isMarkerInstr(MI))
		                  continue;
		                if (MI.definesRegister(ValSide, &TRI)) {
		                  DefMI = &MI;
		                  break;
		                }
		              }
		              if (!DefMI)
		                return false;
		              if (!hasSingleNonDbgUseInMBB(ValSide, Last, DefMI))
		                return false;
		              if (isPhysRegLiveOutOfBlock(ValSide))
		                return false;

		              unsigned NewSetcOpc = 0;
		              Register SrcA = Register(), SrcB = Register();
		              int64_t ImmVal = 0;
		              bool IsImm = false;

		              switch (DefMI->getOpcode()) {
		              case LinxISA::ANDrr:
		              case LinxISA::ANDWrr:
		                NewSetcOpc = LinxISA::SETC_AND;
		                SrcA = DefMI->getOperand(1).getReg();
		                SrcB = DefMI->getOperand(2).getReg();
		                break;
		              case LinxISA::ORrr:
		              case LinxISA::ORWrr:
		                NewSetcOpc = LinxISA::SETC_OR;
		                SrcA = DefMI->getOperand(1).getReg();
		                SrcB = DefMI->getOperand(2).getReg();
		                break;
		              case LinxISA::ANDIri:
		              case LinxISA::ANDIWri:
		              case LinxISA::HLANDIri:
		              case LinxISA::HLANDIWri:
		                if (!DefMI->getOperand(2).isImm())
		                  return false;
		                SrcA = DefMI->getOperand(1).getReg();
		                ImmVal = DefMI->getOperand(2).getImm();
		                if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                  NewSetcOpc = LinxISA::SETC_ANDI;
		                else
		                  NewSetcOpc = LinxISA::HLSETC_ANDI;
		                IsImm = true;
		                break;
		              case LinxISA::ORIri:
		              case LinxISA::ORIWri:
		              case LinxISA::HLORIri:
		              case LinxISA::HLORIWri:
		                if (!DefMI->getOperand(2).isImm())
		                  return false;
		                SrcA = DefMI->getOperand(1).getReg();
		                ImmVal = DefMI->getOperand(2).getImm();
		                if (canEncodeShiftedSignedImm(ImmVal, /*BaseBits=*/12))
		                  NewSetcOpc = LinxISA::SETC_ORI;
		                else
		                  NewSetcOpc = LinxISA::HLSETC_ORI;
		                IsImm = true;
		                break;
		              default:
		                return false;
		              }

		              auto SetcIt =
		                  findSetcInsertPt(MBB, *Last, SrcA, IsImm ? Register() : SrcB);
		              if (IsImm) {
		                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                    .addReg(SrcA)
		                    .addImm(ImmVal);
		              } else {
		                BuildMI(MBB, SetcIt, DebugLoc(), TII.get(NewSetcOpc))
		                    .addReg(SrcA)
		                    .addReg(SrcB);
		              }
		              DefMI->eraseFromParent();
		              return true;
		            };

			            if (!EmittedImmSetc && !tryEmitLogicSetcNZ()) {
			              auto SetcIt = findSetcInsertPt(MBB, *Last, LHSReg, RHSReg);
			              BuildMI(MBB, SetcIt, DebugLoc(), TII.get(SetcOpc))
			                  .addReg(LHSReg)
			                  .addReg(RHSReg);
			            }
			          }
	          Last->eraseFromParent();
	          Changed = true;
	          break;
	        }
	        default:
	          break;
	        }
		      }

		      // Peephole after BlockISA exit lowering (but before inserting block
		      // markers and T/U remapping): fold a word zero-extend shift-pair
		      // feeding a compare against zero into a SrcR `.uw` conversion
		      // modifier. The ISA only supports conversions on the right operand,
		      // so we flip operands when needed:
		      //
		      //   tmp1 = sll x, 32
			      //   tmp2 = srl tmp1, 32
			      //   c.setc.ne tmp2, zero
			      // =>
			      //   setc.ne zero, x<.uw>
			      auto prevNonMarkerMI = [&](MachineBasicBlock::iterator Pos) -> MachineInstr * {
			        auto It = Pos;
			        while (It != MBB.begin()) {
			          --It;
			          if (It->isDebugInstr() || isMarkerInstr(*It))
			            continue;
			          return &*It;
			        }
			        return nullptr;
			      };

			      auto hasUseBeforeDefFrom = [&](Register Reg,
			                                    MachineBasicBlock::iterator Start) -> bool {
			        if (!Reg)
			          return false;
			        for (auto It = Start; It != MBB.end(); ++It) {
			          MachineInstr &MI = *It;
			          if (MI.isDebugInstr() || isMarkerInstr(MI))
			            continue;
			          if (MI.definesRegister(Reg, &TRI))
			            return false;
			          for (const MachineOperand &MO : MI.operands()) {
			            if (!MO.isReg() || MO.isImplicit() || MO.isDef())
			              continue;
			            if (MO.getReg() == Reg)
			              return true;
			          }
			        }
			        return false;
			      };

			      auto matchAdjacentZextWByShiftPair =
			          [&](MachineInstr &UseMI, Register ZextReg, Register &OrigSrc,
			              MachineInstr *&SllMIOut, MachineInstr *&SrlMIOut) -> bool {
			        OrigSrc = Register();
			        SllMIOut = nullptr;
			        SrlMIOut = nullptr;
			        if (!ZextReg)
			          return false;

			        MachineInstr *SrlMI = prevNonMarkerMI(UseMI.getIterator());
			        if (!SrlMI || SrlMI->getOpcode() != LinxISA::SRLrr ||
			            SrlMI->getNumOperands() < 3 || !SrlMI->getOperand(1).isReg() ||
			            !SrlMI->getOperand(2).isReg())
			          return false;
			        if (!SrlMI->definesRegister(ZextReg, &TRI))
			          return false;

			        const Register Tmp1 = SrlMI->getOperand(1).getReg();
			        const Register ShAmtReg = SrlMI->getOperand(2).getReg();
			        if (!Tmp1 || !ShAmtReg)
			          return false;

			        MachineInstr *SllMI = prevNonMarkerMI(SrlMI->getIterator());
			        if (!SllMI || SllMI->getOpcode() != LinxISA::SLLrr ||
			            SllMI->getNumOperands() < 3 || !SllMI->getOperand(1).isReg() ||
			            !SllMI->getOperand(2).isReg())
			          return false;
			        if (!SllMI->definesRegister(Tmp1, &TRI))
			          return false;
			        if (SllMI->getOperand(2).getReg() != ShAmtReg)
			          return false;

			        auto getConstShiftAmt = [&](MachineInstr &Anchor) -> std::optional<int64_t> {
			          for (auto DI = Anchor.getIterator(); DI != MBB.begin();) {
			            --DI;
			            MachineInstr &DefMI = *DI;
			            if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
			              continue;
			            if (!DefMI.definesRegister(ShAmtReg, &TRI))
			              continue;
			            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
			                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
			                !DefMI.getOperand(2).isImm())
			              return std::nullopt;
			            switch (DefMI.getOpcode()) {
			            case LinxISA::ADDIri:
			            case LinxISA::ADDIWri:
			              return DefMI.getOperand(2).getImm();
			            default:
			              return std::nullopt;
			            }
			          }
			          if (ShAmtReg.isPhysical())
			            return getPhysRegConstAtMBBEntry(ShAmtReg);
			          return std::nullopt;
			        };

			        auto ShAmtC = getConstShiftAmt(*SrlMI);
			        if (!ShAmtC || *ShAmtC != 32)
			          return false;

			        const Register Src = SllMI->getOperand(1).getReg();
			        if (!Src)
			          return false;

			        // Ensure the shift results are not used later (otherwise removing
			        // the shifts would break the block).
			        if (hasUseBeforeDefFrom(ZextReg, std::next(UseMI.getIterator())))
			          return false;
			        if (hasUseBeforeDefFrom(Tmp1, std::next(SrlMI->getIterator())))
			          return false;

			        OrigSrc = Src;
			        SllMIOut = SllMI;
			        SrlMIOut = SrlMI;
			        return true;
			      };

			      for (auto It = MBB.begin(); It != MBB.end();) {
			        MachineInstr &MI = *It;
			        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
			          ++It;
		          continue;
		        }

		        const unsigned Opc = MI.getOpcode();
		        const bool IsEq =
		            (Opc == LinxISA::CSETC_EQ || Opc == LinxISA::SETC_EQ);
		        const bool IsNe =
		            (Opc == LinxISA::CSETC_NE || Opc == LinxISA::SETC_NE);
		        if (!IsEq && !IsNe) {
		          ++It;
		          continue;
		        }

		        if (MI.getNumOperands() < 2 || !MI.getOperand(0).isReg() ||
		            !MI.getOperand(1).isReg()) {
		          ++It;
		          continue;
		        }

		        const Register A = MI.getOperand(0).getReg();
		        const Register B = MI.getOperand(1).getReg();
		        if (!A || !B) {
		          ++It;
		          continue;
		        }
			        if (A != LinxISA::R0 && B != LinxISA::R0) {
			          ++It;
			          continue;
			        }

			        const Register ZextReg = (A == LinxISA::R0) ? B : A;
			        Register OrigSrc;
			        MachineInstr *SllMI = nullptr;
			        MachineInstr *SrlMI = nullptr;
			        if (!matchAdjacentZextWByShiftPair(MI, ZextReg, OrigSrc, SllMI, SrlMI)) {
			          ++It;
			          continue;
			        }

		        const unsigned NewOpc = IsEq ? LinxISA::SETC_EQ : LinxISA::SETC_NE;
		        MachineInstr *NewMI =
		            BuildMI(MBB, MI.getIterator(), MI.getDebugLoc(), TII.get(NewOpc))
		                .addReg(LinxISA::R0)
		                .addReg(OrigSrc)
		                .getInstr();
		        NewMI->getOperand(1).setTargetFlags(
		            NewMI->getOperand(1).getTargetFlags() | LinxII::MO_SRCR_UW);

		        auto NextIt = std::next(It);
		        MI.eraseFromParent();
		        SrlMI->eraseFromParent();
		        SllMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      for (auto It = MBB.begin(); It != MBB.end();) {
		        MachineInstr &MI = *It;
		        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
		          ++It;
		          continue;
		        }

		        const unsigned Opc = MI.getOpcode();
		        const bool IsEq = (Opc == LinxISA::CMPEQ);
		        const bool IsNe = (Opc == LinxISA::CMPNE);
		        if (!IsEq && !IsNe) {
		          ++It;
		          continue;
		        }

		        if (MI.getNumOperands() < 3 || !MI.getOperand(1).isReg() ||
		            !MI.getOperand(2).isReg()) {
		          ++It;
		          continue;
		        }

		        const Register SrcL = MI.getOperand(1).getReg();
		        const Register SrcR = MI.getOperand(2).getReg();
		        if (!SrcL || !SrcR) {
		          ++It;
		          continue;
		        }
		        if (SrcL != LinxISA::R0 && SrcR != LinxISA::R0) {
		          ++It;
		          continue;
		        }

			        const Register ZextReg = (SrcL == LinxISA::R0) ? SrcR : SrcL;
			        Register OrigSrc;
			        MachineInstr *SllMI = nullptr;
			        MachineInstr *SrlMI = nullptr;
			        if (!matchAdjacentZextWByShiftPair(MI, ZextReg, OrigSrc, SllMI, SrlMI)) {
			          ++It;
			          continue;
			        }

		        MI.getOperand(1).setReg(LinxISA::R0);
		        MI.getOperand(2).setReg(OrigSrc);
		        MI.getOperand(2).setTargetFlags(
		            MI.getOperand(2).getTargetFlags() | LinxII::MO_SRCR_UW);

		        auto NextIt = std::next(It);
		        SrlMI->eraseFromParent();
		        SllMI->eraseFromParent();
		        Changed = true;
		        It = NextIt;
		      }

		      // Pre-blockify peephole: fuse MUL + ADD into MADD when the MUL result
		      // is single-use in the block.
		      //
		      //   tmp = mul  a, b
	      //   dst = add  tmp, c
	      // =>
	      //   dst = madd a, b, c
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MulMI = *It;
	        if (MulMI.isDebugInstr() || isMarkerInstr(MulMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned MulOpc = MulMI.getOpcode();
	        const bool IsMul64 = (MulOpc == LinxISA::MULrr);
	        const bool IsMul32 = (MulOpc == LinxISA::MULWrr);
	        if (!IsMul64 && !IsMul32) {
	          ++It;
	          continue;
	        }

	        if (MulMI.getNumOperands() < 3 || !MulMI.getOperand(0).isReg() ||
	            !MulMI.getOperand(0).isDef() || !MulMI.getOperand(1).isReg() ||
	            !MulMI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = MulMI.getOperand(0).getReg();
	        const Register A = MulMI.getOperand(1).getReg();
	        const Register B = MulMI.getOperand(2).getReg();
	        if (!Tmp.isPhysical() || !A.isPhysical() || !B.isPhysical()) {
	          ++It;
	          continue;
	        }
	        if (isPhysRegLiveOutOfBlock(Tmp)) {
	          ++It;
	          continue;
	        }

	        // Find the next real instruction that reads Tmp and try to match ADD.
	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
	        if (NextIt == MBB.end()) {
	          ++It;
	          continue;
	        }

	        MachineInstr &AddMI = *NextIt;
	        const unsigned AddOpc = AddMI.getOpcode();
	        const bool IsAdd64 = (AddOpc == LinxISA::ADDrr);
	        const bool IsAdd32 = (AddOpc == LinxISA::ADDWrr);
	        if (!((IsMul64 && IsAdd64) || (IsMul32 && IsAdd32))) {
	          ++It;
	          continue;
	        }

	        if (AddMI.getNumOperands() < 3 || !AddMI.getOperand(0).isReg() ||
	            !AddMI.getOperand(0).isDef() || !AddMI.getOperand(1).isReg() ||
	            !AddMI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = AddMI.getOperand(0).getReg();
	        if (!Dst.isPhysical()) {
	          ++It;
	          continue;
	        }

	        const Register Op1 = AddMI.getOperand(1).getReg();
	        const Register Op2 = AddMI.getOperand(2).getReg();
	        Register C = Register();
	        if (Op1 == Tmp && Op2.isPhysical())
	          C = Op2;
	        else if (Op2 == Tmp && Op1.isPhysical())
	          C = Op1;
	        else {
	          ++It;
	          continue;
	        }

	        if (!hasSingleNonDbgUseInMBB(Tmp, &AddMI, &MulMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned MaddOpc = IsMul64 ? LinxISA::MADD : LinxISA::MADDW;
	        MachineInstr *NewMI =
	            BuildMI(MBB, AddMI.getIterator(), AddMI.getDebugLoc(),
	                    TII.get(MaddOpc), Dst)
	                .addReg(A)
	                .addReg(B)
	                .addReg(C)
	                .getInstr();

	        AddMI.eraseFromParent();
	        MulMI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Peephole: sink simple address calculations into immediate-offset
	      // loads/stores when the combined offset fits the instruction encoding.
	      //
	      // This favors using complex addressing modes over sharing an AGEN
	      // temporary across multiple memory ops:
	      //   addi tmp, base, C
	      //   lw   rd, [tmp + off]
	      // =>
	      //   lw   rd, [base + (C+off)]
	      //
	      // When all uses of `tmp` in the block are foldable, the AGEN is removed.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &DefMI = *It;
	        if (DefMI.isDebugInstr() || isMarkerInstr(DefMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned DefOpc = DefMI.getOpcode();
	        const bool IsAddI = (DefOpc == LinxISA::ADDIri || DefOpc == LinxISA::ADDIWri);
	        const bool IsSubI = (DefOpc == LinxISA::SUBIri || DefOpc == LinxISA::SUBIWri);
	        if (!IsAddI && !IsSubI) {
	          ++It;
	          continue;
	        }
	        if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(0).isReg() ||
	            !DefMI.getOperand(0).isDef() || !DefMI.getOperand(1).isReg() ||
	            !DefMI.getOperand(2).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Tmp = DefMI.getOperand(0).getReg();
	        const Register Base = DefMI.getOperand(1).getReg();
	        const int64_t Addend = IsSubI ? -DefMI.getOperand(2).getImm()
	                                      : DefMI.getOperand(2).getImm();
	        if (!Tmp || !Tmp.isPhysical() || !Base || !Base.isPhysical()) {
	          ++It;
	          continue;
	        }
	        // This peephole is intended for the "address-generation temporary"
	        // shape `addi tmp, base, C` where `tmp` is a distinct register. If we
	        // try to fold an in-place update (tmp == base), we would need to
	        // *remove* the defining instruction to preserve semantics; doing so
	        // is generally not possible for reserved/live-out regs like `sp`.
	        if (Tmp == Base) {
	          ++It;
	          continue;
	        }
	        const bool CanEraseDef = !isPhysRegLiveOutOfBlock(Tmp);

	        struct UseRef {
	          MachineInstr *MI;
	          unsigned BaseOpNo;
	          unsigned OffOpNo;
	        };
	        SmallVector<UseRef, 4> Uses;

	        auto isFoldableMem = [&](MachineInstr &MI, unsigned &BaseOpNo,
	                                 unsigned &OffOpNo) -> bool {
	          const unsigned Opc = MI.getOpcode();
	          switch (Opc) {
	          case LinxISA::LBI:
	          case LinxISA::LBUI:
	          case LinxISA::LHI:
	          case LinxISA::LHUI:
	          case LinxISA::LWI:
	          case LinxISA::LWUI:
	          case LinxISA::LDI:
	          case LinxISA::SBI:
	          case LinxISA::SHI:
	          case LinxISA::SWI:
	          case LinxISA::SDI:
	            BaseOpNo = 1;
	            OffOpNo = 2;
	            return true;
	          default:
	            return false;
	          }
	        };
	        auto memImmScale = [&](unsigned Opc) -> int64_t {
	          switch (Opc) {
	          case LinxISA::LBI:
	          case LinxISA::LBUI:
	          case LinxISA::SBI:
	            return 1;
	          case LinxISA::LHI:
	          case LinxISA::LHUI:
	          case LinxISA::SHI:
	            return 2;
	          case LinxISA::LWI:
	          case LinxISA::LWUI:
	          case LinxISA::SWI:
	            return 4;
	          case LinxISA::LDI:
	          case LinxISA::SDI:
	            return 8;
	          default:
	            return 1;
	          }
	        };

	        // Collect all uses from DefMI onwards until Tmp is redefined.
	        bool Bad = false;
	        bool StoppedByBaseDef = false;
	        for (auto UI = std::next(It), UE = MBB.end(); UI != UE; ++UI) {
	          MachineInstr &MI = *UI;
	          if (MI.isDebugInstr() || isMarkerInstr(MI))
	            continue;
	          if (MI.definesRegister(Tmp, &TRI))
	            break;
	          // Folding `tmp = base + C` into uses of `tmp` requires that `base`
	          // still holds the same value. If `base` is redefined, stop
	          // collecting fold candidates to avoid rewriting memory ops to use
	          // an updated base.
	          if (MI.definesRegister(Base, &TRI)) {
	            StoppedByBaseDef = true;
	            break;
	          }

	          unsigned BaseOpNo = 0, OffOpNo = 0;
	          if (!isFoldableMem(MI, BaseOpNo, OffOpNo)) {
	            if (MI.readsRegister(Tmp, &TRI)) {
	              Bad = true;
	              break;
	            }
	            continue;
	          }

	          if (BaseOpNo >= MI.getNumOperands() || OffOpNo >= MI.getNumOperands()) {
	            Bad = true;
	            break;
	          }
	          MachineOperand &BaseMO = MI.getOperand(BaseOpNo);
	          MachineOperand &OffMO = MI.getOperand(OffOpNo);
	          if (!BaseMO.isReg() || BaseMO.getReg() != Tmp) {
	            if (MI.readsRegister(Tmp, &TRI)) {
	              Bad = true;
	              break;
	            }
	            continue;
	          }
	          if (!OffMO.isImm()) {
	            Bad = true;
	            break;
	          }

	          const int64_t OldOff = OffMO.getImm();
	          // The machine-level mem-immediate is in *scaled units* (AArch64
	          // style): the final byte offset is `imm * access_size`. Convert the
	          // address-generation addend (bytes) into the same unit system.
	          const int64_t Scale = memImmScale(MI.getOpcode());
	          if (Scale <= 0 || (Addend % Scale) != 0) {
	            Bad = true;
	            break;
	          }
	          const int64_t NewOff = OldOff + (Addend / Scale);
	          if (!isInt<12>(NewOff)) {
	            Bad = true;
	            break;
	          }

	          Uses.push_back(UseRef{&MI, BaseOpNo, OffOpNo});
	        }

	        if (Bad || Uses.empty()) {
	          ++It;
	          continue;
	        }

	        for (const UseRef &U : Uses) {
	          U.MI->getOperand(U.BaseOpNo).setReg(Base);
	          const int64_t OldOff = U.MI->getOperand(U.OffOpNo).getImm();
	          const int64_t Scale = memImmScale(U.MI->getOpcode());
	          U.MI->getOperand(U.OffOpNo).setImm(OldOff + (Addend / Scale));
	        }

	        auto Next = std::next(It);
	        if (CanEraseDef && !StoppedByBaseDef) {
	          DefMI.eraseFromParent();
	          It = Next;
	        } else {
	          It = Next;
	        }
	        Changed = true;
	      }

	      // Peephole: fold `slli tmp, x, k; add dst, base, tmp` into
	      // `add base, x<<k, ->dst` (uses the ISA shamt field).
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &ShiftMI = *It;
	        if (ShiftMI.isDebugInstr() || isMarkerInstr(ShiftMI)) {
	          ++It;
	          continue;
	        }

	        const unsigned ShiftOpc = ShiftMI.getOpcode();

	        // Peephole: fold `sll tmp, x, sh; srl/sra dst, tmp, sh` into
	        // `bxu/bxs x, M=0, N=<width>`.
	        if (ShiftOpc == LinxISA::SLLrr && ShiftMI.getNumOperands() >= 3 &&
	            ShiftMI.getOperand(2).isReg()) {
	          const Register ShDst = ShiftMI.getOperand(0).getReg();
	          const Register ShSrc = ShiftMI.getOperand(1).getReg();
	          const Register ShAmtReg = ShiftMI.getOperand(2).getReg();

	          auto NextIt = std::next(It);
	          while (NextIt != MBB.end() &&
	                 (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	            ++NextIt;
	          if (NextIt == MBB.end()) {
	            ++It;
	            continue;
	          }

	          MachineInstr &ShrMI = *NextIt;
	          const unsigned ShrOpc = ShrMI.getOpcode();
	          const bool IsShiftR = (ShrOpc == LinxISA::SRLrr || ShrOpc == LinxISA::SRArr);
	          if (!IsShiftR || ShrMI.getNumOperands() < 3) {
	            ++It;
	            continue;
	          }

	          if (ShrMI.getOperand(1).getReg() != ShDst ||
	              !ShrMI.getOperand(2).isReg() ||
	              ShrMI.getOperand(2).getReg() != ShAmtReg) {
	            ++It;
	            continue;
	          }

	          // The shifted temporary must be single-use.
	          if (!hasSingleNonDbgUseInMBB(ShDst, &ShrMI, &ShiftMI)) {
	            ++It;
	            continue;
	          }

	          // Recover the constant shift amount from its defining instruction.
	          MachineInstr *ShAmtDefMI = nullptr;
	          auto getConstShiftAmt = [&](MachineInstr &UseMI) -> std::optional<int64_t> {
	            if (!ShAmtReg || !ShAmtReg.isPhysical())
	              return std::nullopt;
	            for (auto DI = UseMI.getIterator(); DI != MBB.begin();) {
	              --DI;
	              MachineInstr &DefMI = *DI;
	              if (DefMI.isDebugInstr() || isMarkerInstr(DefMI))
	                continue;
	              if (!DefMI.definesRegister(ShAmtReg, &TRI))
	                continue;
	              ShAmtDefMI = &DefMI;
	              break;
	            }
	            if (!ShAmtDefMI)
	              return std::nullopt;

	            MachineInstr &DefMI = *ShAmtDefMI;
	            if (DefMI.getNumOperands() < 3 || !DefMI.getOperand(1).isReg() ||
	                DefMI.getOperand(1).getReg() != LinxISA::R0 ||
	                !DefMI.getOperand(2).isImm())
	              return std::nullopt;
	            switch (DefMI.getOpcode()) {
	            case LinxISA::ADDIri:
	            case LinxISA::ADDIWri:
	              return DefMI.getOperand(2).getImm();
	            default:
	              return std::nullopt;
	            }
	          };

	          auto ShAmtC = getConstShiftAmt(ShiftMI);
	          if (!ShAmtC) {
	            ++It;
	            continue;
	          }
	          const int64_t ShAmt = *ShAmtC;
	          if (ShAmt <= 0 || ShAmt >= 64) {
	            ++It;
	            continue;
	          }

	          const int64_t Width = 64 - ShAmt;
	          const int64_t Imml = Width - 1; // N-1
	          const int64_t Imms = 0;         // M=0
	          if (!isUInt<6>(static_cast<uint64_t>(Imml))) {
	            ++It;
	            continue;
	          }

	          const Register ShrDst = ShrMI.getOperand(0).getReg();
	          const unsigned NewOpc = (ShrOpc == LinxISA::SRArr) ? LinxISA::BXS
	                                                             : LinxISA::BXU;
	          MachineInstr *NewMI =
	              BuildMI(MBB, ShrMI.getIterator(), ShrMI.getDebugLoc(),
	                      TII.get(NewOpc), ShrDst)
	                  .addReg(ShSrc)
	                  .addImm(Imml)
	                  .addImm(Imms)
	                  .getInstr();

	          // Remove the original two shifts.
	          ShrMI.eraseFromParent();
	          ShiftMI.eraseFromParent();

	          // If the shift-amount materialization is now dead and not live-out,
	          // remove it too.
	          if (ShAmtDefMI && !isPhysRegLiveOutOfBlock(ShAmtReg)) {
	            bool AnyUse = false;
	            for (const MachineInstr &MI : MBB) {
	              if (MI.isDebugInstr() || isMarkerInstr(MI))
	                continue;
	              if (MI.readsRegister(ShAmtReg, &TRI)) {
	                AnyUse = true;
	                break;
	              }
	            }
	            if (!AnyUse)
	              ShAmtDefMI->eraseFromParent();
	          }

	          Changed = true;
	          It = std::next(NewMI->getIterator());
	          continue;
	        }

	        const bool IsSLLI =
	            (ShiftOpc == LinxISA::SLLIri || ShiftOpc == LinxISA::SLLIWri);
	        if (!IsSLLI || ShiftMI.getNumOperands() < 3 || !ShiftMI.getOperand(2).isImm()) {
	          ++It;
	          continue;
        }

        const Register ShDst = ShiftMI.getOperand(0).getReg();
        const Register ShSrc = ShiftMI.getOperand(1).getReg();
        const int64_t ShAmt = ShiftMI.getOperand(2).getImm();
        if (ShAmt == 0) {
          ++It;
          continue;
        }

	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
        if (NextIt == MBB.end()) {
          ++It;
          continue;
        }

	        MachineInstr &BinMI = *NextIt;
	        const unsigned BinOpc = BinMI.getOpcode();

	        // Peephole: fold `slli tmp, x, k; srli/srai dst, tmp, k` into BXU/BXS.
	        if (ShiftOpc == LinxISA::SLLIri &&
	            (BinOpc == LinxISA::SRLIri || BinOpc == LinxISA::SRAIri) &&
	            BinMI.getNumOperands() >= 3 && BinMI.getOperand(2).isImm() &&
	            BinMI.getOperand(1).isReg() && BinMI.getOperand(1).getReg() == ShDst &&
	            BinMI.getOperand(2).getImm() == ShAmt) {
	          if (hasSingleNonDbgUseInMBB(ShDst, &BinMI, &ShiftMI)) {
	            const int64_t Width = 64 - ShAmt;
	            const int64_t Imml = Width - 1;
	            const int64_t Imms = 0;
	            if (ShAmt > 0 && ShAmt < 64 &&
	                isUInt<6>(static_cast<uint64_t>(Imml))) {
	              const Register Dst = BinMI.getOperand(0).getReg();
	              const unsigned NewOpc = (BinOpc == LinxISA::SRAIri) ? LinxISA::BXS
	                                                                  : LinxISA::BXU;
	              MachineInstr *NewMI =
	                  BuildMI(MBB, BinMI.getIterator(), BinMI.getDebugLoc(),
	                          TII.get(NewOpc), Dst)
	                      .addReg(ShSrc)
	                      .addImm(Imml)
	                      .addImm(Imms)
	                      .getInstr();
	              BinMI.eraseFromParent();
	              ShiftMI.eraseFromParent();
	              Changed = true;
	              It = std::next(NewMI->getIterator());
	              continue;
	            }
	          }
	        }
        unsigned NewOpc = 0;

        if (ShiftOpc == LinxISA::SLLIri) {
          switch (BinOpc) {
          case LinxISA::ADDrr:
            NewOpc = LinxISA::ADDrr_SH;
            break;
          case LinxISA::SUBrr:
            NewOpc = LinxISA::SUBrr_SH;
            break;
          case LinxISA::ANDrr:
            NewOpc = LinxISA::ANDrr_SH;
            break;
          case LinxISA::ORrr:
            NewOpc = LinxISA::ORrr_SH;
            break;
          case LinxISA::XORrr:
            NewOpc = LinxISA::XORrr_SH;
            break;
          default:
            break;
          }
        } else if (ShiftOpc == LinxISA::SLLIWri) {
          switch (BinOpc) {
          case LinxISA::ADDWrr:
            NewOpc = LinxISA::ADDWrr_SH;
            break;
          case LinxISA::SUBWrr:
            NewOpc = LinxISA::SUBWrr_SH;
            break;
          case LinxISA::ANDWrr:
            NewOpc = LinxISA::ANDWrr_SH;
            break;
          case LinxISA::ORWrr:
            NewOpc = LinxISA::ORWrr_SH;
            break;
          case LinxISA::XORWrr:
            NewOpc = LinxISA::XORWrr_SH;
            break;
          default:
            break;
          }
        }

        if (!NewOpc || BinMI.getNumOperands() < 3) {
          ++It;
          continue;
        }

        const Register BinDst = BinMI.getOperand(0).getReg();
        Register BinOp1 = BinMI.getOperand(1).getReg();
        Register BinOp2 = BinMI.getOperand(2).getReg();

        Register Other;
        if (BinOp1 == ShDst)
          Other = BinOp2;
        else if (BinOp2 == ShDst)
          Other = BinOp1;
        else {
          ++It;
          continue;
        }

        // SUB is not commutative; only fold when the shifted value is the RHS.
        const bool IsSub =
            (BinOpc == LinxISA::SUBrr) || (BinOpc == LinxISA::SUBWrr);
        if (IsSub && BinOp1 == ShDst) {
          ++It;
          continue;
        }

        // Ignore ShiftMI itself: register allocation may legally coalesce
        // `tmp` with `x`, yielding an in-place shift (e.g. `r3 = slli r3, k`).
        if (!hasSingleNonDbgUseInMBB(ShDst, &BinMI, &ShiftMI)) {
          ++It;
          continue;
        }

        MachineInstr *NewMI =
            BuildMI(MBB, BinMI.getIterator(), BinMI.getDebugLoc(),
                    TII.get(NewOpc), BinDst)
                .addReg(Other)
                .addReg(ShSrc)
                .addImm(ShAmt)
                .getInstr();
        BinMI.eraseFromParent();
        ShiftMI.eraseFromParent();
        Changed = true;
        It = std::next(NewMI->getIterator());
      }

      if (IsTileBlock) {
        // If the pass runs twice, strip any stale standard start marker(s)
        // and keep the tile header intact.
        auto It = MBB.begin();
        while (It != MBB.end() && It->isPHI())
          ++It;
        while (It != MBB.end() && isStdBStartOpcode(It->getOpcode())) {
          It = MBB.erase(It);
          Changed = true;
        }
      } else {
	      // Insert `BSTART.STD <kind>` after PHIs.
	      auto InsertBStart = MBB.begin();
	      while (InsertBStart != MBB.end() && InsertBStart->isPHI())
	        ++InsertBStart;

      // Remove any existing start marker (in case the pass runs twice).
      if (InsertBStart != MBB.end() &&
          (InsertBStart->getOpcode() == LinxISA::CBSTART_STD ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_FALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_DIRECT ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_COND ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_CALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_IND ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_ICALL ||
           InsertBStart->getOpcode() == LinxISA::BSTART_STD_RET)) {
        InsertBStart = MBB.erase(InsertBStart);
        Changed = true;
      }

	      MachineInstr *BStartMI = nullptr;
	      MachineInstr *SetRetMI = nullptr;
	      switch (Kind) {
      case ExitKind::Fall:
        // Prefer the compressed BrType marker: C.BSTART (FALL).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(1) // BrType = FALL
                       .getInstr();
        break;
      case ExitKind::Direct:
        if (TargetBB)
          TargetBB->setLabelMustBeEmitted();
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::BSTART_STD_DIRECT))
                       .addMBB(TargetBB)
                       .getInstr();
        break;
      case ExitKind::Cond:
        if (TargetBB)
          TargetBB->setLabelMustBeEmitted();
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::BSTART_STD_COND))
                       .addMBB(TargetBB)
                       .getInstr();
        break;
	      case ExitKind::Call: {
	        if (!CallTargetOp)
	          report_fatal_error("Linx: missing call target operand");
	        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
	                           TII.get(LinxISA::BSTART_STD_CALL))
	                       .add(*CallTargetOp)
	                       .getInstr();
	        // Set return target for the call (ra = PC + imm20<<1). The ISA
	        // requires SETRET to be immediately after the CALL BSTART header.
	        if (ReturnBB) {
	          ReturnBB->setLabelMustBeEmitted();
	          auto InsertSetRet = std::next(BStartMI->getIterator());
	          SetRetMI =
	              BuildMI(MBB, InsertSetRet, DebugLoc(), TII.get(LinxISA::SETRET))
	                  .addMBB(ReturnBB)
	                  .getInstr();
	        }
	        break;
	      }
      case ExitKind::Ret:
        // Prefer the compressed BrType marker: C.BSTART (RET).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(7) // BrType = RET
                       .getInstr();
        break;
      case ExitKind::Ind:
        // Prefer the compressed BrType marker: C.BSTART (IND).
        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
                           TII.get(LinxISA::CBSTART_STD))
                       .addImm(5) // BrType = IND
                       .getInstr();
        break;
	      case ExitKind::ICall:
	        // Prefer the compressed BrType marker: C.BSTART (ICALL).
	        BStartMI = BuildMI(MBB, InsertBStart, DebugLoc(),
	                           TII.get(LinxISA::CBSTART_STD))
	                       .addImm(6) // BrType = ICALL
	                       .getInstr();
	        // Indirect calls behave like CALL blocks but select the callee via
	        // SETC.TGT. Emit SETRET so the continuation block is reachable after
	        // the callee returns. SETRET must be immediately after the BSTART
	        // header.
	        if (ReturnBB) {
	          ReturnBB->setLabelMustBeEmitted();
	          auto InsertSetRet = std::next(BStartMI->getIterator());
	          SetRetMI =
	              BuildMI(MBB, InsertSetRet, DebugLoc(), TII.get(LinxISA::SETRET))
	                  .addMBB(ReturnBB)
	                  .getInstr();
	        }
	        break;
	      }
      Changed = true;

		      if (HeaderSetcTgtReg) {
		        auto InsertSetcTgt = std::next(BStartMI->getIterator());
		        if (SetRetMI)
		          InsertSetcTgt = std::next(SetRetMI->getIterator());
		        BuildMI(MBB, InsertSetcTgt, DebugLoc(), TII.get(LinxISA::CSETC_TGT))
		            .addReg(*HeaderSetcTgtReg);
		        Changed = true;
		      }

		      if (ICallSetcTgtReg) {
		        auto InsertSetcTgt = std::next(BStartMI->getIterator());
		        if (SetRetMI)
		          InsertSetcTgt = std::next(SetRetMI->getIterator());

		        auto InsertAfterCalleeDef = InsertSetcTgt;
		        for (auto It = InsertSetcTgt, E = MBB.instr_end(); It != E; ++It) {
		          if (It->isDebugInstr() || isMarkerInstr(*It))
		            continue;
		          if (It->modifiesRegister(*ICallSetcTgtReg, &TRI)) {
		            InsertAfterCalleeDef = std::next(It);
		          }
		        }

		        BuildMI(MBB, InsertAfterCalleeDef, DebugLoc(),
		                TII.get(LinxISA::CSETC_TGT))
		            .addReg(*ICallSetcTgtReg);
		        Changed = true;
		      }

	      // Assign block-local values to the hand queues.
	      //
	      // Hardware semantics: every definition to `t` shifts older values into
	      // `t#1..t#4` (similarly for `u` -> `u#1..u#4`). We only rewrite values
	      // whose uses occur within the next 4 queued defs for a chosen hand.
	      //
	      // Peepholes:
	      // - Remove redundant `addw X, zero, ->Y` when Y is used only by an
	      //   immediate SETC operation. This keeps the compare closer to the
	      //   original producer (often a 32-bit load), and exposes more values
	      //   as candidates for T/U-hand remapping.
	      struct UseSite {
	        MachineInstr *MI = nullptr;
	        unsigned OpNo = 0;
	        unsigned UseIdx = 0;
	      };
	      struct Segment {
	        Register Reg;
	        MachineInstr *DefMI = nullptr;
	        unsigned DefOpNo = 0;
	        unsigned DefIdx = 0;
	        SmallVector<UseSite, 4> Uses;
	        bool ClosedByRedef = false;
	        bool TouchesInlineAsm = false;
	      };

      SmallVector<Segment, 32> Segs;
      DenseMap<unsigned, unsigned> ActiveSeg; // PhysReg.id() -> Segs index
	      unsigned InstIdx = 0;

        auto getNextNonMarker = [&](MachineBasicBlock::iterator It)
            -> MachineBasicBlock::iterator {
          auto E = MBB.end();
          while (It != E) {
            if (!It->isDebugInstr() && !isMarkerInstr(*It))
              break;
            ++It;
          }
          return It;
        };

        auto isSetcImmediate = [&](const MachineInstr &MI) -> bool {
          switch (MI.getOpcode()) {
          case LinxISA::SETC_EQI:
          case LinxISA::SETC_NEI:
          case LinxISA::SETC_LTI:
          case LinxISA::SETC_GEI:
          case LinxISA::SETC_LTUI:
          case LinxISA::SETC_GEUI:
            return true;
          default:
            return false;
          }
        };

        auto hasAnyUseAfter = [&](Register Reg, MachineBasicBlock::iterator From)
            -> bool {
          for (auto It = From, E = MBB.end(); It != E; ++It) {
            MachineInstr &MI = *It;
            if (MI.isDebugInstr() || isMarkerInstr(MI))
              continue;
            for (const MachineOperand &MO : MI.operands()) {
              if (!MO.isReg() || MO.isImplicit())
                continue;
              if (MO.isUse() && MO.getReg() == Reg)
                return true;
            }
          }
          return false;
        };

        // Identify whether this block (MachineBasicBlock) contains any BlockISA
        // headers that may implicitly consume ABI argument/return registers
        // (CALL/ICALL/RET). If so, be conservative about remapping a0-a7 since
        // those uses are not modeled as explicit register operands on the block
        // header itself.
        //
        // Note: a single MachineBasicBlock can contain multiple BlockISA blocks
        // (markers like C.BSTART ...). We must therefore scan all marker
        // instructions, not just the first header.
        auto markerHasImplicitAbiUses = [&](const MachineInstr &MI) -> bool {
          const unsigned Opc = MI.getOpcode();
          if (Opc == LinxISA::BSTART_STD_CALL || Opc == LinxISA::BSTART_STD_ICALL ||
              Opc == LinxISA::BSTART_STD_RET) {
            return true;
          }
          if (Opc == LinxISA::CBSTART_STD && MI.getNumOperands() >= 1 &&
              MI.getOperand(0).isImm()) {
            const int64_t BrType = MI.getOperand(0).getImm() & 0x7;
            return BrType == 4 /*CALL*/ || BrType == 6 /*ICALL*/ ||
                   BrType == 7 /*RET*/;
          }
          return false;
        };

        bool BlockHasImplicitAbiUses = false;
        for (const MachineInstr &MI : MBB) {
          if (!isMarkerInstr(MI))
            continue;
          if (markerHasImplicitAbiUses(MI)) {
            BlockHasImplicitAbiUses = true;
            break;
          }
        }

        // Peephole: ADDWrr dst, src, zero; SETC_*I dst, imm  ==> SETC_*I src, imm
        for (auto It = MBB.begin(), E = MBB.end(); It != E;) {
          MachineInstr &MI = *It;
          ++It;
          if (MI.isDebugInstr() || isMarkerInstr(MI))
            continue;
          if (MI.getOpcode() != LinxISA::ADDWrr)
            continue;
          if (MI.getNumOperands() < 3)
            continue;
          if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef())
            continue;
          if (!MI.getOperand(1).isReg() || !MI.getOperand(2).isReg())
            continue;
          const Register Dst = MI.getOperand(0).getReg();
          const Register A = MI.getOperand(1).getReg();
          const Register B = MI.getOperand(2).getReg();
          if (!Dst.isPhysical())
            continue;

          Register Src;
          if (A == LinxISA::R0 && B != LinxISA::R0)
            Src = B;
          else if (B == LinxISA::R0 && A != LinxISA::R0)
            Src = A;
          else
            continue;

          auto NextIt = getNextNonMarker(MI.getIterator());
          if (NextIt == E)
            continue;
          // NextIt currently points to MI itself; advance one.
          NextIt = getNextNonMarker(std::next(MI.getIterator()));
          if (NextIt == E)
            continue;
          MachineInstr &NextMI = *NextIt;
          if (!isSetcImmediate(NextMI))
            continue;
          if (NextMI.getNumOperands() < 2 || !NextMI.getOperand(0).isReg())
            continue;
          if (NextMI.getOperand(0).getReg() != Dst)
            continue;

          // If Dst is used again later, keep the ADDW.
          if (hasAnyUseAfter(Dst, std::next(NextIt)))
            continue;

          NextMI.getOperand(0).setReg(Src);
          MI.eraseFromParent();
          Changed = true;
        }

		      auto isCandidatePhysReg = [&](Register Reg) -> bool {
		        if (!Reg || !Reg.isPhysical())
		          return false;
            // Only remap architectural GPR values. Never rewrite non-GPR
            // physical registers (e.g. tile regs) into the T/U hand queues.
            if (!LinxISA::GPRRegClass.contains(Reg))
              return false;
		        if (Reg.id() >= Reserved.size())
	          return false;
		        if (Reserved.test(Reg.id()))
		          return false;
	        // The a0-a7 argument registers are ABI-visible at CALL/ICALL/RET
	        // boundaries. Only allow remapping them inside blocks that do not
	        // implicitly consume ABI regs.
	        if (BlockHasImplicitAbiUses) {
	          switch (Reg) {
	          case LinxISA::R2:
	          case LinxISA::R3:
	          case LinxISA::R4:
	          case LinxISA::R5:
	          case LinxISA::R6:
	          case LinxISA::R7:
	          case LinxISA::R8:
	          case LinxISA::R9:
	            return false;
	          default:
	            break;
	          }
	        }
	        return true;
	      };

      for (MachineInstr &MI : MBB) {
        if (MI.isDebugInstr() || isMarkerInstr(MI))
          continue;

        // Process uses before defs to handle read-modify-write forms.
        for (unsigned OpNo = 0; OpNo < MI.getNumOperands(); ++OpNo) {
          MachineOperand &MO = MI.getOperand(OpNo);
          if (!MO.isReg() || MO.isImplicit() || MO.isDef())
            continue;

          Register Reg = MO.getReg();
          if (!isCandidatePhysReg(Reg))
            continue;

          auto It = ActiveSeg.find(Reg.id());
          if (It == ActiveSeg.end())
            continue;

	          Segment &S = Segs[It->second];
	          S.Uses.push_back(UseSite{&MI, OpNo, InstIdx});
	          if (MI.isInlineAsm())
	            S.TouchesInlineAsm = true;
	        }

        for (unsigned OpNo = 0; OpNo < MI.getNumOperands(); ++OpNo) {
          MachineOperand &MO = MI.getOperand(OpNo);
          if (!MO.isReg() || MO.isImplicit() || !MO.isDef())
            continue;

          Register Reg = MO.getReg();
          if (!isCandidatePhysReg(Reg))
            continue;

          // Close the previous segment (if any) for this physical register.
          auto It = ActiveSeg.find(Reg.id());
          if (It != ActiveSeg.end()) {
            Segs[It->second].ClosedByRedef = true;
            ActiveSeg.erase(It);
          }

          Segment S;
          S.Reg = Reg;
          S.DefMI = &MI;
          S.DefOpNo = OpNo;
          S.DefIdx = InstIdx;
          S.TouchesInlineAsm = MI.isInlineAsm();
          ActiveSeg[Reg.id()] = Segs.size();
          Segs.push_back(S);
        }

        ++InstIdx;
      }

	      SmallVector<unsigned, 32> CandidateSegs;
	      CandidateSegs.reserve(Segs.size());
	      for (unsigned I = 0; I < Segs.size(); ++I) {
	        const Segment &S = Segs[I];
	        if (!S.DefMI || S.Uses.empty())
	          continue;
	        // Never remap values that touch inline asm. Inline asm operand
	        // constraints expect architectural registers; rewriting defs/uses to
	        // the T/U hand queues breaks the ABI-visible semantics (notably
	        // syscall/ACR entry/exit sequences).
	        if (S.TouchesInlineAsm)
	          continue;
	        unsigned LastUseIdx = 0;
	        for (const UseSite &U : S.Uses)
	          LastUseIdx = std::max(LastUseIdx, U.UseIdx);
	        if (LastUseIdx <= S.DefIdx)
	          continue;
	        // If the value is live-out, we can't remap it to the hand queue.
	        if (!S.ClosedByRedef && isPhysRegLiveOutOfBlock(S.Reg))
	          continue;
	        CandidateSegs.push_back(I);
	      }

      if (!CandidateSegs.empty()) {
        enum class Hand : uint8_t { None, T, U };

        auto isTCompressibleDef = [&](const MachineInstr &MI) -> bool {
          switch (MI.getOpcode()) {
          case LinxISA::ADDrr:
          case LinxISA::SUBrr:
          case LinxISA::ANDrr:
          case LinxISA::ORrr:
            return true;
          case LinxISA::ADDIri:
          case LinxISA::SUBIri: {
            if (MI.getNumOperands() >= 3 && MI.getOperand(2).isImm())
              return isInt<5>(MI.getOperand(2).getImm());
            return false;
          }
          case LinxISA::LWI:
          case LinxISA::LDI: {
            if (MI.getNumOperands() >= 3 && MI.getOperand(2).isImm())
              return isInt<5>(MI.getOperand(2).getImm());
            return false;
          }
          default:
            return false;
          }
        };

        // Greedy assignment in reverse def order. For each candidate, choose a
        // hand where the value is still within the 4-deep queue at its use.
        SmallVector<unsigned, 32> Sorted = CandidateSegs;
        llvm::sort(Sorted, [&](unsigned A, unsigned B) {
          return Segs[A].DefIdx > Segs[B].DefIdx;
        });

	        SmallVector<unsigned, 32> AssignedT;
	        SmallVector<unsigned, 32> AssignedU;
	        SmallVector<Hand, 32> AssignedHand(Segs.size(), Hand::None);
	        DenseMap<const MachineInstr *, unsigned> UsedHandReads; // bit0=T, bit1=U
	
	        auto countBetweenAt = [&](ArrayRef<unsigned> Assigned,
	                                  const Segment &S, unsigned UseIdx) {
	          unsigned Between = 0;
	          for (unsigned J : Assigned) {
	            const Segment &B = Segs[J];
	            if (B.DefIdx > S.DefIdx && B.DefIdx < UseIdx)
	              ++Between;
	          }
	          return Between;
	        };
	
	        auto getLastUseIdx = [&](const Segment &S) -> unsigned {
	          unsigned LastUseIdx = 0;
	          for (const UseSite &U : S.Uses)
	            LastUseIdx = std::max(LastUseIdx, U.UseIdx);
	          return LastUseIdx;
	        };
	
	        auto hasMultiUseInSameMI = [&](const Segment &S) -> bool {
	          SmallPtrSet<const MachineInstr *, 4> Seen;
	          for (const UseSite &U : S.Uses) {
	            if (!Seen.insert(U.MI).second)
	              return true;
	          }
	          return false;
	        };
	
	        auto canReadHandInAllUses = [&](Hand H, const Segment &S) -> bool {
	          const unsigned Bit = (H == Hand::T) ? 0x1u : 0x2u;
	          for (const UseSite &U : S.Uses) {
	            const unsigned Mask = UsedHandReads.lookup(U.MI);
	            if ((Mask & Bit) != 0)
	              return false;
	          }
	          return true;
	        };
	
	        for (unsigned I : Sorted) {
	          const Segment &S = Segs[I];
	          const unsigned LastUseIdx = getLastUseIdx(S);
	          unsigned BetweenT = countBetweenAt(AssignedT, S, LastUseIdx);
	          unsigned BetweenU = countBetweenAt(AssignedU, S, LastUseIdx);
	
	          // Per-queue port rule: at most one T read and one U read per
	          // instruction. Avoid mapping multiple operands in the same MI to the
	          // same hand (even if they are the same physical register).
	          if (hasMultiUseInSameMI(S))
	            continue;
	
	          const bool CanT = BetweenT <= 3 && canReadHandInAllUses(Hand::T, S);
	          const bool CanU = BetweenU <= 3 && canReadHandInAllUses(Hand::U, S);
	          if (!CanT && !CanU)
	            continue;
	
	          Hand H = Hand::None;

	          // Prefer mapping defs that can become 16-bit ops to the T-hand.
	          const bool PreferT = isTCompressibleDef(*S.DefMI);

	          if (PreferT && CanT) {
	            H = Hand::T;
	          } else if (CanT && CanU) {
	            H = (BetweenT <= BetweenU) ? Hand::T : Hand::U;
	          } else if (CanT) {
	            H = Hand::T;
	          } else {
	            H = Hand::U;
	          }
	
	          AssignedHand[I] = H;
	          if (H == Hand::T)
	            AssignedT.push_back(I);
	          else if (H == Hand::U)
	            AssignedU.push_back(I);
	
	          const unsigned Bit = (H == Hand::T) ? 0x1u : 0x2u;
	          for (const UseSite &U : S.Uses)
	            UsedHandReads[U.MI] |= Bit;
	        }
	
	        for (unsigned I : Sorted) {
	          const Segment &S = Segs[I];
	          Hand H = AssignedHand[I];
	          if (H == Hand::None)
	            continue;
	
	          MachineOperand &DefMO = S.DefMI->getOperand(S.DefOpNo);
	          DefMO.setReg(H == Hand::T ? LinxISA::U4 : LinxISA::U3); // "->t"/"->u"
	
		          for (const UseSite &U : S.Uses) {
		            const unsigned Between = (H == Hand::T)
		                                         ? countBetweenAt(AssignedT, S, U.UseIdx)
		                                         : countBetweenAt(AssignedU, S, U.UseIdx);
		            const unsigned Index = Between + 1;
		            Register UseReg = (H == Hand::T) ? getTQueueUseReg(Index)
		                                             : getUQueueUseReg(Index);
		            if (!UseReg)
		              continue;
		            MachineOperand &UseMO = U.MI->getOperand(U.OpNo);
		            UseMO.setReg(UseReg); // "t#k"/"u#k"
		          }
		          Changed = true;
		        }
			      }
	      } // end !IsTileBlock

	      // Post-remap peephole: use block-private T-hand for simple SETC
	      // conditions that consume a single-use PC-relative load result.
	      //
	      // This improves code size and scheduling by keeping the loaded value in
	      // the block-private queue:
	      //   lw.pcr [sym], ->aX
	      //   setc.*i aX, imm
	      // =>
	      //   lw.pcr [sym], ->t
	      //   setc.*i t#1, imm
	      auto isPcrLoadOpc = [&](unsigned Opc) -> bool {
	        switch (Opc) {
	        case LinxISA::LB_PCR:
	        case LinxISA::LBU_PCR:
	        case LinxISA::LH_PCR:
	        case LinxISA::LHU_PCR:
	        case LinxISA::LW_PCR:
	        case LinxISA::LWU_PCR:
	        case LinxISA::LD_PCR:
	        case LinxISA::HL_LB_PCR:
	        case LinxISA::HL_LBU_PCR:
	        case LinxISA::HL_LH_PCR:
	        case LinxISA::HL_LHU_PCR:
	        case LinxISA::HL_LW_PCR:
	        case LinxISA::HL_LWU_PCR:
	        case LinxISA::HL_LD_PCR:
	          return true;
	        default:
	          return false;
	        }
	      };
	      auto isSetcImmOpcode = [&](unsigned Opc) -> bool {
	        switch (Opc) {
	        case LinxISA::SETC_EQI:
	        case LinxISA::SETC_NEI:
	        case LinxISA::SETC_LTI:
	        case LinxISA::SETC_GEI:
	        case LinxISA::SETC_LTUI:
	        case LinxISA::SETC_GEUI:
	        case LinxISA::SETC_ANDI:
	        case LinxISA::SETC_ORI:
	        case LinxISA::HLSETC_ANDI:
	        case LinxISA::HLSETC_ORI:
	          return true;
	        default:
	          return false;
	        }
	      };
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &LdMI = *It;
	        if (LdMI.isDebugInstr() || isMarkerInstr(LdMI)) {
	          ++It;
	          continue;
	        }
	        if (!isPcrLoadOpc(LdMI.getOpcode()) || LdMI.getNumOperands() < 2 ||
	            !LdMI.getOperand(0).isReg() || !LdMI.getOperand(0).isDef()) {
	          ++It;
	          continue;
	        }
	        const Register Dst = LdMI.getOperand(0).getReg();
	        if (!Dst.isPhysical() || Dst == LinxISA::U4 || Dst == LinxISA::U3) {
	          ++It;
	          continue;
	        }
	        auto NextIt = std::next(It);
	        while (NextIt != MBB.end() &&
	               (NextIt->isDebugInstr() || isMarkerInstr(*NextIt)))
	          ++NextIt;
	        if (NextIt == MBB.end()) {
	          ++It;
	          continue;
	        }
	        MachineInstr &SetcMI = *NextIt;
	        if (!isSetcImmOpcode(SetcMI.getOpcode()) || SetcMI.getNumOperands() < 2 ||
	            !SetcMI.getOperand(0).isReg() || SetcMI.getOperand(0).getReg() != Dst) {
	          ++It;
	          continue;
	        }

	        if (!hasSingleNonDbgUseInMBB(Dst, &SetcMI, &LdMI)) {
	          ++It;
	          continue;
	        }
	        // The T/U hand queues are block-private: values pushed into the queue
	        // do not survive control-flow edges. Only rewrite when the loaded
	        // value is guaranteed not to be live-out of this MachineBasicBlock.
	        if (isPhysRegLiveOutOfBlock(Dst)) {
	          ++It;
	          continue;
	        }

	        LdMI.getOperand(0).setReg(LinxISA::U4);   // "->t"
	        SetcMI.getOperand(0).setReg(LinxISA::T1); // "t#1"
	        Changed = true;
	        It = std::next(SetcMI.getIterator());
	      }

	      // Post-remap peephole: use 16-bit C.ZEXT.* when extracting low bits into
	      // the T-hand implicit destination.
	      //
	      // The earlier shift-folding peephole produces `BXU src, M=0, N=<width>`.
	      // When the result is block-private (`->t`), we can encode common widths
	      // with compressed zext forms.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::BXU || MI.getNumOperands() < 4) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
	            !MI.getOperand(3).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) { // compressed form writes implicit `->t`
	          ++It;
	          continue;
	        }

	        const Register Src = MI.getOperand(1).getReg();
	        const int64_t Imml = MI.getOperand(2).getImm();
	        const int64_t Imms = MI.getOperand(3).getImm();
	        if (Imms != 0) {
	          ++It;
	          continue;
	        }

	        unsigned NewOpc = 0;
	        if (Imml == 7)
	          NewOpc = LinxISA::C_ZEXT_B;
	        else if (Imml == 15)
	          NewOpc = LinxISA::C_ZEXT_H;
	        else if (Imml == 31)
	          NewOpc = LinxISA::C_ZEXT_W;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(NewOpc), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Post-remap peephole: compress common 32->64 sign-extends into C.SEXT.W
	      // when the destination is the T-hand implicit register.
	      //
	      // `addw src, zero, ->t` is a common legalization pattern for `sext.w`
	      // (truncate to 32 and sign-extend back to 64). Prefer the 16-bit
	      // encoding when block-private.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::ADDWrr || MI.getNumOperands() < 3) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isReg()) {
	          ++It;
	          continue;
	        }
	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) {
	          ++It;
	          continue;
	        }
	        Register Src = Register();
	        const Register A = MI.getOperand(1).getReg();
	        const Register B = MI.getOperand(2).getReg();
	        if (A == LinxISA::R0 && B != LinxISA::R0)
	          Src = B;
	        else if (B == LinxISA::R0 && A != LinxISA::R0)
	          Src = A;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(LinxISA::C_SEXT_W), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Post-remap peephole: use 16-bit C.SEXT.* when sign-extending low bits
	      // into the T-hand implicit destination.
	      //
	      // The earlier shift-folding peephole may produce `BXS src, M=0, N=<width>`.
	      // When the result is block-private (`->t`), we can encode common widths
	      // with compressed sext forms.
	      for (auto It = MBB.begin(); It != MBB.end();) {
	        MachineInstr &MI = *It;
	        if (MI.isDebugInstr() || isMarkerInstr(MI)) {
	          ++It;
	          continue;
	        }
	        if (MI.getOpcode() != LinxISA::BXS || MI.getNumOperands() < 4) {
	          ++It;
	          continue;
	        }
	        if (!MI.getOperand(0).isReg() || !MI.getOperand(0).isDef() ||
	            !MI.getOperand(1).isReg() || !MI.getOperand(2).isImm() ||
	            !MI.getOperand(3).isImm()) {
	          ++It;
	          continue;
	        }

	        const Register Dst = MI.getOperand(0).getReg();
	        if (Dst != LinxISA::U4) { // compressed form writes implicit `->t`
	          ++It;
	          continue;
	        }

	        const Register Src = MI.getOperand(1).getReg();
	        const int64_t Imml = MI.getOperand(2).getImm();
	        const int64_t Imms = MI.getOperand(3).getImm();
	        if (Imms != 0) {
	          ++It;
	          continue;
	        }

	        unsigned NewOpc = 0;
	        if (Imml == 7)
	          NewOpc = LinxISA::C_SEXT_B;
	        else if (Imml == 15)
	          NewOpc = LinxISA::C_SEXT_H;
	        else if (Imml == 31)
	          NewOpc = LinxISA::C_SEXT_W;
	        else {
	          ++It;
	          continue;
	        }

	        MachineInstr *NewMI =
	            BuildMI(MBB, It, MI.getDebugLoc(), TII.get(NewOpc), Dst)
	                .addReg(Src)
	                .getInstr();
	        MI.eraseFromParent();
	        It = std::next(NewMI->getIterator());
	        Changed = true;
	      }

	      // Insert `BSTOP` only for the final laid-out block. When a `BSTART.*`
	      // follows, it already terminates the previous block.
	      auto InsertBStop = MBB.end();
      while (InsertBStop != MBB.begin() && std::prev(InsertBStop)->isDebugInstr())
        --InsertBStop;

      if (MBB.getNextNode()) {
        if (InsertBStop != MBB.begin() &&
            std::prev(InsertBStop)->getOpcode() == LinxISA::BSTOP) {
          std::prev(InsertBStop)->eraseFromParent();
          Changed = true;
        }
      } else {
        if (InsertBStop == MBB.begin() ||
            std::prev(InsertBStop)->getOpcode() != LinxISA::BSTOP) {
          BuildMI(MBB, InsertBStop, DebugLoc(), TII.get(LinxISA::BSTOP));
          Changed = true;
        }
      }
    }

    // Jump tables take the address of their destination blocks. Ensure the
    // targets' labels are emitted even if they are fallthrough blocks with no
    // direct CFG edge pointing at them.
    if (const MachineJumpTableInfo *JTI = MF.getJumpTableInfo()) {
      for (const MachineJumpTableEntry &Entry : JTI->getJumpTables()) {
        for (MachineBasicBlock *JTBB : Entry.MBBs) {
          if (!JTBB || JTBB->hasLabelMustBeEmitted())
            continue;
          JTBB->setLabelMustBeEmitted();
          Changed = true;
        }
      }
    }

    return Changed;
  }
};

} // end anonymous namespace

char LinxISABlockify::ID = 0;

INITIALIZE_PASS(LinxISABlockify, "linx-blockify", "Linx Blockify", false,
                false)

FunctionPass *llvm::createLinxISABlockifyPass() { return new LinxISABlockify(); }
