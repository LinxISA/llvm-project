//===-- LinxV5ISelLowering.h - LinxV5 DAG Lowering Interface ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that LinxV5 uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_LINXV4_LINXV4ISELLOWERING_H
#define LLVM_LIB_TARGET_LINXV4_LINXV4ISELLOWERING_H

#include "LinxV5.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {
class LinxV5Subtarget;
namespace LinxV5ISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET_FLAG,
  URET_FLAG,
  SRET_FLAG,
  MRET_FLAG,
  CALL,
  /// Select with condition operator - This selects between a true value and
  /// a false value (ops #3 and #4) based on the boolean result of comparing
  /// the lhs and rhs (ops #0 and #1) of a conditional expression with the
  /// condition code in op #2, a XLenVT constant from the ISD::CondCode enum.
  /// The lhs and rhs are XLenVT integers. The true and false values can be
  /// integer or floating point.
  SELECT_CC,
  BuildPairF64,
  TAIL,
  // RV64I shifts, directly matching the semantics of the named LinxV5
  // instructions.
  SLLW,
  SRAW,
  SRLW,
  // 32-bit operations from RV64M that can't be simply matched with a pattern
  // at instruction selection time. These have undefined behavior for division
  // by 0 or overflow (divw) like their target independent counterparts.
  DIVW,
  DIVUW,
  REMUW,

  // float-integer covert operations
  FCVTU,
  FCVTS,
  BITCAST,

  FunctionBlock,
  ADDlo,
  LOOPSET,
  RDADD,
  RDOR,
  RDMAX,
  RDMIN,
  SHFLUP,
  SHFLDOWN,
  SHFLIDX,
  SHFLXOR,
  FABS,
  FSQRT,
  FEXP,
  MIN,
  MAX,

  // Vector Call
  VCALL,

  // Memory Call
  MCALL,

  // J-Core Template Block Inst
  BLK_MATMUL,
  BLK_MATMUL_AC,
  BLK_MATMULMX,
  BLK_MATMULMXB,
  BLK_MATMULMX_AC,
  BLK_MATMULMXB_AC,
  // v5: TMATMUL with a Shared Right operand (C.B.IOS binder, B not in B.IOT).
  BLK_MATMUL_SHARED,
  BLK_TLOAD,
  BLK_TSTORE,
  BLK_ACCCVT,
  V5_GMOV,
  V5_SHARED_L2S,
  V5_SHARED_S2L,
  MERGE_PREDICATION,
  CopyP,
  Copy2P,
  Copy2PTerm,
  MERGE_CF,
  IMPLICIT_DEF,
};
} // namespace LinxV5ISD

class LinxV5TargetLowering : public TargetLowering {
  const LinxV5Subtarget &Subtarget;

public:
  explicit LinxV5TargetLowering(const TargetMachine &TM,
                                const LinxV5Subtarget &STI);

  const LinxV5Subtarget &getSubtarget() const { return Subtarget; }

  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;
  bool isLegalICmpImmediate(int64_t Imm) const override;
  bool isLegalAddImmediate(int64_t Imm) const override;
  bool isZExtFree(SDValue Val, EVT VT2) const override;
  bool isSExtFree(SDValue Val) const override;
  bool isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const override;
  bool signExtendConstant(const ConstantInt *CI) const override;
  bool isIntDivCheap(EVT VT, AttributeList Attr) const override;
  bool isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                  EVT VT) const override;
  bool isFPImmLegal(const APFloat &Imm, EVT VT,
                    bool ForCodeSize) const override;

  bool isSDNodeAlwaysUniform(const SDNode *N) const override;
  bool isSDNodeSourceOfDivergenceImpl(const SDNode *N,
                                      FunctionLoweringInfo *FLI,
                                      LegacyDivergenceAnalysis *DA,
                                      bool Continuous) const;
  bool isSDNodeSourceOfDivergence(const SDNode *N,
    FunctionLoweringInfo *FLI, LegacyDivergenceAnalysis *DA) const override;
  const TargetRegisterClass *
  getRegClassFor(MVT VT, bool isDivergent = false) const override;

  // Provide custom lowering hooks for some operations.
  TargetLowering::LegalizeAction
  getCustomOperationAction(SDNode &Op) const override;
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool targetShrinkDemandedConstant(SDValue Op, const APInt &DemandedBits,
                                    const APInt &DemandedElts,
                                    TargetLoweringOpt &TLO) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  unsigned getInlineAsmMemConstraint(StringRef ConstraintCode) const override;

  TargetLowering::ConstraintWeight
  getSingleConstraintMatchWeight(AsmOperandInfo &info,
                                 const char *constraint) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, std::string &Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  void finalizeLowering(MachineFunction &MF) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  bool convertSetCCLogicToBitwiseLogic(EVT VT) const override {
    return VT.isScalarInteger();
  }
  bool convertSelectOfConstantsToMath(EVT VT) const override { return true; }

  bool shouldInsertFencesForAtomic(const Instruction *I) const override {
    return isa<LoadInst>(I) || isa<StoreInst>(I);
  }

  ISD::NodeType getExtendForAtomicOps() const override {
    return ISD::SIGN_EXTEND;
  }

  ISD::NodeType getExtendForAtomicCmpSwapArg() const override {
    return ISD::SIGN_EXTEND;
  }

  bool shouldExpandShift(SelectionDAG &DAG, SDNode *N) const override {
    if (DAG.getMachineFunction().getFunction().hasMinSize())
      return false;
    return true;
  }

  /// If a physical register, this returns the register that receives the
  /// exception address on entry to an EH pad.
  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override;

  /// If a physical register, this returns the register that receives the
  /// exception typeid on entry to a landing pad.
  Register
  getExceptionSelectorRegister(const Constant *PersonalityFn) const override;

  bool shouldExtendTypeInLibCall(EVT Type) const override;
  bool shouldSignExtendTypeInLibCall(EVT Type, bool IsSigned) const override;

  // Lower incoming arguments, copy physregs into vregs
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerINTRINSIC_VOID(SDValue Op, SelectionDAG &DAG) const;

  bool shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                         Type *Ty) const override {
    return true;
  }
  bool mayBeEmittedAsTailCall(const CallInst *CI) const override;
  bool shouldConsiderGEPOffsetSplit() const override { return true; }

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  bool isSkipCommonZextCombine(SDNode *N, SelectionDAG &DAG) const override;

  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override;

  const char *getTargetNodeName(unsigned Opcode) const override;

