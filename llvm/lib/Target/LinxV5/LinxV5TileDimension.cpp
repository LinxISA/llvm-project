//===-- LinxV5TileDimension.cpp - Tile shape values
//-------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LinxV5TileDimension.h"
#include "LinxV5.h"
#include "LinxV5InstrInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/IR/InlineAsm.h"
#include <deque>

using namespace llvm;
using namespace llvm::LinxV5;

// Only whole, non-escaping scalar spill slots are transparent. In particular,
// ordinary loads and arbitrary stack addresses are not treated as value copies.
static Optional<int> privateScalarSpill(const MachineInstr &MI) {
  if (MI.memoperands().size() != 1)
    return None;
  const MachineMemOperand &MMO = **MI.memoperands_begin();
  const auto *PSV =
      dyn_cast_or_null<FixedStackPseudoSourceValue>(MMO.getPseudoValue());
  if (!PSV || !MMO.isUnordered() || MMO.getOffset() != 0 || MMO.getSize() != 8)
    return None;
  int FI = PSV->getFrameIndex();
  const auto &MFI = MI.getMF()->getFrameInfo();
  if (!MFI.isSpillSlotObjectIndex(FI) || MFI.isAliasedObjectIndex(FI) ||
      MFI.getObjectSize(FI) != 8)
    return None;
  return FI;
}

unsigned TileDimensionValueAnalysis::getNode(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator Before,
                                             unsigned Location, bool Stack) {
  Key K{&MBB, Before == MBB.end() ? nullptr : &*Before, Location, Stack};
  auto Inserted = Indices.try_emplace(K, Nodes.size());
  if (!Inserted.second)
    return Inserted.first->second;
  unsigned Index = Inserted.first->second;
  Nodes.emplace_back();
  auto Unknown = [&]() { Nodes[Index].State = Node::Unknown; };
  auto Known = [&](TileDimensionValue V) {
    Nodes[Index].State = Node::Known;
    Nodes[Index].Value = V;
  };
  auto Depend = [&](MachineBasicBlock &B, MachineBasicBlock::iterator I,
                    unsigned L, bool S) {
    unsigned D = getNode(B, I, L, S);
    Nodes[Index].Dependencies.push_back(D);
    Nodes[D].Users.push_back(Index);
  };
  if (Nodes.size() > 65536) {
    Unknown();
    return Index;
  }
  Register Reg(Location);
  if (!Stack && Reg == R0) {
    Known({Register(), nullptr, 0u});
    return Index;
  }
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  if (!Stack && Reg.isVirtual() && MF.getRegInfo().isSSA()) {
    MachineInstr *Def = MF.getRegInfo().getVRegDef(Reg);
    if (!Def) {
      Unknown();
      return Index;
    }
    auto After = std::next(Def->getIterator());
    if (&MBB != Def->getParent() || Before != After) {
      Depend(*Def->getParent(), After, Location, false);
      return Index;
    }
  }
  for (MachineInstr &MI : reverse(make_range(MBB.begin(), Before))) {
    if (Stack) {
      if (!MI.mayStore() && !MI.isCall() && !MI.hasUnmodeledSideEffects())
        continue;
      auto FI = privateScalarSpill(MI);
      if (FI && static_cast<unsigned>(*FI) == Location) {
        if ((MI.getOpcode() == SD || MI.getOpcode() == SDI ||
             MI.getOpcode() == SDI_U) &&
            MI.getOperand(0).isReg()) {
          Depend(MBB, MI.getIterator(), MI.getOperand(0).getReg(), false);
        } else
          Unknown();
        return Index;
      }
      // Calls/ordinary memory accesses cannot name a non-escaping spill slot.
      // Stack or unknown machine memory writes can, and invalidate the proof.
      bool IsTileAsm =
          MI.isInlineAsm() &&
          StringRef(MI.getOperand(InlineAsm::MIOp_AsmString).getSymbolName())
              .startswith("BSTART.");
      bool MayOverlap =
          MI.mayStore() && MI.memoperands_empty() && !IsTileAsm && !MI.isCall();
      for (const MachineMemOperand *MMO : MI.memoperands()) {
        if (!MMO->isStore())
          continue;
        const auto *PSV = MMO->getPseudoValue();
        if (const auto *FS = dyn_cast_or_null<FixedStackPseudoSourceValue>(PSV))
          MayOverlap |= static_cast<unsigned>(FS->getFrameIndex()) == Location;
        else if (PSV)
          MayOverlap |= PSV->isStack();
      }
      if (MayOverlap) {
        Unknown();
        return Index;
      }
      continue;
    }
    if (!MI.modifiesRegister(Reg, TRI))
      continue;
    if (MI.isCopy() || (MI.getOpcode() == ORI && MI.getOperand(2).isImm() &&
                        MI.getOperand(2).getImm() == 0)) {
      Depend(MBB, MI.getIterator(), MI.getOperand(1).getReg(), false);
    } else if (MI.getOpcode() == ADDI && MI.getOperand(1).getReg() == R0 &&
               isUInt<16>(MI.getOperand(2).getImm())) {
      Known({Register(), nullptr,
             static_cast<unsigned>(MI.getOperand(2).getImm())});
    } else if ((MI.getOpcode() == LD || MI.getOpcode() == LDI ||
                MI.getOpcode() == LDI_U) &&
               privateScalarSpill(MI)) {
      Depend(MBB, MI.getIterator(), *privateScalarSpill(MI), true);
    } else if (!MI.isCall() && !MI.isImplicitDef() &&
               MI.definesRegister(Reg, TRI)) {
      Known({Reg, &MI, None});
    } else {
      Unknown();
    }
    return Index;
  }
  if (MBB.pred_empty()) {
    if (Stack)
      Unknown();
    else
      Known({Reg, nullptr, None});
    return Index;
  }
  if (!Stack && MBB.pred_size() > 1)
    Nodes[Index].MergedValue = TileDimensionValue{Reg, nullptr, None, &MBB};
  for (MachineBasicBlock *Pred : MBB.predecessors())
    Depend(*Pred, Pred->end(), Location, Stack);
  return Index;
}

