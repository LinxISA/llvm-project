//===---- LinxV5ISelDAGToDAG.h - A dag to dag inst selector for LinxV5 ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the LinxV5 target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV5_LINXV5ISELDAGTODAG_H
#define LLVM_LIB_TARGET_LINXV5_LINXV5ISELDAGTODAG_H

#include "LinxV5.h"
#include "LinxV5TargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

// LinxV5-specific code to select LinxV5 machine instructions for
// SelectionDAG operations.
namespace llvm {
class LinxV5DAGToDAGISel : public SelectionDAGISel {
  const LinxV5Subtarget *Subtarget = nullptr;
  DenseMap<SDNode *, bool> XDivergenceMap;

public:
  explicit LinxV5DAGToDAGISel(LinxV5TargetMachine &TargetMachine)
      : SelectionDAGISel(TargetMachine) {}

  StringRef getPassName() const override {
    return "LinxV5 DAG->DAG Pattern Instruction Selection";
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<LinxV5Subtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  SDValue findLC0AndBuildNewTree(SDValue N, SDValue &LC0, unsigned Scale);
  // TODO: remove this after 0.54
  SDValue findBaseAndBuildNewTree(SDValue N, SDValue &Uniform);
  void reformContinuousAddr(SDValue N, unsigned Scale);
  void reformContinuousAddrs();
  SDValue combineSHL(SDValue N);
  void reformSIMTAddr(SDValue N, unsigned Scale);
  void reformSIMTAddrs();
  bool isXUniform(SDNode *N) { return !isXDivergent(N); }
  // Is N divergent on x-axis only
  bool isXDivergent(SDNode *N);
  bool calculateXDivergence(SDNode *N);
  void updateXDivergence(SDNode *N);
  void analyzeXDivergence();
  void PreprocessISelDAG() override;
  void Select(SDNode *Node) override;

  bool SelectInlineAsmMemoryOperand(const SDValue &Op, unsigned ConstraintID,
                                    std::vector<SDValue> &OutOps) override;

  bool SelectAddrFI(SDValue Addr, SDValue &Base);
  template <unsigned Scale>
  bool SelectContinuousADDRBase(SDValue N, SDValue &Addr);
  template <unsigned Scale>
  bool SelectContinuousLoadADDRrr(SDValue N, SDValue &SrcL, SDValue &SrcR,
                                  SDValue &SrcRType, SDValue &Shamt);
  template <unsigned Scale>
  bool SelectContinuousStoreADDRrr(SDValue N, SDValue &SrcL, SDValue &SrcR,
                                   SDValue &SrcRType, SDValue &ShamtImm);
  template <unsigned Scale>
  bool SelectContinuousADDRri(SDValue N, SDValue &SrcL, SDValue &Imm);
  bool SelectSIMTLoadADDRrr(SDValue N, SDValue &SrcL, SDValue &SrcR,
                            SDValue &SrcRType, SDValue &Shamt);
  template <unsigned Shamt>
  bool SelectSIMTStoreADDRrr_scaled(SDValue N, SDValue &SrcL, SDValue &SrcR,
                                    SDValue &SrcRType, SDValue &ShamtImm);
  bool SelectSIMTStoreADDRrr_unscaled(SDValue N, SDValue &SrcL, SDValue &SrcR,
                                      SDValue &SrcRType, SDValue &ShamtImm);
  template <LinxV5Op::SrcRType RType>
  bool SelectLoadADDRrr(SDValue N, SDValue &SrcL, SDValue &SrcR,
                        SDValue &Shamt);
  template <LinxV5Op::SrcRType RType, unsigned Shamt>
  bool SelectStoreADDRrr_scaled(SDValue N, SDValue &SrcL, SDValue &SrcR);
  template <LinxV5Op::SrcRType RType>
  bool SelectStoreADDRrr_unscaled(SDValue N, SDValue &SrcL, SDValue &SrcR);
  template <LinxV5Op::SrcRType RType>
  bool SelectSrcREXT(SDValue N, SDValue &SrcR);
  template <unsigned Shamt>
  bool SelectADDRri_scaled(SDValue N, SDValue &SrcL, SDValue &Imm);
  bool SelectADDRri_unscaled(SDValue N, SDValue &SrcL, SDValue &Imm);
  bool SelectBXU(SDValue N, SDValue &SrcL, SDValue &ImmM, SDValue &ImmN);
  bool SelectBIC(SDValue N, SDValue &SrcL, SDValue &ImmM, SDValue &ImmN);
  bool SelectBIS(SDValue N, SDValue &SrcL, SDValue &ImmM, SDValue &ImmN);
  bool SelectBXUFromAnd(SDValue N, SDValue &SrcL, SDValue &ImmM, SDValue &ImmN);
  bool SelectBXUFromShr(SDValue N, SDValue &SrcL, SDValue &ImmM, SDValue &ImmN);
  bool SelectBXUFromShrAnd(SDValue N, SDValue &SrcL, SDValue &ImmM,
                           SDValue &ImmN);
  template <bool Signed>
  bool SelectBXFromShrShl(SDValue N, SDValue &SrcL, SDValue &ImmM,
                          SDValue &ImmN);

  void selectLoadStackGuard(SDNode *Node);

  void selectDim(SDLoc &DL, SDValue Dim, SmallVectorImpl<SDValue> &Ops);

  void selectVCallDim(SDLoc &DL, SDNode *Node, unsigned StartIdx,
                      SmallVector<SDValue> &Ops);

  void selectTemplateBlock(SDLoc &DL, SDNode *Node, unsigned Opc,
                           unsigned TileUseNum);

  void selectTemplateBlockMX(SDLoc &DL, SDNode *Node, unsigned Opc,
                             unsigned TileUseNum);

  // v5: select PseudoMAMULB_SharedRight — pushes dims + DataTypeA/B + TileSize
  // + the single Local A tile + the Shared SSA register + Chain. The
  // Shared Right (B) is not a tile operand (bound by C.B.IOS at MC expansion).
  void selectTemplateBlockShared(SDLoc &DL, SDNode *Node, unsigned Opc);

  void selectTLoad(SDLoc &DL, SDNode *Node, unsigned Opc);

  void selectTStore(SDLoc &DL, SDNode *Node, unsigned Opc);

  bool isWorthOpW(SDNode *Node) const;
  bool isWorthShlext(SDNode *Node) const;
  bool isLoopReg(SDValue N) const;
  bool isRIOReg(SDValue N) const;

// Include the pieces autogenerated from the target description.
#include "LinxV5GenDAGISel.inc"

private:
  bool combineAndShift(SDNode *Node);
  bool combineShiftAndMask(SDNode *Node, uint64_t ShiftBits, uint64_t ImmValue,
                           SDValue AndOp0);
  bool combineAndMaskShift(SDNode *Node, uint64_t ShiftBits, uint64_t ImmValue,
                           SDValue ShiftOp0);
};
}

#endif
