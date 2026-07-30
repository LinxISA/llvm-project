//===-- LinxV5ISelDAGToDAG.cpp - A dag to dag inst selector for LinxV5 ---===//
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

#include "LinxV5ISelDAGToDAG.h"
#include "MCTargetDesc/LinxV5MCTargetDesc.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "linx-isel"
static cl::opt<bool>
    EnableStackGuardUseCWR("linxv5-enable-stack-guard-with-cwr", cl::Hidden,
                           cl::init(false),
                           cl::desc("Enable to use CWR as stack check guard."));

static cl::opt<bool> EnableHLInstOpt(
    "linxv5-enable-HL-Inst-Opt", cl::Hidden, cl::init(true),
    cl::desc("Enable to use 48 bits Instructions Optimization."));

static cl::opt<bool> EnableRegExtensionOpt(
    "linxv5-enable-Reg-Extension-Opt", cl::Hidden, cl::init(true),
    cl::desc("Enable Support ALU instructions for type extension on the "
             "Reg Optimization."));

static cl::opt<bool> EnableLoadCombineOpt(
    "linxv5-enable-Load-Combine", cl::Hidden, cl::init(true),
    cl::desc("Enable Load Instruction Combine Optimization."));

static cl::opt<bool> EnableStoreCombineOpt(
    "linxv5-enable-Store-Combine", cl::Hidden, cl::init(true),
    cl::desc("Enable Store Instruction Combine Optimization."));

static SDNode *selectImmSeqSIMT(SelectionDAG *CurDAG, const SDLoc &DL,
                                LinxV5MatInt::SIMTInstSeq &Seq, MVT VT) {
  SDNode *Res = nullptr;
  SDValue Src = CurDAG->getRegister(LinxV5::R0, MVT::i64);
  for (auto &I : Seq) {
    SDValue SDImm = CurDAG->getTargetConstant(I.Imm, DL, VT);
    SDValue SDDstType = CurDAG->getTargetConstant(I.DstType, DL, MVT::i64);
    SDValue SDSrcType = CurDAG->getTargetConstant(I.SrcType, DL, MVT::i64);
    if (I.Opc == LinxV5::LUI) {
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {SDImm});
    } else if (I.Opc == LinxV5::SIMT_ICVT_U322U64) {
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {SDDstType, Src, SDSrcType});
    } else {
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {SDDstType, Src, SDSrcType, SDImm});
    }
    Src = SDValue(Res, 0);
  }
  return Res;
}

static SDNode *selectImmSeq(SelectionDAG *CurDAG, const SDLoc &DL,
                            LinxV5MatInt::InstSeq &Seq, MVT VT) {
  SDValue Src = CurDAG->getRegister(LinxV5::R0, VT);
  SDNode *Res = Src.getNode();
  for (auto &I : Seq) {
    SDValue SDImm = CurDAG->getTargetConstant(I.Imm, DL, VT);
    if (I.Opc == LinxV5::LUI) {
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {SDImm});
    } else if (I.Opc == LinxV5::OR_UW) {
      SDValue Zero = CurDAG->getRegister(LinxV5::R0, VT);
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {Zero, Src, SDImm});
    } else if (LinxV5::enableBFIOpt() && I.Opc == LinxV5::HL_BFI) {
      unsigned M = static_cast<unsigned>(I.Imm) & 0x7;
      unsigned N = static_cast<unsigned>(I.Imm) >> 3;
      SDValue SDImm1 = CurDAG->getTargetConstant(M, DL, MVT::i64);
      SDValue SDImm2 = CurDAG->getTargetConstant(N, DL, MVT::i64);
      Res = CurDAG->getMachineNode(I.Opc, DL, MVT::i64,
                                   {Src, Src, SDImm1, SDImm2});
    } else {
      Res = CurDAG->getMachineNode(I.Opc, DL, VT, {Src, SDImm});
    }
    Src = SDValue(Res, 0);
  }
  return Res;
}

static bool checkFloatingPointTypes(SelectionDAG &DAG) {
  for (SDNode &Node : DAG.allnodes()) {
    if (Node.use_empty() || Node.getOpcode() == ISD::DELETED_NODE)
      continue;

    for (unsigned i = 0; i < Node.getNumValues(); ++i) {
      MVT vt = Node.getSimpleValueType(i);
      if (vt.isFloatingPoint())
        return true;
    }

    for (unsigned i = 0; i < Node.getNumOperands(); ++i) {
      SDValue Operand = Node.getOperand(i);
      MVT vt = Operand.getSimpleValueType();
      if (vt.isFloatingPoint())
        return true;
    }
  }

  return false;
}
SDNode *emitImmWith48bits(SelectionDAG *CurDAG, const SDLoc &DL, int64_t Imm,
                          MVT VT, LinxV5MatInt::SIMTInstSeq &Seq) {
  if (!EnableHLInstOpt)
    return nullptr;

  if (VT.getSizeInBits() < 32)
    return nullptr;

  // Low 32 bits as unsigned 32-bit immediate (for ADDI / L_ADD_LI operands)
  uint64_t Lo32 = static_cast<uint64_t>(Imm) & 0xFFFFFFFFULL;
  // High 32 bits interpreted as signed 32-bit immediate (for HL_LUI)
  int64_t Hi32 = Imm >> 32;

  int optsize = 0;
  SDNode *Res = nullptr;
  SDValue Src = CurDAG->getRegister(LinxV5::R0, VT);

  // Hi32 use hl.lui
  if (Hi32) {
    Res = CurDAG->getMachineNode(LinxV5::HL_LUI, DL, VT,
                                 {CurDAG->getTargetConstant(Hi32, DL, VT)});
    optsize++;
    Src = SDValue(Res, 0);
  }
  // Lo32 use addi or l.addli(instead of lui+l.addi)
  if (isUInt<12>(Lo32))
    Res = CurDAG->getMachineNode(
        LinxV5::ADDI, DL, VT, {Src, CurDAG->getTargetConstant(Lo32, DL, VT)});
  else
    Res =
        CurDAG->getMachineNode(LinxV5::L_ADD_LI, DL, VT,
                               {Src, CurDAG->getTargetConstant(Lo32, DL, VT)});
  optsize++;

  // optsize can only be 1 or 2, that is:
  //  1. l.addli(64bits)
  //  2. hl.lui(48bits) + addi(32bits)/c.addi(16bits)
  //     hi.lui(48bits) + l.addli(64bits)
  if (optsize < Seq.size())
    return Res;

  return nullptr;
}

static SDNode *selectImm(SelectionDAG *CurDAG, const SDLoc &DL, int64_t Imm,
                         MVT VT) {
  const auto &STI =
      CurDAG->getMachineFunction().getSubtarget<LinxV5Subtarget>();
  if (STI.isSIMT()) {
    LinxV5MatInt::SIMTInstSeq Seq;
    LinxV5::generateSIMTMatIntSeq(Imm, Seq, VT);

    // use hl.lui/l.addli generate Imm
    // SDNode *Res = emitImmWith48bits(CurDAG, DL, Imm, VT, Seq);
    // if (Res)
    // return Res;

    return selectImmSeqSIMT(CurDAG, DL, Seq, VT);
  } else if (!STI.enableLegacyISel()) {
    LinxV5MatInt::InstSeq Seq;
    LinxV5::generateMatIntSeq(Imm, Seq, checkFloatingPointTypes(*CurDAG));
    return selectImmSeq(CurDAG, DL, Seq, VT);
  } else {
    SDValue SDImm = CurDAG->getTargetConstant(Imm, DL, VT);
    SDNode *Result =
        CurDAG->getMachineNode(LinxV5::PseudoVBXCONST, DL, VT, SDImm);
    return Result;
  }
}

static SDNode *selectNegation(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                              SDValue &Val) {
  return CurDAG->getMachineNode(LinxV5::PseudoVBXSUB, DL, VT,
                                SDValue(selectImm(CurDAG, DL, 0, MVT::i64), 0),
                                Val);
}

static SDNode *selectFlipping(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                              SDValue &Val) {
  return CurDAG->getMachineNode(LinxV5::PseudoVBXSUB, DL, VT,
                                SDValue(selectImm(CurDAG, DL, 1, MVT::i64), 0),
                                Val);
}

static SDNode *selectDecrement(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                               SDValue &Val) {
  return CurDAG->getMachineNode(LinxV5::PseudoVBXSUB, DL, VT, Val,
                                SDValue(selectImm(CurDAG, DL, 1, MVT::i64), 0));
}