void TileDimensionValueAnalysis::solve() {
  std::deque<unsigned> Work;
  SmallVector<bool, 32> Queued(Nodes.size(), true);
  for (unsigned I = 0; I != Nodes.size(); ++I)
    Work.push_back(I);
  while (!Work.empty()) {
    unsigned I = Work.front();
    Work.pop_front();
    Queued[I] = false;
    Node &N = Nodes[I];
    if (N.State == Node::Unknown || N.Dependencies.empty())
      continue;
    auto State = Node::Bottom;
    TileDimensionValue Value;
    bool Different = false;
    for (unsigned D : N.Dependencies) {
      const Node &Input = Nodes[D];
      if (Input.State == Node::Unknown) {
        State = Node::Unknown;
        break;
      }
      if (Input.State == Node::Known) {
        Different |= State == Node::Known && Input.Value != Value;
        State = Node::Known;
        Value = Input.Value;
      }
    }
    if (State != Node::Unknown &&
        (Different || (N.MergedValue && N.State == Node::Known &&
                       N.Value == *N.MergedValue))) {
      // A register merge is a scalar PHI value, not an unknown clobber.
      // Its identity is the block entry, independent of later spill/reloads.
      // Once widened to a PHI, retain it to make cyclic equations monotone.
      // Memory merges cannot invent a value when stores disagree.
      if (N.MergedValue)
        Value = *N.MergedValue;
      else
        State = Node::Unknown;
    }
    if (State == N.State && (State != Node::Known || Value == N.Value))
      continue;
    N.State = State;
    N.Value = Value;
    for (unsigned U : N.Users)
      if (!Queued[U]) {
        Work.push_back(U);
        Queued[U] = true;
      }
  }
}

Optional<TileDimensionValue> TileDimensionValueAnalysis::resolve(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Before, Register Reg) {
  unsigned I = getNode(MBB, Before, Reg, false);
  solve();
  return Nodes[I].State == Node::Known
             ? Optional<TileDimensionValue>(Nodes[I].Value)
             : None;
}

Optional<TileDimensionValue>
TileDimensionValueAnalysis::resolveSpill(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator Before,
                                         int FrameIndex) {
  unsigned I = getNode(MBB, Before, FrameIndex, true);
  solve();
  return Nodes[I].State == Node::Known
             ? Optional<TileDimensionValue>(Nodes[I].Value)
             : None;
}
