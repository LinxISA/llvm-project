//===-- LinxV5TileDimension.h - Tile shape values -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5TILEDIMENSION_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5TILEDIMENSION_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include <tuple>
#include <vector>

namespace llvm {
namespace LinxV5 {

// Identity is independent of the physical register currently holding a value.
// A spill/reload transports this identity; it does not create a new shape.
struct TileDimensionValue {
  // Reg identifies the original scalar definition, not its current location.
  // Definition names the original instruction. Merge names a scalar PHI at
  // block entry; keeping it separate prevents collisions with real defs.
  Register Reg;
  const MachineInstr *Definition = nullptr;
  Optional<unsigned> Constant;
  const MachineBasicBlock *Merge = nullptr;

  bool operator==(const TileDimensionValue &Other) const {
    if (Constant || Other.Constant)
      return Constant && Other.Constant && *Constant == *Other.Constant;
    return Reg == Other.Reg && Definition == Other.Definition &&
           Merge == Other.Merge;
  }
  bool operator!=(const TileDimensionValue &Other) const {
    return !(*this == Other);
  }
};

// One analysis instance is valid only while the machine function is unchanged.
// Queries build a dependency graph and solve it to a fixed point, including
// loops. Bottom (a backedge not yet evaluated) is distinct from an unknown or
// conflicting value. A cycle with no external definition remains unproved.
class TileDimensionValueAnalysis {
  using Key = std::tuple<MachineBasicBlock *, const MachineInstr *, unsigned,
                         unsigned>; // register or private spill frame index
  struct Node {
    enum Kind { Bottom, Known, Unknown } State = Bottom;
    TileDimensionValue Value;
    Optional<TileDimensionValue> MergedValue;
    SmallVector<unsigned, 2> Dependencies;
    SmallVector<unsigned, 2> Users;
  };
  MachineFunction &MF;
  DenseMap<Key, unsigned> Indices;
  std::vector<Node> Nodes;
  unsigned getNode(MachineBasicBlock &MBB, MachineBasicBlock::iterator Before,
                   unsigned Location, bool Stack);
  void solve();

public:
  explicit TileDimensionValueAnalysis(MachineFunction &MF) : MF(MF) {}
  Optional<TileDimensionValue> resolve(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator Before,
                                       Register Reg);
  Optional<TileDimensionValue> resolveSpill(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator Before,
                                            int FrameIndex);
};

} // namespace LinxV5
} // namespace llvm
#endif