// Select software floating-point arithmetic function calls to hardware
// instructions;
static SDNode *selectFArithmetics(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                                  SDValue &CallNode) {
  static std::map<std::string, unsigned> FCallsToFArithmetics = {
      // 32-bit floating-point arithmetic instructions
      {"__addsf3", LinxV5::PseudoVBXFADD_S},
      {"__subsf3", LinxV5::PseudoVBXFSUB_S},
      {"__mulsf3", LinxV5::PseudoVBXFMUL_S},
      {"__divsf3", LinxV5::PseudoVBXFDIV_S},
      // 64-bit floating-point arithmetic instructions
      {"__adddf3", LinxV5::PseudoVBXFADD_D},
      {"__subdf3", LinxV5::PseudoVBXFSUB_D},
      {"__muldf3", LinxV5::PseudoVBXFMUL_D},
      {"__divdf3", LinxV5::PseudoVBXFDIV_D},
  };
  ExternalSymbolSDNode *S =
      dyn_cast<ExternalSymbolSDNode>(CallNode->getOperand(1));
  if (!S) {
    return nullptr;
  }
  std::string Sym = S->getSymbol();
  if (!FCallsToFArithmetics.count(Sym)) {
    return nullptr;
  }
  SDValue ArgL = CallNode.getOperand(5).getOperand(0).getOperand(2);
  SDValue ArgR = CallNode.getOperand(5).getOperand(2);
  return CurDAG->getMachineNode(FCallsToFArithmetics[Sym], DL, VT, ArgL, ArgR);
}

static SDNode *selectFConversion(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                                 SDValue &CallNode) {
  // floating-point convertion instructions
  static std::map<std::string, unsigned> FCallsToFConversion = {
      // integer to floating-point conversion
      {"__floatsisf", LinxV5::PseudoVBXFCVT_SW_S},
      {"__floatsidf", LinxV5::PseudoVBXFCVT_SW_D},
      {"__floatdisf", LinxV5::PseudoVBXFCVT_SL_S},
      {"__floatdidf", LinxV5::PseudoVBXFCVT_SL_D},
      // unsigned integer to floating-point conversion
      {"__floatunsisf", LinxV5::PseudoVBXFCVT_UW_S},
      {"__floatunsidf", LinxV5::PseudoVBXFCVT_UW_D},
      {"__floatundisf", LinxV5::PseudoVBXFCVT_UL_S},
      {"__floatundidf", LinxV5::PseudoVBXFCVT_UL_D},
      // floating-point to integer conversion
      {"__fixsfsi", LinxV5::PseudoVBXFCVT_S_SW},
      {"__fixdfsi", LinxV5::PseudoVBXFCVT_D_SW},
      {"__fixsfdi", LinxV5::PseudoVBXFCVT_S_SL},
      {"__fixdfdi", LinxV5::PseudoVBXFCVT_D_SL},
      // floating-point to unsigned integer conversion
      {"__fixunssfsi", LinxV5::PseudoVBXFCVT_S_UW},
      {"__fixunsdfsi", LinxV5::PseudoVBXFCVT_D_UW},
      {"__fixunssfdi", LinxV5::PseudoVBXFCVT_S_UL},
      {"__fixunsdfdi", LinxV5::PseudoVBXFCVT_D_UL},
      // floating-point percision extension/truncation
      {"__extendsfdf2", LinxV5::PseudoVBXFCVT_S_D},
      {"__truncdfsf2", LinxV5::PseudoVBXFCVT_D_S},
  };
  ExternalSymbolSDNode *S =
      dyn_cast<ExternalSymbolSDNode>(CallNode->getOperand(1));
  if (!S) {
    return nullptr;
  }
  std::string Sym = S->getSymbol();
  if (!FCallsToFConversion.count(Sym)) {
    return nullptr;
  }
  SDValue Val = CallNode.getOperand(4).getOperand(2);
  return CurDAG->getMachineNode(FCallsToFConversion[Sym], DL, VT, Val);
}

static SDNode *selectFComparison(SelectionDAG *CurDAG, const SDLoc &DL, EVT VT,
                                 SDValue &CallNode) {
  using SelectionType =
      SDNode *(*)(SelectionDAG *, const SDLoc &, EVT, SDValue &);
  using FInstrInfo = struct {
    unsigned Opcode;
    bool Swap;
    SelectionType AddOnSelection;
  };
  static std::map<std::string, FInstrInfo> FCallsToFComparison = {
      // 32-bit floating-point comparison instructions
      {"__ltsf2", {LinxV5::PseudoVBXFLT_S, false, selectNegation}},
      {"__lesf2", {LinxV5::PseudoVBXFGE_S, true, selectFlipping}},
      {"__gtsf2", {LinxV5::PseudoVBXFLT_S, true, nullptr}},
      {"__gesf2", {LinxV5::PseudoVBXFGE_S, false, selectDecrement}},
      {"__eqsf2", {LinxV5::PseudoVBXFEQ_S, false, selectFlipping}},
      {"__nesf2", {LinxV5::PseudoVBXFEQ_S, false, selectFlipping}},
      // 64-bit floating-point comparison instructions
      {"__ltdf2", {LinxV5::PseudoVBXFLT_D, false, selectNegation}},
      {"__ledf2", {LinxV5::PseudoVBXFGE_D, true, selectFlipping}},
      {"__gtdf2", {LinxV5::PseudoVBXFLT_D, true, nullptr}},
      {"__gedf2", {LinxV5::PseudoVBXFGE_D, false, selectDecrement}},
      {"__eqdf2", {LinxV5::PseudoVBXFEQ_D, false, selectFlipping}},
      {"__nedf2", {LinxV5::PseudoVBXFEQ_D, false, selectFlipping}},
  };
  ExternalSymbolSDNode *S =
      dyn_cast<ExternalSymbolSDNode>(CallNode->getOperand(1));
  if (!S) {
    return nullptr;
  }
  std::string Sym = S->getSymbol();
  if (!FCallsToFComparison.count(Sym)) {
    return nullptr;
  }
  SDValue ArgL = CallNode.getOperand(5).getOperand(0).getOperand(2);
  SDValue ArgR = CallNode.getOperand(5).getOperand(2);
  SDNode *Result =
      CurDAG->getMachineNode(FCallsToFComparison[Sym].Opcode, DL, VT,
                             FCallsToFComparison[Sym].Swap ? ArgR : ArgL,
                             FCallsToFComparison[Sym].Swap ? ArgL : ArgR);
  if (SelectionType AddOnSelection = FCallsToFComparison[Sym].AddOnSelection) {
    SDValue Val(Result, 0);
    Result = AddOnSelection(CurDAG, DL, VT, Val);
  }
  return Result;
}

void LinxV5DAGToDAGISel::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LegacyDivergenceAnalysis>();
  SelectionDAGISel::getAnalysisUsage(AU);
}

void LinxV5DAGToDAGISel::selectLoadStackGuard(SDNode *Node) {
  SDLoc DL(Node);
  int CWR_ID = 0x820;
  SDNode *GetCWR;
  if (Subtarget->enableLegacyISel()) {
    GetCWR = CurDAG->getMachineNode(
        LinxV5::PseudoVBXSYSGET, DL, Node->getValueType(0),
        CurDAG->getTargetConstant(CWR_ID, DL, MVT::i64));
  } else {
    GetCWR =
        CurDAG->getMachineNode(LinxV5::SSR_GET, DL, Node->getValueType(0),
                               CurDAG->getTargetConstant(CWR_ID, DL, MVT::i64));
  }

  ReplaceNode(Node, GetCWR);
  return;
}

void LinxV5DAGToDAGISel::selectDim(SDLoc &DL, SDValue Dim,
                                   SmallVectorImpl<SDValue> &Ops) {
  SDValue Reg, Imm;
  if (isa<ConstantSDNode>(Dim)) {
    Reg = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    Imm = CurDAG->getTargetConstant(cast<ConstantSDNode>(Dim)->getZExtValue(),
                                    DL, MVT::i64);
  } else {
    Reg = Dim;
    Imm = CurDAG->getTargetConstant(0, DL, MVT::i64);
  }
  Ops.push_back(Reg);
  Ops.push_back(Imm);
}

void LinxV5DAGToDAGISel::selectVCallDim(SDLoc &DL, SDNode *Node,
                                        unsigned StartIdx,
                                        SmallVector<SDValue> &Ops) {
  selectDim(DL, Node->getOperand(StartIdx), Ops);
  selectDim(DL, Node->getOperand(StartIdx + 1), Ops);
  selectDim(DL, Node->getOperand(StartIdx + 2), Ops);
}

bool LinxV5DAGToDAGISel::isLoopReg(SDValue N) const {
  if (EnableRegExtensionOpt && N->getOpcode() == ISD::CopyFromReg) {
    auto Reg = cast<RegisterSDNode>(N->getOperand(1))->getReg();
    return LinxV5MCRegisterClasses[LinxV5::LoopRegRegClassID].contains(Reg);
  }
  return false;
}

bool LinxV5DAGToDAGISel::isRIOReg(SDValue N) const {
  if (EnableRegExtensionOpt && N->getOpcode() == ISD::CopyFromReg) {
    auto Reg = cast<RegisterSDNode>(N->getOperand(1))->getReg();
    MCRegister MCReg = MF->getRegInfo().getLiveInPhysReg(Reg);
    return LinxV5MCRegisterClasses[LinxV5::SIMT_RIORegClassID].contains(MCReg);
  }
  return false;
}