private:
  void analyzeInputArgs(MachineFunction &MF, CCState &CCInfo,
                        const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
                        bool isFuncBlock) const;
  void analyzeOutputArgs(MachineFunction &MF, CCState &CCInfo,
                         const SmallVectorImpl<ISD::OutputArg> &Outs,
                         bool IsRet, CallLoweringInfo *CLI,
                         bool isFuncBlock) const;

  template <class NodeTy>
  SDValue getAddr(NodeTy *N, SelectionDAG &DAG, bool IsLocal = true) const;

  SDValue getStaticTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG,
                           bool UseGOT) const;
  SDValue getDynamicTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;

  SDValue lowerXMULO(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerLOAD(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSTORE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSELECT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftLeftParts(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftRightParts(SDValue Op, SelectionDAG &DAG, bool IsSRA) const;
  SDValue lowerGetThreadIdx(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSysGet(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGetSIMTRet(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSetLoopIterations(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerReduce(unsigned Opcode, SDLoc &DL, SDValue Chain, SDValue Op,
                      SelectionDAG &DAG) const;
  SDValue lowerShuffle(unsigned Opcode, SDLoc &DL, SDValue Chain, SDValue Op,
                       SelectionDAG &DAG) const;
  SDValue lowerTwoSrcFloat(unsigned Opcode, SDLoc &DL, SDValue Chain,
                           SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerTileOpWithBody(SDLoc &DL, SDValue Op, unsigned VDefNum,
                              unsigned VUseNum, SelectionDAG &DAG,
                              unsigned Opcode) const;
  SDValue lowerTemplateBLK(unsigned Opcode, SDLoc &DL, SDValue Op,
                           unsigned VUseNum, SelectionDAG &DAG) const;
  SDValue lowerTemplateBLKMX(unsigned Opcode, SDLoc &DL, SDValue Op,
                             unsigned VUseNum, SelectionDAG &DAG) const;
  // v5: lower blk_matmul_shared. Like lowerTemplateBLK but pushes only the A
  // tile use (VUseNum=1) plus the SharedTID as a target constant — the Shared
  // Right operand is bound by C.B.IOS at MC expansion, never a B.IOT source.
  SDValue lowerTemplateBLKShared(SDLoc &DL, SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerTLoad(unsigned Opcode, SDLoc &DL, SDValue Op,
                     SelectionDAG &DAG) const;
  SDValue lowerTStore(unsigned Opcode, SDLoc &DL, SDValue Op,
                      SelectionDAG &DAG) const;
  SDValue lowerACCCVT(SDLoc &DL, SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFPArith(unsigned Opcode, SDLoc &DL, SDValue Chain, SDValue Op,
                       SelectionDAG &DAG) const;

  SDValue lowerSIMTBlockCF(Intrinsic::ID IntID, SDLoc &DL, SDValue Op,
                           SelectionDAG &DAG) const;

  SDValue lowerMergeCF(SDLoc &DL, SDValue Op, SelectionDAG &DAG) const;

  bool isEligibleForTailCallOptimization(
      CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
      const SmallVector<CCValAssign, 16> &ArgLocs) const;

  /// Generate error diagnostics if any register used by CC has been marked
  /// reserved.
  void validateCCReservedRegs(
      const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
      MachineFunction &MF) const;
};
}

#endif