bool LinxV5DAGToDAGISel::isWorthOpW(SDNode *Node) const {
  assert((Node->getOpcode() == ISD::SIGN_EXTEND_INREG) && "Unexpected opcode");

  Node = Node->getOperand(0).getNode();

  // these opcode will be select to wop from (sext_inreg (op x, y), i32)
  switch (Node->getOpcode()) {
  case ISD::ADD:
  case ISD::SUB:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
    break;
  default:
    return false;
  }

  for (auto UI = Node->use_begin(), UE = Node->use_end(); UI != UE; ++UI) {
    SDNode *User = *UI;
    // any other users
    if (User->getOpcode() != ISD::SIGN_EXTEND_INREG) {
      return false;
    }
  }

  return true;
}

bool LinxV5DAGToDAGISel::isWorthShlext(SDNode *Node) const {
  assert(Node->getOpcode() == ISD::SHL && "Unexpected Shext node!");
  // TODO: i am optimize for codesize, alway fold shlext.
  return true;
}

/// We perfer use TableGen to finish this pattern match.
/// Cause TileReg is vAny type and TableGen need specify type.
/// Use DAGToDAG can avoid define lots of vector type.
void LinxV5DAGToDAGISel::selectTemplateBlock(SDLoc &DL, SDNode *Node,
                                             unsigned Opc,
                                             unsigned TileUseNum) {
  SmallVector<SDValue> Ops;
  selectVCallDim(DL, Node, 1, Ops);

  SDValue TileElementTypeA = Node->getOperand(4);
  Ops.push_back(TileElementTypeA);

  SDValue TileElementTypeB = Node->getOperand(5);
  Ops.push_back(TileElementTypeB);

  SDValue TileSize = Node->getOperand(6);
  Ops.push_back(TileSize);

  for (unsigned i = 0, e = TileUseNum; i != e; ++i)
    Ops.push_back(Node->getOperand(i + 7));

  Ops.push_back(Node->getOperand(0)); // Chain

  ReplaceNode(Node, CurDAG->getMachineNode(Opc, DL, Node->getValueType(0),
                                           Node->getValueType(1), Ops));
}

void LinxV5DAGToDAGISel::selectTemplateBlockMX(SDLoc &DL, SDNode *Node,
                                               unsigned Opc,
                                               unsigned TileUseNum) {
  SmallVector<SDValue> Ops;
  selectVCallDim(DL, Node, 1, Ops);

  SDValue DataTypeA = Node->getOperand(4);
  Ops.push_back(DataTypeA);

  SDValue DataTypeB = Node->getOperand(5);
  Ops.push_back(DataTypeB);

  SDValue TileSize = Node->getOperand(6);
  Ops.push_back(TileSize);

  for (unsigned i = 0, e = TileUseNum; i != e; ++i)
    Ops.push_back(Node->getOperand(i + 7));

  Ops.push_back(Node->getOperand(0)); // Chain

  ReplaceNode(Node, CurDAG->getMachineNode(Opc, DL, Node->getValueType(0),
                                           Node->getValueType(1), Ops));
}

void LinxV5DAGToDAGISel::selectTLoad(SDLoc &DL, SDNode *Node, unsigned Opc) {
  SmallVector<SDValue> Ops;
  selectVCallDim(DL, Node, 1, Ops);

  SDValue TileElementType = Node->getOperand(4);
  Ops.push_back(TileElementType);

  SDValue TileSize = Node->getOperand(5);
  Ops.push_back(TileSize);
  Ops.push_back(Node->getOperand(6)); // PadValue
  Ops.push_back(Node->getOperand(7)); // Layout
  Ops.push_back(Node->getOperand(8)); // DType
  Ops.push_back(Node->getOperand(9)); // Stride

  Ops.push_back(Node->getOperand(0)); // Chain

  ReplaceNode(Node, CurDAG->getMachineNode(Opc, DL, Node->getValueType(0),
                                           Node->getValueType(1), Ops));
}

void LinxV5DAGToDAGISel::selectTStore(SDLoc &DL, SDNode *Node, unsigned Opc) {
  SmallVector<SDValue> Ops;
  Ops.push_back(Node->getOperand(8));
  selectVCallDim(DL, Node, 1, Ops);

  SDValue TileElementType = Node->getOperand(4);
  Ops.push_back(TileElementType);
  Ops.push_back(Node->getOperand(5)); // Layout
  Ops.push_back(Node->getOperand(6)); // DType
  Ops.push_back(Node->getOperand(7)); // Stride

  Ops.push_back(Node->getOperand(0)); // Chain

  ReplaceNode(Node,
              CurDAG->getMachineNode(Opc, DL, Node->getValueType(0), Ops));
}

// v5: ACCCVT removed (replaced by TMATMUL_ACC). selectACCCVT deleted.

bool LinxV5DAGToDAGISel::calculateXDivergence(SDNode *N) {
  auto *LI = static_cast<const LinxV5TargetLowering *>(TLI);
  auto *DA = getAnalysisIfAvailable<LegacyDivergenceAnalysis>();
  if (LI->isSDNodeAlwaysUniform(N)) {
    assert(!LI->isSDNodeSourceOfDivergenceImpl(N, FuncInfo.get(), DA, true) &&
           "Conflicting divergence information!");
    return false;
  }
  if (LI->isSDNodeSourceOfDivergenceImpl(N, FuncInfo.get(), DA, true))
    return true;
  for (const auto &Op : N->ops()) {
    if (Op.getValueType() != MVT::Other && XDivergenceMap[Op.getNode()])
      return true;
  }
  return false;
}

void LinxV5DAGToDAGISel::updateXDivergence(SDNode *N) {
  if (XDivergenceMap.count(N))
    return;
  SmallVector<SDNode *, 16> Worklist(1, N);
  do {
    N = Worklist.back();
    int NumNotReady = 0;
    for (auto &Op : N->ops()) {
      if (Op.getValueType() != MVT::Other &&
          !XDivergenceMap.count(Op.getNode())) {
        ++NumNotReady;
        Worklist.push_back(Op.getNode());
      }
    }
    if (NumNotReady == 0) {
      XDivergenceMap[N] = calculateXDivergence(N);
      LLVM_DEBUG(dbgs() << "XDivergent " << XDivergenceMap[N] << " ";
                 N->dump(CurDAG));
      Worklist.pop_back();
    }
  } while (!Worklist.empty());
}

bool LinxV5DAGToDAGISel::isXDivergent(SDNode *N) {
  updateXDivergence(N);
  return XDivergenceMap[N];
}

void LinxV5DAGToDAGISel::analyzeXDivergence() {
  XDivergenceMap.clear();
  for (auto &N : CurDAG->allnodes()) {
    updateXDivergence(&N);
  }
}

static bool isLC0(SDValue N) {
  if (N->getOpcode() == ISD::ZERO_EXTEND)
    N = N->getOperand(0);
  if (N->getOpcode() == ISD::CopyFromReg)
    return cast<RegisterSDNode>(N->getOperand(1))->getReg() == LinxV5::SIMT_LC0;
  return false;
}

SDValue LinxV5DAGToDAGISel::findLC0AndBuildNewTree(SDValue N, SDValue &LC0,
                                                   unsigned Scale) {
  LLVM_DEBUG(dbgs() << __FUNCTION__ << " on "; N->dump(CurDAG));
  if (isLC0(N)) {
    if (Scale != 0)
      return N;
    LC0 = N;
    return SDValue();
  }

  if (isXUniform(N.getNode()))
    return N;

  if (N->getOpcode() == ISD::SHL) {
    SDValue amt = N->getOperand(1);
    if (ConstantSDNode *Amt = dyn_cast<ConstantSDNode>(amt.getNode())) {
      Scale -= Amt->getZExtValue();
      SDValue New = findLC0AndBuildNewTree(N->getOperand(0), LC0, Scale);
      if (!New) {
        assert(LC0);
        return New;
      }
      // let the DAGBuilder to make unique node.
      SDValue NewShl =
          CurDAG->getNode(ISD::SHL, SDLoc(N), N->getValueType(0), New, amt);
      NewShl = combineSHL(NewShl);
      return NewShl;
    }
  }

  if (N->getOpcode() != ISD::ADD) {
    return N;
  }
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);
  if (isXDivergent(N.getNode()) && isXUniform(RHS.getNode())) {
    LHS = findLC0AndBuildNewTree(LHS, LC0, Scale);
  } else if (isXDivergent(N.getNode()) && isXUniform(LHS.getNode())) {
    RHS = findLC0AndBuildNewTree(RHS, LC0, Scale);
  } else {
    assert(isXDivergent(LHS.getNode()) && isXDivergent(RHS.getNode()));
    return N;
  }

  if (!LHS)
    return RHS;
  else if (!RHS)
    return LHS;
  SDValue NewAdd =
      CurDAG->getNode(ISD::ADD, SDLoc(N), N->getValueType(0), LHS, RHS);
  LLVM_DEBUG(dbgs() << __FUNCTION__ << " build "; NewAdd->dump(CurDAG));
  return NewAdd;
}

SDValue LinxV5DAGToDAGISel::findBaseAndBuildNewTree(SDValue N,
                                                    SDValue &Uniform) {
  LLVM_DEBUG(dbgs() << __FUNCTION__ << " on "; N->dump(CurDAG));
  if (!N->isDivergent()) {
    Uniform = N;
    return SDValue();
  }

  if (N->getOpcode() != ISD::ADD) {
    return N;
  }
  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  LHS = findBaseAndBuildNewTree(LHS, Uniform);
  if (!Uniform)
    RHS = findBaseAndBuildNewTree(RHS, Uniform);

  if (!LHS)
    return RHS;
  else if (!RHS)
    return LHS;
  SDValue NewAdd =
      CurDAG->getNode(ISD::ADD, SDLoc(N), N->getValueType(0), LHS, RHS);
  LLVM_DEBUG(dbgs() << __FUNCTION__ << " build "; NewAdd->dump(CurDAG));
  return NewAdd;
}

// reform addr expression to:
// (uniform + [xunifrom]) + lc0 << scale
// sink the (lc0 << scale) to the tail so the left hand is a general addr
// pattern
void LinxV5DAGToDAGISel::reformContinuousAddr(SDValue N, unsigned Scale) {
  if (N->getOpcode() != ISD::ADD) {
    return;
  }

  LLVM_DEBUG(dbgs() << "Try reform "; N->dump(CurDAG));
  LLVM_DEBUG(dbgs() << "Scale " << Scale << "\n");

  // work through the polynomial. find the lc0, and build a new add-tree.
  SDValue LC0;
  SDValue XU = findLC0AndBuildNewTree(N, LC0, Scale);
  if (LC0) {
    {
      // TODO: remove this region after 0.54
      SDValue Base;
      XU = findBaseAndBuildNewTree(XU, Base);
      assert(Base);
      if (!XU)
        XU = Base;
      else
        XU = CurDAG->getNode(ISD::ADD, SDLoc(N), Base.getValueType(), Base, XU);
    }
    assert(isXUniform(XU.getNode()));
    SDValue Offset = CurDAG->getConstant(Scale, SDLoc(N), MVT::i64);
    SDValue LC0S =
        CurDAG->getNode(ISD::SHL, SDLoc(N), Offset.getValueType(), LC0, Offset);
    SDValue NewAddr =
        CurDAG->getNode(ISD::ADD, SDLoc(N), N->getValueType(0), XU, LC0S);
    if (NewAddr != N)
      CurDAG->ReplaceAllUsesWith(N, NewAddr);
  }
}

static bool isMemVTEqsRegVT(LSBaseSDNode *MN) {
  if (LoadSDNode *LN = dyn_cast<LoadSDNode>(MN)) {
    return LN->getExtensionType() == ISD::NON_EXTLOAD;
  } else if (StoreSDNode *SN = dyn_cast<StoreSDNode>(MN)) {
    return !SN->isTruncatingStore();
  }
}

void LinxV5DAGToDAGISel::reformContinuousAddrs() {
  CurDAG->AssignTopologicalOrder();
  // gather all addr expressions
  struct AddrScale {
    SDNode *Addr;
    unsigned Scale;
  };

  SmallVector<AddrScale, 8> Addrs;
  DenseMap<SDNode *, unsigned> Visited;
  for (auto &N : CurDAG->allnodes()) {
    if (LSBaseSDNode *MN = dyn_cast<LSBaseSDNode>(&N)) {
      if (!isMemVTEqsRegVT(MN))
        continue;
      SDNode *Addr = MN->getBasePtr().getNode();
      unsigned Scale =
          countTrailingZeros((uint64_t)MN->getMemoryVT().getSizeInBits() >> 3);
      if (Visited.count(Addr) && Visited[Addr] != Scale)
        continue;
      Visited[Addr] = Scale;
      Addrs.push_back({Addr, Scale});
    }
  }

  // do reform
  for (auto &Addr : reverse(Addrs)) {
    reformContinuousAddr(SDValue(Addr.Addr, 0), Addr.Scale);
  }
}

void LinxV5DAGToDAGISel::reformSIMTAddr(SDValue N, unsigned Scale) {
  if (N->getOpcode() != ISD::ADD) {
    return;
  }

  LLVM_DEBUG(dbgs() << "Try reform "; N->dump(CurDAG));
  LLVM_DEBUG(dbgs() << "Scale " << Scale << "\n");

  // work through the polynomial. find the lc0, and build a new add-tree.
  SDValue XU;
  SDValue Base;
  XU = findBaseAndBuildNewTree(N, Base);
  if (!Base)
    return;
  if (!XU)
    return;
  SDValue NewAddr =
      CurDAG->getNode(ISD::ADD, SDLoc(N), Base.getValueType(), Base, XU);
  if (NewAddr != N)
    CurDAG->ReplaceAllUsesWith(N, NewAddr);
}

void LinxV5DAGToDAGISel::reformSIMTAddrs() {
  CurDAG->AssignTopologicalOrder();
  // gather all addr expressions
  struct AddrScale {
    SDNode *Addr;
    unsigned Scale;
  };

  SmallVector<AddrScale, 8> Addrs;
  DenseMap<SDNode *, unsigned> Visited;
  for (auto &N : CurDAG->allnodes()) {
    if (LSBaseSDNode *MN = dyn_cast<LSBaseSDNode>(&N)) {
      if (!isMemVTEqsRegVT(MN))
        continue;
      SDNode *Addr = MN->getBasePtr().getNode();
      unsigned Scale =
          countTrailingZeros((uint64_t)MN->getMemoryVT().getSizeInBits() >> 3);
      if (Visited.count(Addr) && Visited[Addr] != Scale)
        continue;
      Visited[Addr] = Scale;
      Addrs.push_back({Addr, Scale});
    }
  }

  // do reform
  for (auto &Addr : reverse(Addrs)) {
    reformSIMTAddr(SDValue(Addr.Addr, 0), Addr.Scale);
  }
}

SDValue LinxV5DAGToDAGISel::combineSHL(SDValue N) {
  if (!EnableStoreCombineOpt)
    return N;
  if (!N)
    return N;
  if (N.getOpcode() != ISD::SHL)
    return N;
  SDValue LHS = N.getOperand(0);
  if (LHS.getOpcode() != ISD::SHL)
    return N;
  LLVM_DEBUG(dbgs() << "try combine shl at "; N.dump(CurDAG));
  ConstantSDNode *amt0 = dyn_cast<ConstantSDNode>(N.getOperand(1));
  ConstantSDNode *amt1 = dyn_cast<ConstantSDNode>(LHS.getOperand(1));
  if (!amt0 || !amt1)
    return N;
  uint64_t Scale = amt0->getZExtValue() + amt1->getZExtValue();
  if (Scale > 31)
    return N;
  SDValue New =
      CurDAG->getNode(ISD::SHL, SDLoc(N), N.getValueType(), LHS.getOperand(0),
                      CurDAG->getConstant(Scale, SDLoc(N), MVT::i64));
  return New;
}

void LinxV5DAGToDAGISel::PreprocessISelDAG() {
  std::string BlockName =
      (MF->getName() + ":" + FuncInfo->MBB->getBasicBlock()->getName()).str();
  // if (Subtarget->isSIMT())
  //   reformSIMTAddrs();
  if (Subtarget->enableContinuousMemOpt() && Subtarget->isSIMT()) {
    analyzeXDivergence();
    reformContinuousAddrs();
  }

  LLVM_DEBUG(dbgs() << "ISel preprocessed selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());
}

void LinxV5DAGToDAGISel::Select(SDNode *Node) {
  // If we have a custom node, we have already selected.
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(dbgs() << "== "; Node->dump(CurDAG); dbgs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // Instruction Selection not handled by the auto-generated tablegen selection
  // should be handled here.
  unsigned Opcode = Node->getOpcode();
  MVT XLenVT = Subtarget->getXLenVT();
  SDLoc DL(Node);
  EVT VT = Node->getValueType(0);

  switch (Opcode) {
  case ISD::UNDEF: {
    if (!Subtarget->isSIMT())
      break;
    auto DType = CurDAG->getTargetConstant(
        LinxV5::getSIMTDstTypeFromBits(Node->getValueType(0).getSizeInBits()),
        DL, MVT::i8);
    ReplaceNode(Node, CurDAG->getMachineNode(Node->isDivergent()
                                                 ? LinxV5::LinxV5ImplicitDef
                                                 : LinxV5::LinxV5ImplicitSDef,
                                             DL, Node->getValueType(0), DType));
    return;
  }
  case ISD::ConstantFP: {
    auto *FP = dyn_cast<ConstantFPSDNode>(Node);
    if (!FP)
      return;

    if (FP->isZero() &&
        (!Subtarget->enableLegacyISel() || Subtarget->isSIMT())) {
      auto Zero =
          CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, LinxV5::R0, VT);
      ReplaceNode(Node, Zero.getNode());
      return;
    }
    uint64_t Imm = FP->getValueAPF().bitcastToAPInt().getZExtValue();
    EVT IntVT = VT.changeTypeToInteger();
    SDNode *ImmNode = selectImm(CurDAG, DL, Imm, IntVT.getSimpleVT());
    SDValue ImmValue(ImmNode, 0);
    SDNode *Casted =
        CurDAG->getMachineNode(TargetOpcode::COPY, DL, VT, ImmValue);
    ReplaceNode(Node, Casted);
    return;
  }
  case ISD::Constant: {
    auto ConstNode = cast<ConstantSDNode>(Node);
    if (ConstNode->isZero() &&
        (!Subtarget->enableLegacyISel() || Subtarget->isSIMT())) {
      auto Zero =
          CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, LinxV5::R0, VT);
      ReplaceNode(Node, Zero.getNode());
      return;
    }
    int64_t Imm = ConstNode->getSExtValue();
    ReplaceNode(Node, selectImm(CurDAG, DL, Imm, VT.getSimpleVT()));
    return;
  }
  case LinxV5ISD::SELECT_CC: {
    if (!Subtarget->hasCSel())
      break;
    assert(Subtarget->enableLegacyISel() && "This is legacy select lowering!");
    // output = SELECT_CC (cond_lhs, cond_rhs, cond_kind, data_lhs, data_rhs)
    //   ==>
    // cond = cmp_xx (cond_lhs, cond_rhs)
    // output = csel (data_lhs, data_rhs, cond)
    SDValue CondLHS = Node->getOperand(0);
    SDValue CondRHS = Node->getOperand(1);
    unsigned LinxV5CondOpc;
    switch (cast<ConstantSDNode>(Node->getOperand(2))->getSExtValue()) {
    case ISD::SETEQ:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_EQ;
      break;
    case ISD::SETNE:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_NE;
      break;
    case ISD::SETLT:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_LT;
      break;
    case ISD::SETGE:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_GE;
      break;
    case ISD::SETULT:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_LTU;
      break;
    case ISD::SETUGE:
      LinxV5CondOpc = LinxV5::PseudoVBXCMP_GEU;
      break;
    default:
      llvm_unreachable("Unsupported CondCode for SELECT_CC!");
    }
    SDNode *Cond =
        CurDAG->getMachineNode(LinxV5CondOpc, DL, XLenVT, CondLHS, CondRHS);
    ReplaceNode(Node,
                CurDAG->getMachineNode(LinxV5::PseudoVBXSELECT, DL, XLenVT,
                                       Node->getOperand(3), Node->getOperand(4),
                                       SDValue(Cond, 0 /*data output*/)));
    return;
  }
  case ISD::CopyFromReg: {
    if (Subtarget->hasFloat() && Subtarget->enableLegacyISel()) {
      SDValue CopySrc = Node->getOperand(0);
      if (CopySrc.getOpcode() == ISD::CALLSEQ_END) {
        SDValue CallNode = CopySrc.getOperand(0);
        if (CallNode.getOpcode() == LinxV5ISD::CALL) {
          if (SDNode *Replacement =
                  selectFComparison(CurDAG, DL, VT, CallNode)) {
            ReplaceNode(Node, Replacement);
            return;
          }
          if (SDNode *Replacement =
                  selectFConversion(CurDAG, DL, VT, CallNode)) {
            ReplaceNode(Node, Replacement);
            return;
          }
          if (SDNode *Replacement =
                  selectFArithmetics(CurDAG, DL, VT, CallNode)) {
            ReplaceNode(Node, Replacement);
            return;
          }
        }
      }
    }
    break;
  }
  case ISD::FrameIndex: {
    int FI = cast<FrameIndexSDNode>(Node)->getIndex();
    SDValue TFI = CurDAG->getTargetFrameIndex(FI, VT);
    if (!Subtarget->enableLegacyISel()) {
        SDValue Zero = CurDAG->getTargetConstant(0, DL, MVT::i64);
        ReplaceNode(Node,
                    CurDAG->getMachineNode(LinxV5::ADDI, DL, VT, TFI, Zero));
    } else {
        SDNode *ImmNode = selectImm(CurDAG, DL, 0, XLenVT);
        SDValue SrcReg = SDValue(ImmNode, 0);
        ReplaceNode(Node, CurDAG->getMachineNode(LinxV5::PseudoVBXADD, DL, VT,
                                                 TFI, SrcReg));
    }
    return;
  }
  case LinxV5ISD::VCALL: {
    // We perfer use TableGen to finish this pattern match.
    // Cause TileReg is vAny type and TableGen need specify type.
    // Use DAGToDAG can avoid define lots of vector type.
    SmallVector<SDValue> Ops;
    Ops.push_back(Node->getOperand(1)); // Label
    selectVCallDim(DL, Node, 2, Ops);   // Dim x,y,z

    unsigned TileDefNum = Node->getNumValues() - 1; // At least chain output.
    unsigned OpIdx = 5;
    bool IsConstantTileSize = true;

    for (unsigned i = 0; i < TileDefNum; ++i) {
        SDValue TileSizeValue = Node->getOperand(OpIdx++);
        Ops.push_back(TileSizeValue); // Tile Size Mask
        IsConstantTileSize = isa<ConstantSDNode>(TileSizeValue);
    }

    unsigned TileUseNum = 0;
    bool hasGPRUse = false;
    for (unsigned i = OpIdx, e = Node->getNumOperands(); i != e; ++i) {
        if (Node->getOperand(i).getValueType().isVector())
          ++TileUseNum;
        else
          break;
        Ops.push_back(Node->getOperand(i));
    }

    if (!Subtarget->enableContinuousMemOpt())
        Ops.push_back(
            CurDAG->getTargetConstant(LinxV5Op::DREnum::DR, DL, MVT::i64));
    else
        Ops.push_back(
            CurDAG->getTargetConstant(LinxV5Op::DREnum::MR, DL, MVT::i64));

    Ops.push_back(CurDAG->getRegister(LinxV5::Tile_S, MVT::i64));
    SDValue StackSizeSymbol = Node->getOperand(OpIdx + TileUseNum);
    Ops.push_back(StackSizeSymbol);
    OpIdx++;

    for (unsigned i = OpIdx + TileUseNum, e = Node->getNumOperands(); i != e; ++i) {
        hasGPRUse = true;
        Ops.push_back(Node->getOperand(i));
    }

    Ops.push_back(Node->getOperand(0)); // Chain

    unsigned SizeRCode = TileDefNum ? 0 : 2;
    unsigned VCallOpc =
        LinxV5TileCall::lookupTileCallByDesc(TileDefNum, TileUseNum, hasGPRUse,
                                             SizeRCode, 0, 0, 0, 0)
            ->Insn;

    ReplaceNode(Node,
                CurDAG->getMachineNode(VCallOpc, DL, Node->getVTList(), Ops));
    return;
  }
  case LinxV5ISD::MCALL: {
    // We perfer use TableGen to finish this pattern match.
    // Cause TileReg is vAny type and TableGen need specify type.
    // Use DAGToDAG can avoid define lots of vector type.
    SmallVector<SDValue> Ops;
    Ops.push_back(Node->getOperand(1)); // Label
    selectVCallDim(DL, Node, 2, Ops);   // Dim x,y,z

    unsigned TileDefNum = Node->getNumValues() - 1; // At least chain output.
    unsigned OpIdx = 5;
    bool IsConstantTileSize = true;
    for (unsigned i = 0; i < TileDefNum; ++i) {
        SDValue TileSizeValue = Node->getOperand(OpIdx++);
        Ops.push_back(TileSizeValue); // Tile Size Mask
        IsConstantTileSize = isa<ConstantSDNode>(TileSizeValue);
    }

    unsigned TileUseNum = 0;
    bool hasGPRUse = false;
    for (unsigned i = OpIdx, e = Node->getNumOperands(); i != e; ++i) {
        if (Node->getOperand(i).getValueType().isVector())
          ++TileUseNum;
        else
          break;
        Ops.push_back(Node->getOperand(i));
    }

    if (!Subtarget->enableContinuousMemOpt())
        Ops.push_back(
            CurDAG->getTargetConstant(LinxV5Op::DREnum::DR, DL, MVT::i64));
    else
        Ops.push_back(
            CurDAG->getTargetConstant(LinxV5Op::DREnum::MR, DL, MVT::i64));

    Ops.push_back(CurDAG->getRegister(LinxV5::Tile_S, MVT::i64));
    SDValue StackSizeSymbol = Node->getOperand(OpIdx + TileUseNum);
    Ops.push_back(StackSizeSymbol);
    OpIdx++;

    for (unsigned i = OpIdx + TileUseNum, e = Node->getNumOperands(); i != e; ++i) {
        hasGPRUse = true;
        Ops.push_back(Node->getOperand(i));
    }

    Ops.push_back(Node->getOperand(0)); // Chain

    unsigned SizeRCode = TileDefNum ? 0 : 2;
    unsigned McallOpc =
        LinxV5TileCall::lookupTileCallByDesc(TileDefNum, TileUseNum, hasGPRUse,
                                             SizeRCode, 1, 0, 0, 0)
            ->Insn;

    ReplaceNode(Node,
                CurDAG->getMachineNode(McallOpc, DL, Node->getVTList(), Ops));

    return;
  }
  case LinxV5ISD::BLK_MATMUL: {
    selectTemplateBlock(DL, Node, LinxV5::PseudoMAMULB_SizeI, 2);
    return;
  }
  case LinxV5ISD::BLK_MATMUL_AC: {
    selectTemplateBlock(DL, Node, LinxV5::PseudoMAMULBAC_Higher_SizeI, 3);
    return;
  }
  case LinxV5ISD::BLK_MATMUL_FIXP: {
    selectTemplateBlock(DL, Node, LinxV5::PseudoMAMULB_FIXP_SizeI, 2);
    return;
  }
  case LinxV5ISD::BLK_MATMUL_ACC_FIXP: {
    selectTemplateBlock(DL, Node, LinxV5::PseudoMAMULB_ACC_FIXP_SizeI, 3);
    return;
  }
  case LinxV5ISD::BLK_MATMULMX: {
    selectTemplateBlockMX(DL, Node, LinxV5::PseudoMAMULBMX_SizeI, 4);
    return;
  }
  case LinxV5ISD::BLK_MATMULMXB: {
    selectTemplateBlockMX(DL, Node, LinxV5::PseudoMAMULBMXB_SizeI, 3);
    return;
  }
  case LinxV5ISD::BLK_MATMULMX_AC: {
    selectTemplateBlockMX(DL, Node, LinxV5::PseudoMAMULBMXAC_Higher_SizeI, 5);
    return;
  }
  case LinxV5ISD::BLK_MATMULMXB_AC: {
    selectTemplateBlockMX(DL, Node, LinxV5::PseudoMAMULBMXBAC_Higher_SizeI, 4);
    return;
  }
  case LinxV5ISD::BLK_TLOAD: {
    selectTLoad(DL, Node, LinxV5::PseudoTLOAD_noDsrc_noDdst);
    return;
  }
  case LinxV5ISD::BLK_TSTORE: {
    selectTStore(DL, Node, LinxV5::PseudoTSTORE_noDsrc_noDdst);
    return;
  }
  case LinxV5ISD::V5_GMOV: {
    SmallVector<SDValue> Ops;
    for (unsigned Index = 1; Index != Node->getNumOperands(); ++Index)
      Ops.push_back(Node->getOperand(Index));
    Ops.push_back(Node->getOperand(0));
    ReplaceNode(Node, CurDAG->getMachineNode(
                          LinxV5::PseudoV5GMOV, DL, Node->getVTList(), Ops));
    return;
  }
  case LinxV5ISD::V5_SHARED_L2S: {
    SmallVector<SDValue> Ops;
    for (unsigned Index = 1; Index != Node->getNumOperands(); ++Index)
      Ops.push_back(Node->getOperand(Index));
    Ops.push_back(Node->getOperand(0));
    ReplaceNode(Node, CurDAG->getMachineNode(
                          LinxV5::PseudoV5SharedL2S, DL, Node->getVTList(), Ops));
    return;
  }
  case LinxV5ISD::V5_SHARED_S2L: {
    SmallVector<SDValue> Ops;
    for (unsigned Index = 1; Index != Node->getNumOperands(); ++Index)
      Ops.push_back(Node->getOperand(Index));
    Ops.push_back(Node->getOperand(0));
    ReplaceNode(Node, CurDAG->getMachineNode(
                          LinxV5::PseudoV5SharedS2L, DL, Node->getVTList(), Ops));
    return;
  }
  // v5: BLK_ACCCVT removed (replaced by TMATMUL_ACC).
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::SREM:
  case ISD::UREM: {
    break;
  }
  case ISD::INTRINSIC_W_CHAIN: {
    unsigned IntNo = cast<ConstantSDNode>(Node->getOperand(1))->getZExtValue();
    switch (IntNo) {
    default:
        break;
    }
    break;
  }
  case ISD::LOAD: {
    if (EnableStackGuardUseCWR) {
      MemSDNode *MSD = dyn_cast<MemSDNode>(Node);
      const Value *V = MSD->getMemOperand()->getValue();
      const GlobalVariable *GV = dyn_cast_or_null<GlobalVariable>(V);
      if (GV && GV->getName() == "__stack_chk_guard") {
          selectLoadStackGuard(Node);
          return;
      }
    }
    break;
  }
  }

  // Select the default instruction.
  SelectCode(Node);
}

bool LinxV5DAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, unsigned ConstraintID, std::vector<SDValue> &OutOps) {
  switch (ConstraintID) {
  case InlineAsm::Constraint_m:
    // We just support simple memory operands that have a single address
    // operand and need no special handling.
    OutOps.push_back(Op);
    return false;
  case InlineAsm::Constraint_A:
    OutOps.push_back(Op);
    return false;
  default:
    break;
  }

  return true;
}

bool LinxV5DAGToDAGISel::SelectAddrFI(SDValue Addr, SDValue &Base) {
  if (auto FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), Subtarget->getXLenVT());
    return true;
  }
  return false;
}

static unsigned getZExtRegType(MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UW;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UH;
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_UB;
  default:
    llvm_unreachable("unexpected zext ValueType!");
  }
}

static unsigned getSExtRegType(MVT VT) {
  switch (VT.SimpleTy) {
  case MVT::i32:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SW;
  case MVT::i16:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SH;
  case MVT::i8:
    return LinxV5Op::SIMT_INT_SRC_REG_TYPE_SB;
  default:
    llvm_unreachable("unexpected zext ValueType!");
  }
}

template <unsigned Scale>
bool LinxV5DAGToDAGISel::SelectContinuousADDRBase(SDValue N, SDValue &Addr) {
  if (!Subtarget->enableContinuousMemOpt())
    return false;
  if (N.getOpcode() != ISD::ADD)
    return false;

  // the SrcL should be XUniform
  SDValue SrcL = N.getOperand(0);
  if (isXDivergent(SrcL.getNode()))
    return false;

  Addr = SrcL;
  // the SrcR should be lc0 << Scale
  SDValue SrcR = N.getOperand(1);
  if (isLC0(SrcR) && Scale == 0)
    return true;
  if (SrcR.getOpcode() != ISD::SHL)
    return false;
  ConstantSDNode *amt = dyn_cast<ConstantSDNode>(SrcR.getOperand(1));
  if (!amt || amt->getZExtValue() != Scale) {
    return false;
  }
  return isLC0(SrcR.getOperand(0));
}

template <unsigned Scale>
bool LinxV5DAGToDAGISel::SelectContinuousLoadADDRrr(SDValue N, SDValue &SrcL,
                                                    SDValue &SrcR,
                                                    SDValue &SrcRType,
                                                    SDValue &Shamt) {
  if (!SelectContinuousADDRBase<Scale>(N, N))
    return false;

  return SelectSIMTLoadADDRrr(N, SrcL, SrcR, SrcRType, Shamt);
}

bool LinxV5DAGToDAGISel::SelectSIMTLoadADDRrr(SDValue N, SDValue &SrcL,
                                              SDValue &SrcR, SDValue &SrcRType,
                                              SDValue &Shamt) {
  if (N.getOpcode() != ISD::ADD) {
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    SrcRType = CurDAG->getTargetConstant(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD,
                                         SDLoc(N), MVT::i8);
    Shamt = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  // TODO: How to optimize if this node is reused. like aarch64?

  SrcL = N.getOperand(0);
  SrcR = N.getOperand(1);

  unsigned ExtType = LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD;
  unsigned ShiftValue = 0;

  // try to match shl
  while (SrcR.getOpcode() == ISD::SHL) {
    SDValue SHLNode = SrcR;
    ConstantSDNode *amt = dyn_cast<ConstantSDNode>(SHLNode.getOperand(1));
    if (amt && isUInt<5>(ShiftValue + amt->getZExtValue())) {
      SrcR = SHLNode.getOperand(0);
      ShiftValue += amt->getZExtValue();
    } else {
      break;
    }
    if (!EnableLoadCombineOpt)
      break;
  }
  Shamt = CurDAG->getTargetConstant(ShiftValue, SDLoc(N), MVT::i64);

  // try to match sext/zext
  if (SrcR.getOpcode() == ISD::ZERO_EXTEND) {
    ExtType = getZExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  } else if (SrcR.getOpcode() == ISD::SIGN_EXTEND) {
    ExtType = getSExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  }
  SrcRType = CurDAG->getTargetConstant(ExtType, SDLoc(N), MVT::i8);

  return true;
}

template <unsigned Scale>
bool LinxV5DAGToDAGISel::SelectContinuousStoreADDRrr(SDValue N, SDValue &SrcL,
                                                     SDValue &SrcR,
                                                     SDValue &SrcRType,
                                                     SDValue &ShamtImm) {
  if (!SelectContinuousADDRBase<Scale>(N, N))
    return false;

  return SelectSIMTStoreADDRrr_scaled<Scale>(N, SrcL, SrcR, SrcRType, ShamtImm);
}

template <unsigned Shamt>
bool LinxV5DAGToDAGISel::SelectSIMTStoreADDRrr_scaled(SDValue N, SDValue &SrcL,
                                                      SDValue &SrcR,
                                                      SDValue &SrcRType,
                                                      SDValue &ShamtImm) {
  if (N.getOpcode() != ISD::ADD) {
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    SrcRType = CurDAG->getTargetConstant(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD,
                                         SDLoc(N), MVT::i8);
    ShamtImm = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  SDValue RHS = N.getOperand(1);
  if (RHS.getOpcode() != ISD::SHL)
    return false;

  ConstantSDNode *amt = dyn_cast<ConstantSDNode>(RHS.getOperand(1));
  if (!amt || !(amt->getZExtValue() >= Shamt) ||
      !isUInt<5>(amt->getZExtValue() - Shamt))
    return false;

  SrcL = N.getOperand(0);
  SrcR = RHS.getOperand(0);
  ShamtImm = CurDAG->getTargetConstant(amt->getZExtValue() - Shamt, SDLoc(N),
                                       MVT::i64);

  unsigned ExtType = LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD;
  // try to match sext/zext
  if (SrcR.getOpcode() == ISD::ZERO_EXTEND) {
    ExtType = getZExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  } else if (SrcR.getOpcode() == ISD::SIGN_EXTEND) {
    ExtType = getSExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  }
  SrcRType = CurDAG->getTargetConstant(ExtType, SDLoc(N), MVT::i8);

  return true;
}

bool LinxV5DAGToDAGISel::SelectSIMTStoreADDRrr_unscaled(SDValue N,
                                                        SDValue &SrcL,
                                                        SDValue &SrcR,
                                                        SDValue &SrcRType,
                                                        SDValue &ShamtImm) {
  if (N.getOpcode() != ISD::ADD) {
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    SrcRType = CurDAG->getTargetConstant(LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD,
                                         SDLoc(N), MVT::i8);
    ShamtImm = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  SrcL = N.getOperand(0);
  SrcR = N.getOperand(1);

  unsigned ShiftValue = 0;
  // Check for shift operation
  if (SrcR.getOpcode() == ISD::SHL) {
    SDValue ShiftVal = SrcR.getOperand(1);
    if (ConstantSDNode *CN = dyn_cast<ConstantSDNode>(ShiftVal)) {
      uint64_t ShiftAmount = CN->getZExtValue();
      if (isUInt<5>(ShiftValue +
                    ShiftAmount)) { // Ensure it is within the 5-digit range.
          ShiftValue += ShiftAmount;
          SrcR = SrcR.getOperand(0);
      }
    }
  }
  ShamtImm = CurDAG->getTargetConstant(ShiftValue, SDLoc(N), MVT::i64);

  unsigned ExtType = LinxV5Op::SIMT_INT_SRC_REG_TYPE_SD;
  // try to match sext/zext
  if (SrcR.getOpcode() == ISD::ZERO_EXTEND) {
    ExtType = getZExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  } else if (SrcR.getOpcode() == ISD::SIGN_EXTEND) {
    ExtType = getSExtRegType(SrcR.getOperand(0).getValueType().getSimpleVT());
    SrcR = SrcR.getOperand(0);
  }
  SrcRType = CurDAG->getTargetConstant(ExtType, SDLoc(N), MVT::i8);

  return true;
}

template <LinxV5Op::SrcRType RType>
bool LinxV5DAGToDAGISel::SelectSrcREXT(SDValue N, SDValue &SrcR) {
  if (RType == LinxV5Op::NONE) {
    SrcR = N;
    return true;
  } else if (RType == LinxV5Op::SW) {
    if (N.getOpcode() != ISD::SIGN_EXTEND_INREG ||
        cast<VTSDNode>(N.getOperand(1))->getVT() != MVT::i32)
      return false;
    SrcR = N.getOperand(0);
    return true;
  } else if (RType == LinxV5Op::UW) {
    if (N.getOpcode() != ISD::AND)
      return false;
    auto *C = dyn_cast<ConstantSDNode>(N.getOperand(1));
    if (!C || C->getZExtValue() != 0xffffffffull)
      return false;
    SrcR = N.getOperand(0);
    return true;
  } else {
    report_fatal_error("unexpect SrcR extent type");
  }
}

template <LinxV5Op::SrcRType RType>
bool LinxV5DAGToDAGISel::SelectLoadADDRrr(SDValue N, SDValue &SrcL,
                                          SDValue &SrcR, SDValue &Shamt) {
  if (N.getOpcode() != ISD::ADD) {
    if (RType != LinxV5Op::NONE)
      return false;
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    Shamt = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  // TODO: How to optimize if this node is reused. like aarch64?

  SrcL = N.getOperand(0);
  SrcR = N.getOperand(1);
  unsigned ShiftValue = 0;

  // try to match shl
  while (SrcR.getOpcode() == ISD::SHL) {
    SDValue SHLNode = SrcR;
    ConstantSDNode *amt = dyn_cast<ConstantSDNode>(SHLNode.getOperand(1));
    if (amt && isUInt<5>(ShiftValue + amt->getZExtValue()) &&
        isWorthShlext(SrcR.getNode())) {
      SrcR = SHLNode.getOperand(0);
      ShiftValue += amt->getZExtValue();
    } else {
      break;
    }
    if (!EnableLoadCombineOpt)
      break;
  }
  Shamt = CurDAG->getTargetConstant(ShiftValue, SDLoc(N), MVT::i64);

  return SelectSrcREXT<RType>(SrcR, SrcR);
}

template <LinxV5Op::SrcRType RType, unsigned Shamt>
bool LinxV5DAGToDAGISel::SelectStoreADDRrr_scaled(SDValue N, SDValue &SrcL,
                                                  SDValue &SrcR) {
  if (N.getOpcode() != ISD::ADD) {
    if (RType != LinxV5Op::NONE)
      return false;
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    return true;
  }

  SDValue RHS = N.getOperand(1);
  if (RHS.getOpcode() != ISD::SHL)
    return false;

  ConstantSDNode *amt = dyn_cast<ConstantSDNode>(RHS.getOperand(1));
  if (!amt || amt->getZExtValue() != Shamt)
    return false;

  SrcL = N.getOperand(0);
  SrcR = RHS.getOperand(0);

  return SelectSrcREXT<RType>(SrcR, SrcR);
}

template <LinxV5Op::SrcRType RType>
bool LinxV5DAGToDAGISel::SelectStoreADDRrr_unscaled(SDValue N, SDValue &SrcL,
                                                    SDValue &SrcR) {
  if (N.getOpcode() != ISD::ADD) {
    if (RType != LinxV5Op::NONE)
      return false;
    SrcL = N;
    SrcR = CurDAG->getRegister(LinxV5::R0, MVT::i64);
    return true;
  }

  SrcL = N.getOperand(0);
  SrcR = N.getOperand(1);

  return SelectSrcREXT<RType>(SrcR, SrcR);
}

// SelectADDRri_scaled Imm length
template <unsigned Shamt>
bool checkADDRriScaledImmlength(int64_t imm, bool isSIMT) {
  if (isSIMT)
    return isShiftedInt<24, Shamt>(imm);
  return isShiftedInt<12, Shamt>(imm);
}

// SelectADDRri_unscaled Imm length
bool checkADDRriImmlength(int64_t imm, bool isSIMT) {
  if (isSIMT)
    return isInt<24>(imm);
  return isInt<12>(imm);
}

template <unsigned Scale>
bool LinxV5DAGToDAGISel::SelectContinuousADDRri(SDValue N, SDValue &SrcL,
                                                SDValue &Imm) {
  if (!SelectContinuousADDRBase<Scale>(N, N))
    return false;
  return SelectADDRri_scaled<Scale>(N, SrcL, Imm);
}

template <unsigned Shamt>
bool LinxV5DAGToDAGISel::SelectADDRri_scaled(SDValue N, SDValue &SrcL,
                                             SDValue &Imm) {
  if (N.getOpcode() == ISD::FrameIndex) {
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SrcL = CurDAG->getTargetFrameIndex(FI, MVT::i64);
    Imm = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  if (!CurDAG->isBaseWithConstantOffset(N))
    return false;

  // TODO: How to optimize if this node is reused. like aarch64?

  if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(N.getOperand(1))) {
    int64_t imm = C->getSExtValue();
    if (checkADDRriScaledImmlength<Shamt>(imm, Subtarget->isSIMT())) {
      SrcL = N.getOperand(0);
      if (SrcL.getOpcode() == ISD::FrameIndex &&
          !Subtarget->enableLegacyISel()) {
          int FI = cast<FrameIndexSDNode>(SrcL)->getIndex();
          SrcL = CurDAG->getTargetFrameIndex(FI, MVT::i64);
      }
      Imm = CurDAG->getTargetConstant(imm >> Shamt, SDLoc(N), MVT::i64);
      return true;
    }
  }

  return false;
}

bool LinxV5DAGToDAGISel::SelectADDRri_unscaled(SDValue N, SDValue &SrcL,
                                               SDValue &Imm) {
  if (N.getOpcode() == ISD::FrameIndex) {
    int FI = cast<FrameIndexSDNode>(N)->getIndex();
    SrcL = CurDAG->getTargetFrameIndex(FI, MVT::i64);
    Imm = CurDAG->getTargetConstant(0, SDLoc(N), MVT::i64);
    return true;
  }

  if (N.getOpcode() == LinxV5ISD::ADDlo) {
    SrcL = N.getOperand(0);
    Imm = N.getOperand(1);
    return true;
  }

  if (!CurDAG->isBaseWithConstantOffset(N))
    return false;

  // TODO: How to optimize if this node is reused. like aarch64?

  if (ConstantSDNode *C = dyn_cast<ConstantSDNode>(N.getOperand(1))) {
    int64_t imm = C->getSExtValue();
    if (checkADDRriImmlength(imm, Subtarget->isSIMT())) {
      SrcL = N.getOperand(0);
      if (SrcL.getOpcode() == ISD::FrameIndex &&
          !Subtarget->enableLegacyISel()) {
          int FI = cast<FrameIndexSDNode>(SrcL)->getIndex();
          SrcL = CurDAG->getTargetFrameIndex(FI, MVT::i64);
      }
      Imm = CurDAG->getTargetConstant(imm, SDLoc(N), MVT::i64);
      return true;
    }
  }
  return false;
}

/// isIntImmediate - This method tests to see if the node is a constant
/// operand. If so Imm will receive the 32-bit value.
static bool isIntImmediate(const SDNode *N, uint64_t &Imm) {
  if (const ConstantSDNode *C = dyn_cast<const ConstantSDNode>(N)) {
    Imm = C->getZExtValue();
    return true;
  }
  return false;
}

// isIntImmediate - This method tests to see if a constant operand.
// If so Imm will receive the value.
static bool isIntImmediate(SDValue N, uint64_t &Imm) {
  return isIntImmediate(N.getNode(), Imm);
}

// isOpcWithIntImmediate - This method tests to see if the node is a specific
// opcode and that it has a immediate integer right operand.
// If so Imm will receive the 32 bit value.
static bool isOpcWithIntImmediate(const SDNode *N, unsigned Opc,
                                  uint64_t &Imm) {
  return N->getOpcode() == Opc &&
         isIntImmediate(N->getOperand(1).getNode(), Imm);
}

bool LinxV5DAGToDAGISel::SelectBXU(SDValue N, SDValue &SrcL, SDValue &ImmM,
                                   SDValue &ImmN) {
  switch (N.getOpcode()) {
  case ISD::AND:
    return SelectBXUFromAnd(N, SrcL, ImmM, ImmN);
  case ISD::SRL:
  case ISD::SRA:
    return SelectBXUFromShr(N, SrcL, ImmM, ImmN);
  default:
    return false;
  }
}

// Select (and (srl SrcL, ImmM), Mask)
bool LinxV5DAGToDAGISel::SelectBXUFromAnd(SDValue N, SDValue &SrcL,
                                          SDValue &ImmM, SDValue &ImmN) {
  uint64_t Mask = 0;
  if (!isOpcWithIntImmediate(N.getNode(), ISD::AND, Mask))
    return false;

  const SDNode *Op0 = N->getOperand(0).getNode();

  if (Mask & (Mask + 1))
    return false;

  uint64_t Nimm = countTrailingOnes(Mask);
  uint64_t Mimm = 0;
  // Handle the SRL + ANY_EXTEND case.
  if (!isOpcWithIntImmediate(Op0, ISD::SRL, Mimm))
    return false;

  SrcL = Op0->getOperand(0);
  ImmM = CurDAG->getTargetConstant(Mimm, SDLoc(N), MVT::i64);
  ImmN = CurDAG->getTargetConstant(Nimm, SDLoc(N), MVT::i64);
  return true;
}

bool LinxV5DAGToDAGISel::SelectBXUFromShr(SDValue N, SDValue &SrcL,
                                          SDValue &ImmM, SDValue &ImmN) {
  if (SelectBXUFromShrAnd(N, SrcL, ImmM, ImmN))
    return true;
  if (SelectBXFromShrShl<false>(N, SrcL, ImmM, ImmN))
    return true;
  return false;
}

// Select (srl (and SrcL, Mask), ImmM)
bool LinxV5DAGToDAGISel::SelectBXUFromShrAnd(SDValue N, SDValue &SrcL,
                                             SDValue &ImmM, SDValue &ImmN) {
  uint64_t Shift = 0;
  if (!isOpcWithIntImmediate(N.getNode(), ISD::SRL, Shift))
    return false;

  const SDNode *Op0 = N->getOperand(0).getNode();

  uint64_t Mask = 0;
  if (!isOpcWithIntImmediate(Op0, ISD::AND, Mask))
    return false;

  Mask >>= Shift;
  if (Mask & (Mask + 1))
    return false;

  uint64_t Nimm = countTrailingOnes(Mask);
  uint64_t Mimm = Shift;

  SrcL = Op0->getOperand(0);
  ImmM = CurDAG->getTargetConstant(Mimm, SDLoc(N), MVT::i64);
  ImmN = CurDAG->getTargetConstant(Nimm, SDLoc(N), MVT::i64);
  return true;
}

// Select (shr (shl SrcL, imm),  imm)
template <bool Signed>
bool LinxV5DAGToDAGISel::SelectBXFromShrShl(SDValue N, SDValue &SrcL,
                                            SDValue &ImmM, SDValue &ImmN) {
  unsigned ShrOp = Signed ? ISD::SRA : ISD::SRL;
  uint64_t ShrImm;
  if (!isOpcWithIntImmediate(N.getNode(), ShrOp, ShrImm))
    return false;

  const SDNode *Op0 = N->getOperand(0).getNode();

  uint64_t ShlImm;
  if (!isOpcWithIntImmediate(Op0, ISD::SHL, ShlImm))
    return false;

  if (ShlImm > ShrImm)
    return false;

  uint64_t Nimm = 64 - ShrImm;
  uint64_t Mimm = ShrImm - ShlImm;

  SrcL = Op0->getOperand(0);
  ImmM = CurDAG->getTargetConstant(Mimm, SDLoc(N), MVT::i64);
  ImmN = CurDAG->getTargetConstant(Nimm, SDLoc(N), MVT::i64);
  return true;
}

// Select (and SrcL, Mask)
bool LinxV5DAGToDAGISel::SelectBIC(SDValue N, SDValue &SrcL, SDValue &ImmM,
                                   SDValue &ImmN) {
  uint64_t Mask = 0;
  if (N.getOpcode() != ISD::AND ||
      !isOpcWithIntImmediate(N.getNode(), ISD::AND, Mask))
    return false;

  int pos = 0;
  bool inZeroSequence = false;
  uint64_t Mimm = -1;
  uint64_t Nimm = -1;
  while (pos < 64) {
    if ((Mask & (1ULL << pos)) == 0) {
      if (!inZeroSequence) {
          Mimm = pos;
          inZeroSequence = true;
      }
    } else {
      if (inZeroSequence) {
          Nimm = pos - Mimm;
          break;
      }
    }
    pos++;
  }
  if (inZeroSequence && Nimm == -1)
    Nimm = 64 - Mimm;
  if (Mimm == -1 || Nimm == -1 ||
      (Mask | (((1ULL << Nimm) - 1) << Mimm)) != static_cast<uint64_t>(-1))
    return false;

  const SDNode *Op1 = N->getOperand(1).getNode();

  for (auto UI = Op1->use_begin(), UE = Op1->use_end(); UI != UE; ++UI) {
    SDNode *User = *UI;
    // any other users
    if (User->getOpcode() != ISD::AND && !isInt<12>(Mask)) {
      return false;
    }
  }

  SrcL = N.getOperand(0);
  ImmM = CurDAG->getTargetConstant(Mimm, SDLoc(N), MVT::i64);
  ImmN = CurDAG->getTargetConstant(Nimm, SDLoc(N), MVT::i64);
  return true;
}

// Select (or SrcL, Mask)
bool LinxV5DAGToDAGISel::SelectBIS(SDValue N, SDValue &SrcL, SDValue &ImmM,
                                   SDValue &ImmN) {
  uint64_t Mask = 0;
  if (N.getOpcode() != ISD::OR ||
      !isOpcWithIntImmediate(N.getNode(), ISD::OR, Mask))
    return false;

  int pos = 0;
  bool inOneSequence = false;
  uint64_t Mimm = -1;
  uint64_t Nimm = -1;
  while (pos < 64) {
    if ((Mask & (1ULL << pos)) == (1ULL << pos)) {
      if (!inOneSequence) {
          Mimm = pos;
          inOneSequence = true;
      }
    } else {
      if (inOneSequence) {
          Nimm = pos - Mimm;
          break;
      }
    }
    pos++;
  }
  if (inOneSequence && Nimm == -1)
    Nimm = 64 - Mimm;
  if (Mimm == -1 || Nimm == -1 || (Mask & ~(((1ULL << Nimm) - 1) << Mimm)) != 0)
    return false;

  const SDNode *Op1 = N->getOperand(1).getNode();

  for (auto UI = Op1->use_begin(), UE = Op1->use_end(); UI != UE; ++UI) {
    SDNode *User = *UI;
    // any other users
    if (User->getOpcode() != ISD::OR && !isInt<12>(Mask)) {
      return false;
    }
  }

  SrcL = N.getOperand(0);
  ImmM = CurDAG->getTargetConstant(Mimm, SDLoc(N), MVT::i64);
  ImmN = CurDAG->getTargetConstant(Nimm, SDLoc(N), MVT::i64);
  return true;
}

// This pass converts a legalized DAG into a LinxV5-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createLinxV5ISelDag(LinxV5TargetMachine &TM) {
  return new LinxV5DAGToDAGISel(TM);
}
