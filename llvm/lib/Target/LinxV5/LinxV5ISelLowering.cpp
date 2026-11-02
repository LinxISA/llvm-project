//===-- LinxV5ISelLowering.cpp - LinxV5 DAG Lowering Implementation -----===//
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

#include "LinxV5ISelLowering.h"
#include "LinxV5.h"
#include "LinxV5MachineFunctionInfo.h"
#include "LinxV5RegisterInfo.h"
#include "LinxV5Subtarget.h"
#include "LinxV5TargetMachine.h"
#include "MCTargetDesc/LinxV5MatInt.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicsLinx.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "linxv5-lower"

STATISTIC(NumTailCalls, "Number of tail calls");

static SDValue lowerV5GMOV(SDValue Op, SelectionDAG &DAG);
static SDValue lowerV5SharedS2L(SDValue Op, SelectionDAG &DAG,
                                unsigned Function);
static SDValue lowerV5SharedL2S(SDValue Op, SelectionDAG &DAG,
                                unsigned Function);

static cl::opt<unsigned>
    MaxDepthForSearchSext("max-depth-for-linxv5-search-sext",
                          cl::desc("Max recursion depth for search sext"),
                          cl::init(4), cl::Hidden);

cl::opt<bool> EnableAllVectorAsTilereg(
    "enable-all-vector-as-tilereg",
    cl::desc("Lowering any vector type as tile register."), cl::init(false),
    cl::Hidden);

LinxV5TargetLowering::LinxV5TargetLowering(const TargetMachine &TM,
                                           const LinxV5Subtarget &STI)
    : TargetLowering(TM), Subtarget(STI) {
  LinxV5ABI::ABI ABI = Subtarget.getTargetABI();

  assert(ABI == LinxV5ABI::ABI_LP64 && "LinxV5 only support abi=lp64!");

  MVT XLenVT = Subtarget.getXLenVT();

  if (Subtarget.isSIMT()) {
    addRegisterClass(MVT::i8, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i32, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::i64, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::bf16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f16, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f32, &LinxV5::SIMTCGVRegClass);
    addRegisterClass(MVT::f64, &LinxV5::SIMTCGVRegClass);
  } else if (!Subtarget.enableLegacyISel()) {
    auto *RC = STI.getRegisterInfo()->getSTDRC();
    addRegisterClass(MVT::i64, RC);
    if (STI.hasFloat()) {
      addRegisterClass(MVT::f64, RC);
      addRegisterClass(MVT::f32, RC);
      addRegisterClass(MVT::f16, RC);
    }
  } else {
    addRegisterClass(MVT::i64, &LinxV5::VBXTRRegClass);
  }

  if (EnableAllVectorAsTilereg) {
    const TargetRegisterClass *RC = Subtarget.isSIMT()
                                        ? &LinxV5::SIMTCGVRegClass
                                        : &LinxV5::Tile_ABS_CGRegClass;
    // Registering a vector type here makes it Legal, which both routes its
    // loads/stores to the tile load/store custom lowering and lets
    // DAGCombiner merge scalar stores into that vector. Non-SIMT tile
    // registers require a B.IOT SizeCode, whose minimum is 128 B, so smaller
    // vectors (e.g. v2i64/v4i64 from merged scalar stores) must stay
    // unregistered and keep the default scalar lowering on janus. SIMT has
    // no per-tile size encoding, so it keeps registering every vector.
    for (MVT VT : MVT::integer_fixedlen_vector_valuetypes())
      if (Subtarget.isSIMT() || VT.getSizeInBits() / 8 >= 128)
        addRegisterClass(VT, RC);
    for (MVT VT : MVT::fp_fixedlen_vector_valuetypes())
      if (Subtarget.isSIMT() || VT.getSizeInBits() / 8 >= 128)
        addRegisterClass(VT, RC);
  }

  // Compute derived properties from the register classes.
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(LinxV5::R1);

  for (auto N : {ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD})
    setLoadExtAction(N, XLenVT, MVT::i1, Promote);

  // TODO: add all necessary setOperationAction calls.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, XLenVT, Expand);

  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC, XLenVT, Expand);
  setOperationAction(ISD::SELECT_CC, XLenVT, Expand);
  if (Subtarget.isSIMT() || !Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::BR_CC,
                       {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64},
                       Expand);
    setOperationAction(ISD::SELECT_CC,
                       {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64},
                       Expand);

    setOperationAction(ISD::SETCC, MVT::i1, Custom);

    static const MVT::SimpleValueType FloatVTs[] = {MVT::f16, MVT::f32,
                                                    MVT::f64};

    // Convert an integer comparison to a floating point comparison
    static const ISD::CondCode CCToExpand[] = {
        ISD::SETUEQ, ISD::SETUGT, ISD::SETUGE, ISD::SETULT,
        ISD::SETULE, ISD::SETO,   ISD::SETUO,  ISD::SETUNE};
    const auto SetCommonFCompareActions = [&](MVT VT) {
      setCondCodeAction(CCToExpand, VT, Expand);
    };

    for (MVT VT : FloatVTs) {
      SetCommonFCompareActions(VT);
    }
  }

  if (Subtarget.isScalar() && !Subtarget.enableLegacyISel()) {
    static const MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16};
    setOperationAction(ISD::FREM, {MVT::f64, MVT::f32}, Expand);
    setOperationAction(ISD::FREM, MVT::f16, Promote);
    setOperationAction(ISD::FPOW, FPVTs, Expand);
    setOperationAction(ISD::FCOPYSIGN, FPVTs, Expand);
    setOperationAction(ISD::FABS, FPVTs, Expand);
    setOperationAction(ISD::FSQRT, FPVTs, LibCall);
    setOperationAction(ISD::FEXP, FPVTs, LibCall);
    setOperationAction(ISD::FLOG, FPVTs, LibCall);
    setOperationAction(ISD::FSIN, FPVTs, LibCall);
    setOperationAction(ISD::FCOS, FPVTs, LibCall);
  }

  if (Subtarget.isSIMT()) {
    static const MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16};
    setOperationAction(ISD::FABS, FPVTs, Legal);
    setOperationAction(ISD::FSQRT, FPVTs, Legal);
    setOperationAction(ISD::FEXP, FPVTs, Legal);
    setOperationAction(ISD::FLOG2, FPVTs, Legal);
    setOperationAction(ISD::FSIN, FPVTs, Legal);
    setOperationAction(ISD::FCOS, FPVTs, Legal);
  }

  for (MVT VT : MVT::fixedlen_vector_valuetypes()) {
    setOperationAction(ISD::LOAD, VT, Custom);
    setOperationAction(ISD::STORE, VT, Custom);
  }
  setOperationAction(ISD::STACKSAVE, MVT::Other, Expand);
  setOperationAction(ISD::STACKRESTORE, MVT::Other, Expand);

  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction(ISD::VAARG, MVT::Other, Expand);
  setOperationAction(ISD::VACOPY, MVT::Other, Expand);
  setOperationAction(ISD::VAEND, MVT::Other, Expand);

  setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);
  if (Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i8, Expand);
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i16, Expand);
  }

  if (Subtarget.isScalar()) {
    setOperationAction(ISD::ADD, MVT::i32, Custom);
    setOperationAction(ISD::SUB, MVT::i32, Custom);
    setOperationAction(ISD::SHL, MVT::i32, Custom);
    setOperationAction(ISD::SRA, MVT::i32, Custom);
    setOperationAction(ISD::SRL, MVT::i32, Custom);
  }

  setLibcallName(RTLIB::MUL_I128, nullptr);
  setOperationAction(ISD::UMULO, MVT::i64, Custom);
  setOperationAction(ISD::SMULO, MVT::i64, Custom);

  setOperationAction(ISD::MULHS, XLenVT, Expand);
  setOperationAction(ISD::MULHU, XLenVT, Expand);

  if (Subtarget.isScalar()) {
    setOperationAction(ISD::MUL, MVT::i32, Custom);
    if (Subtarget.isWireless()) {
      MVT IntVTs[] = {MVT::i8, MVT::i16, MVT::i32, MVT::i64};
      setOperationAction(ISD::SDIV, IntVTs, LibCall);
      setOperationAction(ISD::UDIV, IntVTs, LibCall);
      setOperationAction(ISD::SREM, IntVTs, LibCall);
      setOperationAction(ISD::UREM, IntVTs, LibCall);
    } else {
      assert(Subtarget.isGeneric());
      MVT IntVTs[] = {MVT::i8, MVT::i16, MVT::i32};
      setOperationAction(ISD::SDIV, IntVTs, Custom);
      setOperationAction(ISD::UDIV, IntVTs, Custom);
      setOperationAction(ISD::UREM, IntVTs, Custom);
    }
  }

  setOperationAction(ISD::SDIVREM, XLenVT, Expand);
  setOperationAction(ISD::UDIVREM, XLenVT, Expand);
  setOperationAction(ISD::SMUL_LOHI, XLenVT, Expand);
  setOperationAction(ISD::UMUL_LOHI, XLenVT, Expand);

  setOperationAction(ISD::SHL_PARTS, XLenVT, Custom);
  setOperationAction(ISD::SRL_PARTS, XLenVT, Custom);
  setOperationAction(ISD::SRA_PARTS, XLenVT, Custom);

  setOperationAction(ISD::ROTL, {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64}, Expand);
  setOperationAction(ISD::ROTR, {MVT::i8, MVT::i16, MVT::i32, MVT::i64, MVT::f16,
                        MVT::bf16, MVT::f32, MVT::f64}, Expand);

  setOperationAction(ISD::BSWAP, XLenVT, Expand);

  // PTO ISA has scalar ctz/clz/bcnt (ctz/clz/bcnt SrcL, M, N, ->Rd). Lower
  // the count-trailing/leading/popcount ops to those instructions with the
  // full XLEN-wide field (M=0, N=XLEN) instead of Expand→SWAR.
  // Note: the ISA ctz returns N for an all-zero field, which matches both
  // ISD::CTTZ (must return XLEN) and CTTZ_ZERO_UNDEF (any value is legal).
  setOperationAction(ISD::CTTZ, XLenVT, Legal);
  setOperationAction(ISD::CTLZ, XLenVT, Legal);
  setOperationAction(ISD::CTPOP, XLenVT, Legal);

  if (!Subtarget.isSIMT() &&
      (Subtarget.enableLegacyISel() || !Subtarget.hasCSel()))
    setOperationAction(ISD::SELECT, XLenVT, Custom);

  if (Subtarget.isScalar() && Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::FP_TO_SINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i32, Custom);
  } else if (Subtarget.isScalar() && !Subtarget.enableLegacyISel()) {
    setOperationAction(ISD::BITCAST, {MVT::i32, MVT::i16}, Custom);
    setOperationAction(ISD::FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::FP_TO_SINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i32, Custom);
    if (Subtarget.hasFloat()) {
      setOperationAction(ISD::FP_TO_UINT, MVT::i16, Custom);
      setOperationAction(ISD::FP_TO_SINT, MVT::i16, Custom);
      setOperationAction(ISD::STRICT_FP_TO_UINT, MVT::i16, Custom);
      setOperationAction(ISD::STRICT_FP_TO_SINT, MVT::i16, Custom);
    }
  }

  if (Subtarget.isSIMT() || !Subtarget.enableLegacyISel()) {
    // Linxv4 simt block do not have extload or trunc store for floating-point.
    // Expand them.
    // Fill illegal fields of ActionTable is OK. It is unreachable.
    for (auto ValVT : MVT::fp_valuetypes()) {
      for (auto MemVT : MVT::fp_valuetypes()) {
        setLoadExtAction(ISD::EXTLOAD, ValVT, MemVT, Expand);
        setTruncStoreAction(ValVT, MemVT, Expand);
      }
    }

    MVT FPVTs[] = {MVT::f64, MVT::f32, MVT::f16, MVT::bf16};

    setOperationAction(ISD::STRICT_FMA, FPVTs, Legal);
    setOperationAction(ISD::FMA, FPVTs, Legal);
  }

  if (Subtarget.isScalar() && !Subtarget.enableLegacyISel()) {
    // Keep f16 carriers legal for the ABI, loads/stores and conversions, but
    // never select the reserved FSU SrcType=2 arithmetic encodings. Operation
    // promotion inserts an explicit FP_ROUND back to half after each result.
    // Use double precision rather than an intermediate single rounding.
    for (unsigned Opc : {ISD::FADD, ISD::FSUB, ISD::FMUL, ISD::FDIV,
                         ISD::FMA, ISD::STRICT_FADD, ISD::STRICT_FSUB,
                         ISD::STRICT_FMUL, ISD::STRICT_FDIV, ISD::STRICT_FMA})
      setOperationPromotedToType(Opc, MVT::f16, MVT::f64);
    setOperationAction(ISD::SETCC, MVT::f16, Promote);
    // Negation is a bit operation; it must preserve signed zero and NaNs.
    setOperationAction(ISD::FNEG, MVT::f16, Custom);
  }

  setOperationAction(ISD::GlobalAddress, XLenVT, Custom);
  setOperationAction(ISD::BlockAddress, XLenVT, Custom);
  setOperationAction(ISD::ConstantPool, XLenVT, Custom);
  setOperationAction(ISD::JumpTable, XLenVT, Custom);

  setOperationAction(ISD::GlobalTLSAddress, XLenVT, Custom);

  // PTO BWT is a nonblocking wait request, not a trap. Use LLVM's abort
  // fallback rather than manufacturing trap semantics for that opcode.
  setOperationAction(ISD::TRAP, MVT::Other, Expand);
  setOperationAction(ISD::DEBUGTRAP, MVT::Other, Expand);

  // Native scalar atomics access exactly their IR width, including byte and
  // halfword CAS. Keep the existing libcall path for legacy and SIMT functions.
  bool NativeScalar = Subtarget.isScalar() && !Subtarget.enableLegacyISel();
  setMaxAtomicSizeInBitsSupported(NativeScalar ? 64 : 0);
  if (NativeScalar) {
    setMinCmpXchgSizeInBits(8);
    for (unsigned Opc : {ISD::ATOMIC_CMP_SWAP,
                         ISD::ATOMIC_CMP_SWAP_WITH_SUCCESS, ISD::ATOMIC_SWAP,
                         ISD::ATOMIC_LOAD_ADD, ISD::ATOMIC_LOAD_SUB,
                         ISD::ATOMIC_LOAD_AND, ISD::ATOMIC_LOAD_OR,
                         ISD::ATOMIC_LOAD_XOR, ISD::ATOMIC_LOAD_MIN,
                         ISD::ATOMIC_LOAD_MAX, ISD::ATOMIC_LOAD_UMIN,
                         ISD::ATOMIC_LOAD_UMAX, ISD::ATOMIC_LOAD,
                         ISD::ATOMIC_STORE})
      setOperationAction(Opc, MVT::i64, Legal);
    setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Legal);
    setOperationAction(ISD::PREFETCH, MVT::Other, Legal);
  }

  setBooleanContents(ZeroOrOneBooleanContent);
  MVT INTVTs[] = {MVT::Other, MVT::i1, MVT::i8,  MVT::i16, MVT::i32, MVT::i64,
                  MVT::f16,   MVT::f32, MVT::f64, MVT::bf16};
  setOperationAction(ISD::INTRINSIC_W_CHAIN, INTVTs, Custom);

  setOperationAction(ISD::INTRINSIC_WO_CHAIN, INTVTs, Custom);

  setOperationAction(ISD::INTRINSIC_VOID, INTVTs, Custom);

  // Function alignments.
  const Align FunctionAlignment(2);
  setMinFunctionAlignment(FunctionAlignment);
  setPrefFunctionAlignment(FunctionAlignment);

  setMinimumJumpTableEntries(5);

  // Jumps are expensive, compared to logic
  setJumpIsExpensive();

  // We can use any register for comparisons
  setHasMultipleConditionRegisters();

  setTargetDAGCombine(ISD::SETCC);

  setTargetDAGCombine(ISD::ZERO_EXTEND);

  if (!Subtarget.enableLegacyISel())
    setTargetDAGCombine(ISD::GlobalAddress);
}

TargetLowering::AtomicExpansionKind
LinxV5TargetLowering::shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const {
  if (!Subtarget.isScalar() || Subtarget.enableLegacyISel())
    return TargetLowering::shouldExpandAtomicRMWInIR(AI);
  unsigned Bits = AI->getType()->getPrimitiveSizeInBits();
  auto Op = AI->getOperation();
  if (Op == AtomicRMWInst::Xchg)
    return AtomicExpansionKind::None;
  // Store-only byte/halfword add/sub have no acquire bit. Preserve these
  // only when the old value is unused and no acquire semantics are needed.
  if (Bits < 32 && AI->use_empty() &&
      !isAcquireOrStronger(AI->getOrdering()) &&
      (Op == AtomicRMWInst::Add || Op == AtomicRMWInst::Sub))
    return AtomicExpansionKind::None;
  if (AI->isFloatingPointOperation() || Op == AtomicRMWInst::Nand || Bits < 32)
    return AtomicExpansionKind::CmpXChg;
  return AtomicExpansionKind::None;
}

const char *LinxV5TargetLowering::getTargetNodeName(unsigned Opcode) const {
#define MAKE_CASE(V)                                                           \
  case V:                                                                      \
    return #V;
  switch ((LinxV5ISD::NodeType)Opcode) {
  case LinxV5ISD::FIRST_NUMBER:
    break;
    MAKE_CASE(LinxV5ISD::RET_FLAG)
    MAKE_CASE(LinxV5ISD::URET_FLAG)
    MAKE_CASE(LinxV5ISD::SRET_FLAG)
    MAKE_CASE(LinxV5ISD::MRET_FLAG)
    MAKE_CASE(LinxV5ISD::CALL)
    MAKE_CASE(LinxV5ISD::SELECT_CC)
    MAKE_CASE(LinxV5ISD::BuildPairF64)
    MAKE_CASE(LinxV5ISD::TAIL)
    MAKE_CASE(LinxV5ISD::SLLW)
    MAKE_CASE(LinxV5ISD::SRAW)
    MAKE_CASE(LinxV5ISD::SRLW)
    MAKE_CASE(LinxV5ISD::DIVW)
    MAKE_CASE(LinxV5ISD::DIVUW)
    MAKE_CASE(LinxV5ISD::REMUW)
    MAKE_CASE(LinxV5ISD::FCVTU)
    MAKE_CASE(LinxV5ISD::FCVTS)
    MAKE_CASE(LinxV5ISD::BITCAST)
    MAKE_CASE(LinxV5ISD::FunctionBlock)
    MAKE_CASE(LinxV5ISD::ADDlo)
    MAKE_CASE(LinxV5ISD::LOOPSET)
    MAKE_CASE(LinxV5ISD::RDADD)
    MAKE_CASE(LinxV5ISD::RDOR)
    MAKE_CASE(LinxV5ISD::RDMAX)
    MAKE_CASE(LinxV5ISD::RDMIN)
    MAKE_CASE(LinxV5ISD::SHFLUP)
    MAKE_CASE(LinxV5ISD::MAX)
    MAKE_CASE(LinxV5ISD::MIN)
    MAKE_CASE(LinxV5ISD::SHFLDOWN)
    MAKE_CASE(LinxV5ISD::SHFLIDX)
    MAKE_CASE(LinxV5ISD::SHFLXOR)
    MAKE_CASE(LinxV5ISD::VCALL)
    MAKE_CASE(LinxV5ISD::MCALL)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TADD_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TSUB_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TBINARY)
    MAKE_CASE(LinxV5ISD::EW_TBINARY_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TNEG_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TCVT_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TLEA)
    MAKE_CASE(LinxV5ISD::EW_TCI)
    MAKE_CASE(LinxV5ISD::EW_TCI_ROW)
    MAKE_CASE(LinxV5ISD::EW_TCMPS_GPR)
    MAKE_CASE(LinxV5ISD::EW_TCMPS_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_INDEXED_RANGE)
    MAKE_CASE(LinxV5ISD::EW_MGATHER_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_MGATHER_MASKED)
    MAKE_CASE(LinxV5ISD::EW_MSCATTER_MASKED)
    MAKE_CASE(LinxV5ISD::EW_MSCATTER_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_MGATHER_ADD)
    MAKE_CASE(LinxV5ISD::EW_MGATHER_ADD_MASKED)
    MAKE_CASE(LinxV5ISD::EW_MSCATTER_ADD)
    MAKE_CASE(LinxV5ISD::EW_MSCATTER_ADD_PREDICATE)
    MAKE_CASE(LinxV5ISD::EW_TCMP)
    MAKE_CASE(LinxV5ISD::EW_TCMP_ROW)
    MAKE_CASE(LinxV5ISD::EW_TCMP_GPR)
    MAKE_CASE(LinxV5ISD::EW_TCMP_GPR_MASKED)
    MAKE_CASE(LinxV5ISD::EW_TSEL)
    MAKE_CASE(LinxV5ISD::EW_TSEL_ROW)
    MAKE_CASE(LinxV5ISD::EW_TSEL_GPR)
    MAKE_CASE(LinxV5ISD::EW_TEXPANDS)
    MAKE_CASE(LinxV5ISD::EW_TEXPANDS_ROW)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMX)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMXB)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMX_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMULMXB_AC)
    MAKE_CASE(LinxV5ISD::BLK_MATMUL_SHARED)
    MAKE_CASE(LinxV5ISD::BLK_TLOAD)
    MAKE_CASE(LinxV5ISD::BLK_TSTORE)
    MAKE_CASE(LinxV5ISD::BLK_ACCCVT)
    MAKE_CASE(LinxV5ISD::V5_GMOV)
    MAKE_CASE(LinxV5ISD::V5_SHARED_L2S)
    MAKE_CASE(LinxV5ISD::V5_SHARED_S2L)
    MAKE_CASE(LinxV5ISD::FABS)
    MAKE_CASE(LinxV5ISD::FSQRT)
    MAKE_CASE(LinxV5ISD::FEXP)
    MAKE_CASE(LinxV5ISD::MERGE_PREDICATION)
    MAKE_CASE(LinxV5ISD::CopyP)
    MAKE_CASE(LinxV5ISD::Copy2P)
    MAKE_CASE(LinxV5ISD::Copy2PTerm)
    MAKE_CASE(LinxV5ISD::MERGE_CF)
    MAKE_CASE(LinxV5ISD::IMPLICIT_DEF)
    MAKE_CASE(LinxV5ISD::SYNC_CORE)
  }
#undef MAKE_CASE
  return nullptr;
}

EVT LinxV5TargetLowering::getSetCCResultType(const DataLayout &DL,
                                             LLVMContext &, EVT VT) const {
  if (!VT.isVector())
    return getPointerTy(DL);
  return VT.changeVectorElementTypeToInteger();
}

bool LinxV5TargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                 const AddrMode &AM, Type *Ty,
                                                 unsigned AS,
                                                 Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // Require a 12-bit signed offset.
  if (!isInt<12>(AM.BaseOffs))
    return false;

  // Unscaled reg + imm12 case.
  if (AM.Scale == 0)
    return true;

  // Scaled reg + imm12 case.
  if (AM.Scale == 1 && !AM.HasBaseReg)
    return true;

  if (AM.BaseOffs != 0)
    return false;
  uint64_t NumBytes = 0;
  if (Ty->isSized()) {
    uint64_t NumBits = DL.getTypeSizeInBits(Ty);
    NumBytes = NumBits / 8;
    if (!isPowerOf2_64(NumBits))
      NumBytes = 0;
  }
  return static_cast<uint64_t>(AM.Scale) == NumBytes && AM.HasBaseReg;
}

bool LinxV5TargetLowering::isLegalICmpImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

bool LinxV5TargetLowering::isLegalAddImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

// The backend supports 16, 32 and 64 bit floating point immediates.
bool LinxV5TargetLowering::isFPImmLegal(const APFloat &Imm, EVT VT,
                                        bool ForCodeSize) const {
  if (Subtarget.isSIMT()) {
    EVT ScalarVT = VT.getScalarType();
    assert(
        ScalarVT == MVT::f32 || ScalarVT == MVT::f64 || ScalarVT == MVT::f16 ||
        ScalarVT == MVT::bf16 && "Unsupported FP immediate type in SIMT mode");
    return true;
  }
  return false;
}

bool LinxV5TargetLowering::isZExtFree(SDValue Val, EVT VT2) const {
  // Zexts are free if they can be combined with a load.
  if (auto *LD = dyn_cast<LoadSDNode>(Val)) {
    EVT MemVT = LD->getMemoryVT();
    if ((MemVT == MVT::i8 || MemVT == MVT::i16 || MemVT == MVT::i32) &&
        (LD->getExtensionType() == ISD::NON_EXTLOAD ||
         LD->getExtensionType() == ISD::ZEXTLOAD))
      return true;
  }

  return TargetLowering::isZExtFree(Val, VT2);
}

bool LinxV5TargetLowering::isSExtFree(SDValue Val) const {
  // Sexts are free if they are done on the results of the following operators:
  // ADD/SUB/AND/OR/XOR/SRL/SRA/SHL
  SDNode *N = Val.getNode();
  unsigned Opc = N->getOpcode();
  switch (Opc) {
  case ISD::ADD:
  case ISD::SUB:
  case ISD::AND:
  case ISD::OR:
  case ISD::XOR:
  case ISD::SRL:
  case ISD::SRA:
  case ISD::SHL:
    return true;
  default:
    return false;
  }
}

bool LinxV5TargetLowering::isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const {
  return SrcVT == MVT::i32 && DstVT == MVT::i64;
}

bool LinxV5TargetLowering::isIntDivCheap(EVT VT, AttributeList Attr) const {
  return false;
}

bool LinxV5TargetLowering::signExtendConstant(const ConstantInt *CI) const {
  return CI->getType()->isIntegerTy(32);
}

bool LinxV5TargetLowering::isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                                      EVT VT) const {
  if (!VT.isSimple())
    return false;

  if (Subtarget.isScalar() && Subtarget.enableLegacyISel())
    // VBXISel do not support fma
    return false;

  switch (VT.getSimpleVT().SimpleTy) {
  case MVT::f64:
  case MVT::f32:
  case MVT::f16:
  case MVT::bf16:
    return Subtarget.hasFloat();
  default:
    return false;
  }
}

bool LinxV5TargetLowering::isSDNodeAlwaysUniform(const SDNode *N) const {
  switch (N->getOpcode()) {
  case ISD::EntryToken:
  case ISD::TokenFactor:
    return true;
  case ISD::INTRINSIC_W_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(1));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch(IntID) {
    case Intrinsic::blkv_if:
    case Intrinsic::blkv_flow:
    case Intrinsic::blkv_if_break:
    case Intrinsic::blkv_loop:
    case Intrinsic::blkv_end_cf:
    case Intrinsic::linx_blkv_rdmax:
    case Intrinsic::linx_blkv_rdmin:
    case Intrinsic::linx_blkv_rdadd:
    case Intrinsic::linx_blkv_rdor:
      return true;
    }
    return false;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch (IntID) {
    case Intrinsic::blkv_get_tile_ptr:
      return true;
    }
    return false;
  }
  case LinxV5ISD::MERGE_PREDICATION:
  case LinxV5ISD::Copy2P:
  case LinxV5ISD::Copy2PTerm:
  case LinxV5ISD::CopyP:
  case LinxV5ISD::IMPLICIT_DEF:
  case LinxV5ISD::RDADD:
  case LinxV5ISD::RDOR:
  case LinxV5ISD::RDMAX:
  case LinxV5ISD::RDMIN:
    return true;
  }
  if (N->isMachineOpcode()) {
    switch (N->getMachineOpcode()) {
      case LinxV5::LinxV5PseudoMergePred:
        return true;
    }
  }
  return false;
}

const TargetRegisterClass *
LinxV5TargetLowering::getRegClassFor(MVT VT, bool isDivergent) const {
  if (!Subtarget.isSIMT())
    return TargetLowering::getRegClassFor(VT, isDivergent);
  // TODO: Need return simtcgv or simtcgs by isDivergent after all uniform
  // instruction is supported.
  if (!isDivergent)
      return &LinxV5::SIMTCGSRegClass;
  return &LinxV5::SIMTCGVRegClass;
}

bool LinxV5TargetLowering::isSDNodeSourceOfDivergence(
    const SDNode *N, FunctionLoweringInfo *FLI,
    LegacyDivergenceAnalysis *KDA) const {
  return isSDNodeSourceOfDivergenceImpl(N, FLI, KDA, false);
}

bool LinxV5TargetLowering::isSDNodeSourceOfDivergenceImpl(
    const SDNode *N, FunctionLoweringInfo *FLI, LegacyDivergenceAnalysis *KDA,
    bool XDivergence) const {
  switch (N->getOpcode()) {
  case ISD::CopyFromReg: {
    const RegisterSDNode *R = cast<RegisterSDNode>(N->getOperand(1));
    const MachineRegisterInfo &MRI = FLI->MF->getRegInfo();
    const LinxV5RegisterInfo *TRI = Subtarget.getRegisterInfo();
    Register Reg = R->getReg();
    bool TreatLC12AsUniform = XDivergence || Subtarget.enableContinuousMemOpt();

    // LiveIns of block C function must be uniform
    if (MRI.isLiveIn(Reg))
      return false;

    if (Reg.isPhysical())
      return !TRI->isUniformReg(MRI, Reg, TreatLC12AsUniform);

    if (const Value *V = FLI->getValueFromVirtualReg(R->getReg()))
      return KDA->isDivergent(V);

    // assert(Reg == FLI->DemoteRegister || isCopyFromRegOfInlineAsm(N));
    return !TRI->isUniformReg(MRI, Reg, TreatLC12AsUniform);
  }
  case ISD::LOAD: {
    return true;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(N->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    switch(IntID) {
      case Intrinsic::blkv_get_index_x:
      return true;
      case Intrinsic::blkv_get_index_y:
      case Intrinsic::blkv_get_index_z:
      return !XDivergence;
      default:
        return false;
    }
    return false;
  }
  case LinxV5ISD::MERGE_CF:
    // MERGE_CF lowers to SIMT_PSEL, which selects SrcL for mask-active lanes and
    // SrcR for inactive lanes. The result is divergent per-lane even when both
    // operands are uniform (e.g., the i1 merge_cf used to compute a loop exit
    // condition in blkv_if_break). Without this, an any_extend of the i8 PSEL
    // result would match UniformUnaryOp and emit a scalar SIMT_ICVT_*_SCAR,
    // creating an invalid VecReg→ScalarReg copy. Marking MERGE_CF as a source
    // of divergence forces the vector (divergent) ICVT path.
    return true;
  default:
    return false;
  }
}

// Changes the condition code and swaps operands if necessary, so the SetCC
// operation matches one of the comparisons supported directly in the LinxV5
// ISA.
static void normaliseSetCC(SDValue &LHS, SDValue &RHS, ISD::CondCode &CC) {
  switch (CC) {
  default:
    break;
  case ISD::SETGT:
  case ISD::SETLE:
  case ISD::SETUGT:
  case ISD::SETULE:
    CC = ISD::getSetCCSwappedOperands(CC);
    std::swap(LHS, RHS);
    break;
  }
}

// Return the LinxV5 branch opcode that matches the given DAG integer
// condition code. The CondCode must be one of those supported by the LinxV5
// ISA (see normaliseSetCC).
static unsigned getVBXBranchOpcodeForIntCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return LinxV5::PseudoVBXBEQ;
  case ISD::SETNE:
    return LinxV5::PseudoVBXBNE;
  case ISD::SETLT:
    return LinxV5::PseudoVBXBLT;
  case ISD::SETGE:
    return LinxV5::PseudoVBXBGE;
  case ISD::SETULT:
    return LinxV5::PseudoVBXBLTU;
  case ISD::SETUGE:
    return LinxV5::PseudoVBXBGEU;
  default:
    llvm_unreachable("Unsupported CondCode");
  }
}

static unsigned getBranchOpcodeForIntCondCode(ISD::CondCode CC) {
  switch (CC) {
  case ISD::SETEQ:
    return LinxV5::SETC_EQ_BR;
  case ISD::SETNE:
    return LinxV5::SETC_NE_BR;
  case ISD::SETLT:
    return LinxV5::SETC_LT_BR;
  case ISD::SETGE:
    return LinxV5::SETC_GE_BR;
  case ISD::SETULT:
    return LinxV5::SETC_LTU_BR;
  case ISD::SETUGE:
    return LinxV5::SETC_GEU_BR;
  default:
    llvm_unreachable("Unsupported CondCode");
  }
}

static void expandMUL128(SelectionDAG &DAG, SDValue LL, SDValue LH, SDValue RL,
                         SDValue RH, SDLoc &dl, EVT NVT, SDValue &Lo,
                         SDValue &Hi) {
  unsigned Bits = NVT.getSizeInBits();
  unsigned HalfBits = Bits >> 1;
  SDValue Mask = DAG.getConstant(APInt::getLowBitsSet(Bits, HalfBits), dl, NVT);
  SDValue LLL = DAG.getNode(ISD::AND, dl, NVT, LL, Mask);
  SDValue RLL = DAG.getNode(ISD::AND, dl, NVT, RL, Mask);

  SDValue T = DAG.getNode(ISD::MUL, dl, NVT, LLL, RLL);
  SDValue TL = DAG.getNode(ISD::AND, dl, NVT, T, Mask);

  SDValue Shift = DAG.getShiftAmountConstant(HalfBits, NVT, dl);
  SDValue TH = DAG.getNode(ISD::SRL, dl, NVT, T, Shift);
  SDValue LLH = DAG.getNode(ISD::SRL, dl, NVT, LL, Shift);
  SDValue RLH = DAG.getNode(ISD::SRL, dl, NVT, RL, Shift);

  SDValue U = DAG.getNode(ISD::ADD, dl, NVT,
                          DAG.getNode(ISD::MUL, dl, NVT, LLH, RLL), TH);
  SDValue UL = DAG.getNode(ISD::AND, dl, NVT, U, Mask);
  SDValue UH = DAG.getNode(ISD::SRL, dl, NVT, U, Shift);

  SDValue V = DAG.getNode(ISD::ADD, dl, NVT,
                          DAG.getNode(ISD::MUL, dl, NVT, LLL, RLH), UL);
  SDValue VH = DAG.getNode(ISD::SRL, dl, NVT, V, Shift);

  SDValue W =
      DAG.getNode(ISD::ADD, dl, NVT, DAG.getNode(ISD::MUL, dl, NVT, LLH, RLH),
                  DAG.getNode(ISD::ADD, dl, NVT, UH, VH));
  Lo = DAG.getNode(ISD::ADD, dl, NVT, TL,
                   DAG.getNode(ISD::SHL, dl, NVT, V, Shift));

  Hi = DAG.getNode(ISD::ADD, dl, NVT, W,
                   DAG.getNode(ISD::ADD, dl, NVT,
                               DAG.getNode(ISD::MUL, dl, NVT, RH, LL),
                               DAG.getNode(ISD::MUL, dl, NVT, RL, LH)));
}

SDValue LinxV5TargetLowering::lowerXMULO(SDValue Op, SelectionDAG &DAG) const {
  unsigned opcode = Op.getOpcode();
  assert((opcode == ISD::UMULO || opcode == ISD::SMULO) && "Invalid Opcode.");

  bool isSigned = (opcode == ISD::SMULO);
  EVT VT = MVT::i64;
  SDLoc dl(Op);
  SDValue LHS = Op.getOperand(0);

  if (LHS.getValueType() != VT)
    return Op;

  SDValue ShiftAmt = DAG.getConstant(63, dl, VT);

  SDValue RHS = Op.getOperand(1);
  SDValue HiLHS, HiRHS;
  if (isSigned) {
    HiLHS = DAG.getNode(ISD::SRA, dl, VT, LHS, ShiftAmt);
    HiRHS = DAG.getNode(ISD::SRA, dl, MVT::i64, RHS, ShiftAmt);
  } else {
    HiLHS = DAG.getConstant(0, dl, VT);
    HiRHS = DAG.getConstant(0, dl, MVT::i64);
  }

  SDValue LO;
  SDValue HI;
  expandMUL128(DAG, LHS, HiLHS, RHS, HiRHS, dl, VT, LO, HI);
  if (isSigned) {
    SDValue Tmp1 = DAG.getNode(ISD::SRA, dl, VT, LO, ShiftAmt);
    HI = DAG.getSetCC(dl, MVT::i64, HI, Tmp1, ISD::SETNE);
  } else {
    HI = DAG.getSetCC(dl, MVT::i64, HI, DAG.getConstant(0, dl, VT), ISD::SETNE);
  }
  SDValue Ops[2] = {LO, HI};
  SDValue Res = DAG.getMergeValues(Ops, dl);
  return Res;
}

/// When intrinsic with varargs, it is not treat as common ISD intrinsic node.
TargetLowering::LegalizeAction
LinxV5TargetLowering::getCustomOperationAction(SDNode &Op) const {
  // TODO: Check for linx kernel launch
  return Legal;
}

static bool isSIMT(const SelectionDAG &DAG) {
  return DAG.getSubtarget<LinxV5Subtarget>().isSIMT();
}

SDValue LinxV5TargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::SMULO:
  case ISD::UMULO:
    return lowerXMULO(Op, DAG);
  case ISD::GlobalAddress:
    return lowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return lowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return lowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return lowerJumpTable(Op, DAG);
  case ISD::GlobalTLSAddress:
    return lowerGlobalTLSAddress(Op, DAG);
  case ISD::SELECT:
    return lowerSELECT(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::FRAMEADDR:
    return lowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return lowerRETURNADDR(Op, DAG);
  case ISD::SHL_PARTS:
    return lowerShiftLeftParts(Op, DAG);
  case ISD::SRA_PARTS:
    return lowerShiftRightParts(Op, DAG, true);
  case ISD::SRL_PARTS:
    return lowerShiftRightParts(Op, DAG, false);
  case ISD::LOAD:
    return lowerLOAD(Op, DAG);
  case ISD::STORE:
    return lowerSTORE(Op, DAG);
  case ISD::FNEG: {
    // Half carriers live in the low 16 bits of a scalar GPR. Avoid the
    // generic stack-based bitcast expansion and preserve all payload bits.
    SDLoc DL(Op);
    SDValue Bits = DAG.getNode(LinxV5ISD::BITCAST, DL, MVT::i64,
                               Op.getOperand(0));
    Bits = DAG.getNode(ISD::XOR, DL, MVT::i64, Bits,
                       DAG.getConstant(0x8000, DL, MVT::i64));
    return DAG.getNode(LinxV5ISD::BITCAST, DL, MVT::f16, Bits);
  }
  case ISD::BITCAST: {
    if (!Subtarget.enableLegacyISel()) {
      SDLoc DL(Op);
      EVT VT = Op.getValueType();
      if (VT.isVector())
        return SDValue();
      SDValue Op0 = Op.getOperand(0);
      EVT Op0VT = Op0.getValueType();
      assert(Op0VT.isInteger() && "unexpected ValueType!");
      if (Op0VT.getSimpleVT() != MVT::i64) {
        Op0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, Op0);
      }
      return DAG.getNode(LinxV5ISD::BITCAST, DL, VT, Op0);
    }
    return SDValue();
  }
  case ISD::INTRINSIC_W_CHAIN: {
    ConstantSDNode *CN = cast<ConstantSDNode>(Op->getOperand(1));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(Op);
    auto calcNumTileDefs = [](SDValue Op) {
      unsigned dNum = 0;
      for (auto &V : Op->values()) {
        if (V.isVector())
          ++dNum;
      }
      return dNum;
    };
    auto calcNumTileUses = [](SDValue Op) {
      unsigned uNum = 0;
      for (auto &O : Op->ops()) {
        if (O.getValueType().isVector())
          ++uNum;
      }
      return uNum;
    };
    if (Intrinsic::getBaseName(IntID).startswith("llvm.linx.vcall.par")) {
      unsigned dNum = calcNumTileDefs(Op);
      unsigned uNum = calcNumTileUses(Op);
      return lowerTileOpWithBody(DL, Op, dNum, uNum, DAG, LinxV5ISD::VCALL);
    } else if (Intrinsic::getBaseName(IntID).startswith(
                   "llvm.linx.mcall.par")) {
      unsigned dNum = calcNumTileDefs(Op);
      unsigned uNum = calcNumTileUses(Op);
      return lowerTileOpWithBody(DL, Op, dNum, uNum, DAG, LinxV5ISD::MCALL);
    }
    if (IntID == Intrinsic::linx_scalar_fsqrt ||
        IntID == Intrinsic::linx_scalar_fexp) {
      EVT VT = Op.getValueType();
      if (!Subtarget.isScalar() || Subtarget.enableLegacyISel() ||
          !Subtarget.hasFloat() || (VT != MVT::f32 && VT != MVT::f64))
        report_fatal_error("scalar FP intrinsic requires native scalar f32/f64");
      unsigned Opc = IntID == Intrinsic::linx_scalar_fsqrt
                         ? (VT == MVT::f32 ? LinxV5::FSQRT_FS : LinxV5::FSQRT_FD)
                         : (VT == MVT::f32 ? LinxV5::FEXP_FS : LinxV5::FEXP_FD);
      return SDValue(DAG.getMachineNode(Opc, DL, DAG.getVTList(VT, MVT::Other),
                                       {Op.getOperand(2), Op.getOperand(0)}), 0);
    }
    switch (IntID) {
    default: {
      Op->print(errs(), &DAG);
      report_fatal_error("unimplemented Intrinsic operand");
    }
    case Intrinsic::linx_get_thread_id:
    case Intrinsic::linx_get_thread_idx:
      return lowerGetThreadIdx(Op, DAG);
    case Intrinsic::linx_v5_gmov:
      return lowerV5GMOV(Op, DAG);
    case Intrinsic::linx_v5_shared_l2s_insert:
      return lowerV5SharedL2S(Op, DAG,
                              LinxV5Op::TileOPTMA::TMOV);
    case Intrinsic::linx_v5_shared_l2s_publish:
      return lowerV5SharedL2S(Op, DAG,
                              LinxV5Op::TileOPTMA::TMOV);
    case Intrinsic::linx_v5_shared_s2l_broadcast:
      return lowerV5SharedS2L(Op, DAG, LinxV5Op::TileOPTMA::TMOV);
    case Intrinsic::linx_v5_shared_s2l_extract:
      return lowerV5SharedS2L(Op, DAG, LinxV5Op::TileOPTMA::TMOV);
    case Intrinsic::linx_get_simt_ret:
      return lowerGetSIMTRet(Op, DAG);
    case Intrinsic::linx_get_sysreg: {
      return lowerSysGet(Op, DAG);
    }
    case Intrinsic::linx_shuffle_up:
      return lowerShuffle(LinxV5ISD::SHFLUP, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_down:
      return lowerShuffle(LinxV5ISD::SHFLDOWN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_idx:
      return lowerShuffle(LinxV5ISD::SHFLIDX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_shuffle_bfly:
      return lowerShuffle(LinxV5ISD::SHFLXOR, DL, Op.getOperand(0), Op, DAG);
case Intrinsic::linx_blk_matmul:
      return lowerTemplateBLK(LinxV5ISD::BLK_MATMUL, DL, Op, 2, DAG);
    case Intrinsic::linx_experimental_ew_cube_matmul_masked:
      return lowerTemplateBLKMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tadd_masked:
      return lowerElementwiseTAddMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tsub_masked:
      return lowerElementwiseTSubMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tbinary:
      return lowerElementwiseTBinary(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tbinary_gpr_masked:
      return lowerElementwiseTBinaryGPRMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tneg_gpr_masked:
      return lowerElementwiseTNegGPRMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tcvt_gpr_masked:
      return lowerElementwiseTCVTGPRMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tlea:
      return lowerElementwiseTLEA(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tci:
      return lowerElementwiseTCI(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tci_row:
      return lowerElementwiseTCI(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_tcmps_gpr_masked:
      return lowerElementwiseTCMPSGPR(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_tcmps_gpr:
      return lowerElementwiseTCMPSGPR(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mgather_masked_range:
    case Intrinsic::linx_experimental_ew_mgather_gpr_masked_range:
    case Intrinsic::linx_experimental_ew_mgather_add_range:
    case Intrinsic::linx_experimental_ew_mgather_add_masked_range:
      return lowerIndexedTLSURange(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mgather_gpr_masked:
      return lowerElementwiseMGatherGPRMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mgather_tile_masked:
      return lowerElementwiseMGatherMasked(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_mgather_masked:
      return lowerElementwiseMGatherMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mgather_add:
      return lowerElementwiseMGatherAdd(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mgather_add_masked:
      return lowerElementwiseMGatherAddMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mscatter_masked:
      return lowerElementwiseMScatterMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mscatter_gpr_masked:
      return lowerElementwiseMScatterGPRMasked(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_mscatter_add:
      return lowerElementwiseMScatterAdd(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tcmp:
      return lowerElementwiseTCmp(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tcmp_row:
      return lowerElementwiseTCmp(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_tcmp_gpr_masked:
      return lowerElementwiseTCmpGPR(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_tcmp_gpr:
      return lowerElementwiseTCmpGPR(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tsel:
      return lowerElementwiseTSel(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_tsel_row:
      return lowerElementwiseTSel(DL, Op, DAG, true);
    case Intrinsic::linx_experimental_ew_tsel_gpr:
      return lowerElementwiseTSelGPR(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_texpands:
      return lowerElementwiseTExpands(DL, Op, DAG);
    case Intrinsic::linx_experimental_ew_texpands_row:
      return lowerElementwiseTExpands(DL, Op, DAG, true);
    case Intrinsic::linx_blk_matmul_shared:
      return lowerTemplateBLKShared(DL, Op, DAG);
    case Intrinsic::linx_blk_matmul_ac:
      return lowerTemplateBLK(LinxV5ISD::BLK_MATMUL_AC, DL, Op, 3, DAG);
    case Intrinsic::linx_blk_matmulmx:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMX, DL, Op, 4, DAG);
    case Intrinsic::linx_blk_matmulmxb:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMXB, DL, Op, 3, DAG);
    case Intrinsic::linx_blk_matmulmx_ac:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMX_AC, DL, Op, 5, DAG);
    case Intrinsic::linx_blk_matmulmxb_ac:
      return lowerTemplateBLKMX(LinxV5ISD::BLK_MATMULMXB_AC, DL, Op, 4, DAG);
    case Intrinsic::linx_blk_tload:
      return lowerTLoad(LinxV5ISD::BLK_TLOAD, DL, Op, DAG);
    case Intrinsic::linx_blk_acccvt:
      return lowerACCCVT(DL, Op, DAG);
    case Intrinsic::linx_blkv_fabs:
      return lowerFPArith(LinxV5ISD::FABS, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_fsqrt:
      return lowerFPArith(LinxV5ISD::FSQRT, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_fexp:
      return lowerFPArith(LinxV5ISD::FEXP, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_max:
      return lowerTwoSrcFloat(LinxV5ISD::MAX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_min:
      return lowerTwoSrcFloat(LinxV5ISD::MIN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdmax:
      return lowerReduce(LinxV5ISD::RDMAX, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdmin:
      return lowerReduce(LinxV5ISD::RDMIN, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdadd:
      return lowerReduce(LinxV5ISD::RDADD, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::linx_blkv_rdor:
      return lowerReduce(LinxV5ISD::RDOR, DL, Op.getOperand(0), Op, DAG);
    case Intrinsic::blkv_merge_cf:
      return lowerMergeCF(DL, Op, DAG);
    case Intrinsic::blkv_if_break: {
      SDValue Chain = Op.getOperand(0);
      SDValue ExitCond = DAG.getZExtOrTrunc(Op.getOperand(2), DL, MVT::i64);
      SDValue Broken = DAG.getZExtOrTrunc(Op.getOperand(3), DL, MVT::i64);
      SDValue OldMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    Chain);
      SDValue MergedExit =
          DAG.getNode(LinxV5ISD::MERGE_PREDICATION, DL, MVT::Other,
                      OldMask.getValue(1), ExitCond);
      SDValue NewMask = DAG.getNode(LinxV5ISD::CopyP, DL,
                                    DAG.getVTList(MVT::i64, MVT::Other),
                                    MergedExit);
      SDValue NextExit = DAG.getNode(ISD::OR, DL, MVT::i64, NewMask, Broken);
      SDValue RestoreMask =
          DAG.getNode(LinxV5ISD::Copy2P, DL, MVT::Other, NewMask.getValue(1),
                      OldMask);
      return DAG.getMergeValues({NextExit, RestoreMask}, DL);
    }
    }
    return SDValue();
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    // Intrinsic without side effect.
    ConstantSDNode *CN = cast<ConstantSDNode>(Op->getOperand(0));
    Intrinsic::ID IntID = static_cast<Intrinsic::ID>(CN->getZExtValue());
    SDLoc DL(Op);
    switch (IntID) {
    default: {
      Op->print(errs(), &DAG);
      report_fatal_error("unimplemented Intrinsic operand");
    }
    case Intrinsic::blkv_get_tile_ptr: {
      Register Reg =
          cast<RegisterSDNode>(Op.getOperand(1).getOperand(1))->getReg();
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, Op.getValueType());
    }
    case Intrinsic::blkv_get_index_x:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC0,
                                MVT::i16);
    case Intrinsic::blkv_get_index_y:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC1,
                                MVT::i16);
    case Intrinsic::blkv_get_index_z:
      return DAG.getCopyFromReg(DAG.getEntryNode(), DL, LinxV5::SIMT_LC2,
                                MVT::i16);
    case Intrinsic::linx_get_thread_id:
    case Intrinsic::linx_get_thread_idx:
      return lowerGetThreadIdx(Op, DAG);
    }
  }
  case ISD::INTRINSIC_VOID: {
    return LowerINTRINSIC_VOID(Op, DAG);
  }
  case ISD::SETCC: {
    if (!Op.getNode()->isDivergent()) {
      SDLoc DL(Op);
      SDValue In1 = DAG.getSExtOrTrunc(Op.getOperand(0), DL, MVT::i64);
      SDValue In2 = DAG.getSExtOrTrunc(Op.getOperand(1), DL, MVT::i64);
      return DAG.getNode(ISD::SETCC, DL, Op.getValueType(), In1, In2, Op.getOperand(2));
    }
    return SDValue();
  }
  default: {
    Op->print(errs(), &DAG);
    report_fatal_error("unimplemented operand");
  }
  }
}

static SDValue getTargetNode(GlobalAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  if (!DAG.getSubtarget<LinxV5Subtarget>().enableLegacyISel())
    return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, N->getOffset(),
                                      Flags);
  return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                   Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, SDLoc DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

static bool canCallSIMT(const SelectionDAG &DAG) { return !isSIMT(DAG); }

// operands = [chain, intrinsic_id, src]
SDValue LinxV5TargetLowering::lowerReduce(unsigned Opcode, SDLoc &DL,
                                          SDValue Chain, SDValue Op,
                                          SelectionDAG &DAG) const {
  SDValue Src = Op.getOperand(2);
  SDValue Result = DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                               {Chain, Src});
  return Result;
}

SDValue LinxV5TargetLowering::lowerShuffle(unsigned Opcode, SDLoc &DL,
                                           SDValue Chain, SDValue Op,
                                           SelectionDAG &DAG) const {
  if (Opcode == LinxV5ISD::SHFLXOR) {
    SDValue Shfl =
        DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                    Chain, Op.getOperand(2), Op.getOperand(3));
    return Shfl;
  }
  SDValue Shfl =
      DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                  Chain, Op.getOperand(2), Op.getOperand(3), Op.getOperand(4));
  return Shfl;
}

// operands = [chain, intrinsic_id, lhs, rhs]
SDValue LinxV5TargetLowering::lowerTwoSrcFloat(unsigned Opcode, SDLoc &DL,
                                               SDValue Chain, SDValue Op,
                                               SelectionDAG &DAG) const {

  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);

  SDValue Result =
      DAG.getNode(Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other),
                  {Chain, LHS, RHS});
  return Result;
}

// operands = [chain, intrinsic_id, lhs, rhs]
SDValue LinxV5TargetLowering::lowerMergeCF(SDLoc &DL, SDValue Op,
                                               SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDValue LHS = Op.getOperand(2);
  SDValue RHS = Op.getOperand(3);

  assert(LHS.getValueType() == RHS.getValueType() && "must merge data with the same type");

  SDValue Result;
  if (LHS.getValueType() == MVT::i1) {
    SDValue LHSPromote = DAG.getSExtOrTrunc(LHS, DL, MVT::i8);
    SDValue RHSPromote = DAG.getSExtOrTrunc(RHS, DL, MVT::i8);
    SDValue Merge = DAG.getNode(LinxV5ISD::MERGE_CF, DL,
                         DAG.getVTList(MVT::i8, MVT::Other),
                         Chain, LHSPromote, RHSPromote);
    Result = DAG.getSExtOrTrunc(Merge, DL, MVT::i1);
  } else {
    Result = DAG.getNode(LinxV5ISD::MERGE_CF, DL,
                          DAG.getVTList(Op.getValueType(), MVT::Other),
                          Chain, LHS, RHS);
  }

  return Result;
}

static unsigned calculateVCallSizeMask(EVT Type, unsigned MaxCode = 12) {
  // The tile register type's size is the per-PE tile size and encodes directly
  // (no division by 4): SizeCode = Log2(SizeBytes)-6 (128 B -> 1). Local B.IOT
  // Local B.IOT and Shared B.IOS destinations both allow SizeCode 1..12
  // (128 B..256 KB per the active PTO-ISA contract).
  uint64_t SizeBytes = Type.getFixedSizeInBits() / 8;
  if (!isPowerOf2_64(SizeBytes) || SizeBytes < 128)
    report_fatal_error(
        "LinxV5 Tile size must be a power of two from 128 B upward");
  unsigned Code = Log2_64(SizeBytes) - 6; // 128 -> 1
  if (Code > MaxCode)
    report_fatal_error("LinxV5 Tile size exceeds the per-PE capacity for its "
                       "destination role");
  return Code;
}

static uint64_t getV5ConstantOperand(SDValue Op, StringRef Name,
                                     uint64_t MaxValue, bool AllowZero = true) {
  const auto *Constant = dyn_cast<ConstantSDNode>(Op);
  if (!Constant)
    report_fatal_error(Twine("LinxV5 ") + Name +
                       " operand must be a compile-time constant");
  uint64_t Value = Constant->getZExtValue();
  if (Value > MaxValue || (!AllowZero && Value == 0))
    report_fatal_error(Twine("invalid LinxV5 ") + Name + " operand");
  return Value;
}

static SDValue lowerV5GMOV(SDValue Op, SelectionDAG &DAG) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(2), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(3), "PE mask", 15, false);
  if (const auto *PeerTID = dyn_cast<ConstantSDNode>(Op.getOperand(4)))
    if (PeerTID->getZExtValue() > 3)
      report_fatal_error("invalid LinxV5 GMOV peer thread ID");
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64),
      Op.getOperand(4), Op.getOperand(5)};
  return DAG.getNode(LinxV5ISD::V5_GMOV, DL, Op->getVTList(), Ops);
}

static SDValue lowerV5SharedS2L(SDValue Op, SelectionDAG &DAG,
                                unsigned Function) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(3), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(4), "PE mask", 15, false);
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(Function, DL, MVT::i64),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      Op.getOperand(2),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64)};
  return DAG.getNode(LinxV5ISD::V5_SHARED_S2L, DL, Op->getVTList(), Ops);
}

static SDValue lowerV5SharedL2S(SDValue Op, SelectionDAG &DAG,
                                unsigned Function) {
  SDLoc DL(Op);
  uint64_t DataType = getV5ConstantOperand(Op.getOperand(2), "data type", 31);
  uint64_t PEMask =
      getV5ConstantOperand(Op.getOperand(3), "PE mask", 15, false);
  SDValue SrcTile = Op.getOperand(4);
  SDValue Ops[] = {
      Op.getOperand(0),
      DAG.getTargetConstant(Function, DL, MVT::i64),
      DAG.getTargetConstant(DataType, DL, MVT::i64),
      DAG.getTargetConstant(PEMask, DL, MVT::i64),
      DAG.getTargetConstant(calculateVCallSizeMask(SrcTile.getValueType()), DL,
                            MVT::i64),
      SrcTile};
  return DAG.getNode(LinxV5ISD::V5_SHARED_L2S, DL,
                     DAG.getVTList(MVT::i64, MVT::Other), Ops);
}

/// Intrinsic Ops: (0)Chain; (1)IntNo; (2)FuncPtr; (3,4,5): Dimensions; (6):
/// Tile, ...
SDValue LinxV5TargetLowering::lowerTileOpWithBody(SDLoc &DL, SDValue Op,
                                                  unsigned VDefNum,
                                                  unsigned VUseNum,
                                                  SelectionDAG &DAG,
                                                  unsigned Opcode) const {
  MachineFunction &MF = DAG.getMachineFunction();
  Module *M = MF.getFunction().getParent();

  SmallVector<SDValue> Ops;

  // Chain and Glue
  SDValue Chain = Op.getOperand(0);
  unsigned GetNum = Op.getNumOperands() - 6 - VUseNum;
  SmallVector<SDValue> GPRUses;  // SDValue
  for (unsigned i = 0; i < GetNum; ++i) {
    SDValue Src = Op.getOperand(i + 6 + VUseNum);
    if (Src.getValueType().isInteger() && Src.getValueType() != MVT::i64) {
      Src = DAG.getNode(ISD::SIGN_EXTEND, DL, MVT::i64, Src);
    }
    GPRUses.push_back(Src);
  }
  Ops.push_back(Chain);

  SDValue FuncPtr = Op.getOperand(2);
  const GlobalValue *GV = dyn_cast<GlobalAddressSDNode>(FuncPtr)->getGlobal();
  FuncPtr = DAG.getTargetGlobalAddress(GV, DL, MVT::i64);
  Ops.push_back(FuncPtr);

  Ops.push_back(Op.getOperand(3)); // Dim-X
  Ops.push_back(Op.getOperand(4)); // Dim-Y
  Ops.push_back(Op.getOperand(5)); // Dim-Z

  SDNode *N = Op.getNode();
  for (int i = 0; i < VDefNum; i++) {
    unsigned TileSize = calculateVCallSizeMask(SDValue(N, i).getValueType());
    SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
    Ops.push_back(SizeValue); // Tile Size
  }

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 6);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  if (const Function *F = dyn_cast<Function>(GV)) {
    SmallString<32> StackSizeName(F->getName());
    StackSizeName += "_stack_size";
    GlobalVariable *SSGV = M->getGlobalVariable(StackSizeName.str());
    if (!SSGV) {
      Type *Ty = Type::getInt32Ty(M->getContext());
      SSGV = new GlobalVariable(*M, Ty, false, GlobalValue::ExternalLinkage, nullptr, StackSizeName.str());
    }
    // TODO: support specifying stack size with register
    SDValue StackSizeSym = DAG.getTargetGlobalAddress(SSGV, DL, MVT::i64);
    Ops.push_back(StackSizeSym);
  }

  for (SDValue Reg : GPRUses) {
    Ops.push_back(Reg);
  }

  SDValue Call;
  Call = DAG.getNode(Opcode, DL, Op->getVTList(), Ops);
  return Call;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6~): Tile Input
SDValue LinxV5TargetLowering::lowerTemplateBLK(unsigned Opcode, SDLoc &DL,
                                               SDValue Op, unsigned VUseNum,
                                               SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementTypeA = Op.getOperand(5);
  unsigned TypeEnumA = cast<ConstantSDNode>(TileElementTypeA)->getZExtValue();
  SDValue TypeEnumValueA = DAG.getTargetConstant(TypeEnumA, DL, MVT::i64);
  Ops.push_back(TypeEnumValueA);

  SDValue TileElementTypeB = Op.getOperand(6);
  unsigned TypeEnumB = cast<ConstantSDNode>(TileElementTypeB)->getZExtValue();
  SDValue TypeEnumValueB = DAG.getTargetConstant(TypeEnumB, DL, MVT::i64);
  Ops.push_back(TypeEnumValueB);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 7);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

SDValue LinxV5TargetLowering::lowerTemplateBLKMasked(SDLoc &DL, SDValue Op,
                                                     SelectionDAG &DAG) const {
  // PR #352 execution masks are currently defined only for Local CUBE_M16 and
  // CUBE_M32. Keep the contract explicit at the intrinsic boundary so an
  // arbitrary element-wise call cannot silently become a malformed bundle.
  uint64_t DimM = getV5ConstantOperand(Op.getOperand(2), "masked CUBE M",
                                       32, false);
  uint64_t DimN = getV5ConstantOperand(Op.getOperand(3), "masked CUBE N",
                                       0xffff, false);
  uint64_t DimK = getV5ConstantOperand(Op.getOperand(4), "masked CUBE K",
                                       0xffff, false);
  if (DimM != 16 && DimM != 32)
    report_fatal_error("masked CUBE execution mask requires M=16 or M=32");
  if (DimN == 0 || DimK == 0)
    report_fatal_error("masked CUBE execution mask requires positive N and K");

  getV5ConstantOperand(Op.getOperand(11), "masked CUBE PredInv", 1);
  getV5ConstantOperand(Op.getOperand(12), "masked CUBE Zero", 1);

  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  Ops.push_back(Op.getOperand(2));
  Ops.push_back(Op.getOperand(3));
  Ops.push_back(Op.getOperand(4));

  for (unsigned Index : {5u, 6u}) {
    unsigned Type = getV5ConstantOperand(Op.getOperand(Index),
                                          "masked CUBE data type", 31);
    Ops.push_back(DAG.getTargetConstant(Type, DL, MVT::i64));
  }

  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(Op.getOperand(9));
  Ops.push_back(Op.getOperand(10));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(11), "masked CUBE PredInv", 1),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(12), "masked CUBE Zero", 1),
      DL, MVT::i64));
  Ops.push_back(Op.getOperand(0));
  return DAG.getNode(LinxV5ISD::BLK_MATMUL_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTAddMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  getV5ConstantOperand(Op.getOperand(2), "masked TADD rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "masked TADD cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "masked TADD data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "masked TADD layout", 31);
  getV5ConstantOperand(Op.getOperand(9), "masked TADD PredInv", 1);
  getV5ConstantOperand(Op.getOperand(10), "masked TADD Zero", 1);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {2u, 3u}) {
    if (isa<ConstantSDNode>(Op.getOperand(Index))) {
      Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
      Ops.push_back(DAG.getTargetConstant(
          cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
          MVT::i64));
    } else {
      Ops.push_back(Op.getOperand(Index));
      Ops.push_back(DAG.getTargetConstant(0, DL, MVT::i64));
    }
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "masked TADD data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "masked TADD layout", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(9), "masked TADD PredInv", 1),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(10), "masked TADD Zero", 1),
      DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TADD_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTSubMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  getV5ConstantOperand(Op.getOperand(2), "masked TSUB rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "masked TSUB cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "masked TSUB data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "masked TSUB layout", 31);
  getV5ConstantOperand(Op.getOperand(9), "masked TSUB PredInv", 1);
  getV5ConstantOperand(Op.getOperand(10), "masked TSUB Zero", 1);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  // TLSU contract uses LB0=columns and LB1=rows.
  for (unsigned Index : {3u, 2u}) {
    if (isa<ConstantSDNode>(Op.getOperand(Index))) {
      Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
      Ops.push_back(DAG.getTargetConstant(
          cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
          MVT::i64));
    } else {
      Ops.push_back(Op.getOperand(Index));
      Ops.push_back(DAG.getTargetConstant(0, DL, MVT::i64));
    }
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "masked TSUB data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "masked TSUB layout", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(9), "masked TSUB PredInv", 1),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(10), "masked TSUB Zero", 1),
      DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TSUB_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTLEA(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows =
      getV5ConstantOperand(Op.getOperand(2), "TLEA rows", 32, false);
  const uint64_t Cols =
      getV5ConstantOperand(Op.getOperand(3), "TLEA columns", 32, false);
  const uint64_t DataType = getV5ConstantOperand(
      Op.getOperand(4), "TLEA source data type", 31);
  if (DataType != LinxV5Op::DataType::S32 &&
      DataType != LinxV5Op::DataType::U32 &&
      DataType != LinxV5Op::DataType::S64 &&
      DataType != LinxV5Op::DataType::U64)
    report_fatal_error("TLEA source data type must be S32, U32, S64 or U64");
  const uint64_t Layout =
      getV5ConstantOperand(Op.getOperand(5), "TLEA layout", 31);
  if (Layout != 0 && Layout != 29)
    report_fatal_error("TLEA layout must be RowMajor or CUBE_M32");
  const uint64_t ElementBits =
      getV5ConstantOperand(Op.getOperand(7), "TLEA element width", 64);
  if (ElementBits != 8 && ElementBits != 16 && ElementBits != 32 &&
      ElementBits != 64)
    report_fatal_error("TLEA element width must be 8, 16, 32 or 64 bits");
  EVT SourceVT = Op.getOperand(6).getValueType();
  EVT ResultVT = Op.getValueType();
  if (!SourceVT.isFixedLengthVector() || !ResultVT.isFixedLengthVector() ||
      !SourceVT.getScalarType().isInteger() ||
      !ResultVT.getScalarType().isInteger() ||
      SourceVT.getVectorNumElements() != ResultVT.getVectorNumElements() ||
      Rows > UINT64_MAX / Cols ||
      Rows * Cols != SourceVT.getVectorNumElements() ||
      (Layout == 29 && Rows != 32) ||
      (SourceVT.getScalarSizeInBits() != 32 &&
       SourceVT.getScalarSizeInBits() != 64) ||
      ResultVT.getScalarSizeInBits() != 64)
    report_fatal_error(
        "TLEA requires equal-lane i32/i64 source and i64 destination Tiles");
  if (((DataType == LinxV5Op::DataType::S32 ||
        DataType == LinxV5Op::DataType::U32) &&
       SourceVT.getScalarSizeInBits() != 32) ||
      ((DataType == LinxV5Op::DataType::S64 ||
        DataType == LinxV5Op::DataType::U64) &&
       SourceVT.getScalarSizeInBits() != 64))
    report_fatal_error("TLEA source data type does not match index Tile width");
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {2u, 3u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(DAG.getConstant(ElementBits, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TLEA, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTBinary(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows =
      getV5ConstantOperand(Op.getOperand(2), "element binary rows", 32, false);
  const uint64_t Cols =
      getV5ConstantOperand(Op.getOperand(3), "element binary columns", 32,
                           false);
  const uint64_t DataType = getV5ConstantOperand(
      Op.getOperand(4), "element binary data type", 31);
  const uint64_t Layout =
      getV5ConstantOperand(Op.getOperand(5), "element binary layout", 31);
  const uint64_t Opcode =
      getV5ConstantOperand(Op.getOperand(6), "element binary opcode", 9);

  EVT ResultVT = Op.getValueType();
  EVT LHSVT = Op.getOperand(7).getValueType();
  EVT RHSVT = Op.getOperand(8).getValueType();
  const bool IntegerProfile = DataType == LinxV5Op::DataType::U32 ||
                              DataType == LinxV5Op::DataType::S32;
  const bool F32Norm =
      DataType == LinxV5Op::DataType::FP32 && Layout == 0 &&
      ((Rows == 16 && Cols == 32 && ResultVT == MVT::v512f32) ||
       (Rows == 1 && Cols == 32 && ResultVT == MVT::v32f32));
  const bool WideInteger = DataType == LinxV5Op::DataType::U64 ||
                           DataType == LinxV5Op::DataType::S64;
  const bool IntegerCube =
      Rows == 32 &&
      (Cols == 1 || Cols == 2 || Cols == 4 || Cols == 8 || Cols == 16) &&
      (IntegerProfile || WideInteger) && Layout == 29 &&
      ResultVT.isFixedLengthVector() &&
      ResultVT.getVectorNumElements() == Rows * Cols &&
      ResultVT.getScalarType() == (WideInteger ? MVT::i64 : MVT::i32);
  const bool PredicateCube =
      Rows == 32 &&
      (Cols == 1 || Cols == 2 || Cols == 4 || Cols == 8 || Cols == 16) &&
      Layout == 29 && DataType == LinxV5Op::DataType::U8 &&
      ResultVT.isFixedLengthVector() && ResultVT.getScalarType() == MVT::i8 &&
      ResultVT.getVectorNumElements() == std::max<uint64_t>(128, Rows * Cols) &&
      Opcode >= 5 && Opcode <= 7;
  if ((!IntegerCube && !PredicateCube && !F32Norm) || LHSVT != ResultVT ||
      RHSVT != ResultVT)
    report_fatal_error(
        "element binary requires an exact U32/S32 CUBE_M32 or FP32 NORM "
        "profile");

  static constexpr uint64_t PTOSelectors[] = {
      LinxV5Op::TileOPTEPL::TADD, LinxV5Op::TileOPTEPL::TSUB,
      LinxV5Op::TileOPTEPL::TMUL, LinxV5Op::TileOPTEPL::TDIV,
      LinxV5Op::TileOPTEPL::TREM, LinxV5Op::TileOPTEPL::TAND,
      LinxV5Op::TileOPTEPL::TOR,  LinxV5Op::TileOPTEPL::TXOR,
      LinxV5Op::TileOPTEPL::TSHL, LinxV5Op::TileOPTEPL::TSHR};
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {2u, 3u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(DAG.getTargetConstant(PTOSelectors[Opcode], DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TBINARY, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTBinaryGPRMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows = getV5ConstantOperand(
      Op.getOperand(2), "GPR-masked element binary rows", 32, false);
  const uint64_t Cols = getV5ConstantOperand(
      Op.getOperand(3), "GPR-masked element binary columns", 32, false);
  const uint64_t DataType = getV5ConstantOperand(
      Op.getOperand(4), "GPR-masked element binary data type", 31);
  const uint64_t Layout = getV5ConstantOperand(
      Op.getOperand(5), "GPR-masked element binary layout", 31);
  const uint64_t Opcode = getV5ConstantOperand(
      Op.getOperand(6), "GPR-masked element binary opcode", 9);
  const uint64_t PredInv = getV5ConstantOperand(
      Op.getOperand(11), "GPR-masked element binary PredInv", 1);
  const uint64_t Zero = getV5ConstantOperand(
      Op.getOperand(12), "GPR-masked element binary Zero", 1);
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(10));
  EVT ResultVT = Op.getValueType();
  const bool IntegerProfile = DataType == LinxV5Op::DataType::U32 ||
                              DataType == LinxV5Op::DataType::S32;
  const bool WideIntegerProfile = DataType == LinxV5Op::DataType::U64 ||
                                  DataType == LinxV5Op::DataType::S64;
  const bool F32Profile = DataType == LinxV5Op::DataType::FP32;
  const bool F16Profile = DataType == LinxV5Op::DataType::FP16;
  const bool DenseInteger =
      IntegerProfile && Rows == 32 && Cols == 2 && ResultVT == MVT::v64i32;
  const bool NativeWideInteger = WideIntegerProfile && Rows == 32 &&
                                 Cols == 1 && ResultVT == MVT::v32i64;
  const bool DenseWideAddress = WideIntegerProfile && Rows == 32 && Cols == 2 &&
                                ResultVT == MVT::v64i64;
  const bool DenseF16 = F16Profile && Rows == 32 && Cols == 2 &&
                        ResultVT == MVT::v64f16 && Opcode <= 1;
  if (Rows != 32 ||
      (Cols != 1 && !DenseF16 && !DenseInteger && !DenseWideAddress) ||
      (!IntegerProfile && !WideIntegerProfile && !F32Profile && !F16Profile) ||
      Layout != 29 ||
      (F32Profile   ? ResultVT != MVT::v32f32 || Opcode > 1
       : F16Profile ? !DenseF16
       : WideIntegerProfile ? !NativeWideInteger && !DenseWideAddress
                            : (ResultVT != MVT::v32i32 && !DenseInteger)) ||
      Op.getOperand(7).getValueType() != ResultVT ||
      Op.getOperand(8).getValueType() != ResultVT ||
      Op.getOperand(9).getValueType() != MVT::i64 ||
      Op.getOperand(10).getValueType() != MVT::i64 || !MaskHigh ||
      MaskHigh->getZExtValue() != 0 || PredInv != 0 || Zero != 1)
    report_fatal_error(
        "GPR-masked element binary requires an exact admitted logical32 or "
        "dense64 CUBE_M32 Tile, one low mask word and zero inactive lanes");
  static constexpr uint64_t PTOSelectors[] = {
      LinxV5Op::TileOPTEPL::TADD, LinxV5Op::TileOPTEPL::TSUB,
      LinxV5Op::TileOPTEPL::TMUL, LinxV5Op::TileOPTEPL::TDIV,
      LinxV5Op::TileOPTEPL::TREM, LinxV5Op::TileOPTEPL::TAND,
      LinxV5Op::TileOPTEPL::TOR,  LinxV5Op::TileOPTEPL::TXOR,
      LinxV5Op::TileOPTEPL::TSHL, LinxV5Op::TileOPTEPL::TSHR};
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(DAG.getTargetConstant(PTOSelectors[Opcode], DL, MVT::i64));
  Ops.push_back(Op.getOperand(9));
  Ops.push_back(Op.getOperand(10));
  Ops.push_back(DAG.getTargetConstant(PredInv, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Zero, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TBINARY_GPR_MASKED, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTNegGPRMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "masked TNEG rows", 32, false);
  const uint64_t Cols = getV5ConstantOperand(Op.getOperand(3), "masked TNEG columns", 32, false);
  const uint64_t DType = getV5ConstantOperand(Op.getOperand(4), "masked TNEG data type", 31);
  const uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "masked TNEG layout", 31);
  const bool FP32Profile = DType == LinxV5Op::DataType::FP32 && Cols == 1 &&
                           Op.getValueType() == MVT::v32f32;
  const bool FP16Profile = DType == LinxV5Op::DataType::FP16 && Cols == 2 &&
                           Op.getValueType() == MVT::v64f16;
  if (Rows != 32 || (!FP32Profile && !FP16Profile) || Layout != 29 ||
      Op.getOperand(6).getValueType() != Op.getValueType() ||
      Op.getOperand(7).getValueType() != MVT::i64)
    report_fatal_error("GPR-masked TNEG requires exact 32x1 FP32 CUBE_M32 "
                       "Tiles and one low mask word");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  return DAG.getNode(LinxV5ISD::EW_TNEG_GPR_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTCVTGPRMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows =
      getV5ConstantOperand(Op.getOperand(2), "masked TCVT rows", 32, false);
  const uint64_t Cols =
      getV5ConstantOperand(Op.getOperand(3), "masked TCVT columns", 32, false);
  const uint64_t SrcDType = getV5ConstantOperand(
      Op.getOperand(4), "masked TCVT source data type", 31);
  const uint64_t DstDType = getV5ConstantOperand(
      Op.getOperand(5), "masked TCVT destination data type", 31);
  const uint64_t Layout =
      getV5ConstantOperand(Op.getOperand(6), "masked TCVT layout", 31);
  const uint64_t RMode =
      getV5ConstantOperand(Op.getOperand(7), "masked TCVT rounding mode", 7);
  const uint64_t Sat =
      getV5ConstantOperand(Op.getOperand(8), "masked TCVT saturation", 1);
  const uint64_t PredInv = getV5ConstantOperand(
      Op.getOperand(12), "masked TCVT predicate inversion", 1);
  const uint64_t Zero =
      getV5ConstantOperand(Op.getOperand(13), "masked TCVT zeroing", 1);
  EVT ResultVT = Op.getValueType();
  EVT SourceVT = Op.getOperand(9).getValueType();
  const bool SourceType = SrcDType == LinxV5Op::DataType::S64 ||
                          SrcDType == LinxV5Op::DataType::U64;
  const bool DestinationType = DstDType == LinxV5Op::DataType::S32 ||
                               DstDType == LinxV5Op::DataType::U32;
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(11));
  if (Rows != 32 || Cols != 1 || !SourceType || !DestinationType ||
      Layout != 29 || RMode != 0 || Sat != 0 || ResultVT != MVT::v32i32 ||
      SourceVT != MVT::v32i64 ||
      Op.getOperand(10).getValueType() != MVT::i64 ||
      Op.getOperand(11).getValueType() != MVT::i64 || !MaskHigh ||
      MaskHigh->getZExtValue() != 0 || PredInv != 0 || Zero != 1)
    report_fatal_error(
        "GPR-masked TCVT requires exact logical32 S64/U64 to S32/U32 "
        "CUBE_M32 narrowing, RNONE, Sat=0, one low mask word and zero "
        "inactive lanes");

  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(SrcDType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(DstDType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(9));
  Ops.push_back(DAG.getTargetConstant(RMode, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Sat, DL, MVT::i64));
  Ops.push_back(Op.getOperand(10));
  Ops.push_back(Op.getOperand(11));
  Ops.push_back(DAG.getTargetConstant(PredInv, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Zero, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_TCVT_GPR_MASKED, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTCI(SDLoc &DL, SDValue Op,
                                                  SelectionDAG &DAG,
                                                  bool ExplicitRow) const {
  unsigned PhysicalCols = 0;
  if (ExplicitRow) {
    PhysicalCols = getV5ConstantOperand(Op.getOperand(8),
                                        "NORM physical columns", 2048, false);
    uint64_t ValidCols = getV5ConstantOperand(
        Op.getOperand(3), "NORM valid columns", 2048, false);
    if (getV5ConstantOperand(Op.getOperand(2), "NORM rows", 1, false) != 1 ||
        getV5ConstantOperand(Op.getOperand(4), "NORM dtype", 31) != 25 ||
        getV5ConstantOperand(Op.getOperand(5), "NORM layout", 31) != 0 ||
        !((PhysicalCols == 128 && (ValidCols == 32 || ValidCols == 128)) ||
          (PhysicalCols == 2048 && ValidCols == 2048)))
      report_fatal_error("unsupported NORM U32 row descriptor");
  }

  uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "TCI rows", 32, false);
  uint64_t Cols =
      getV5ConstantOperand(Op.getOperand(3), "TCI columns", 2048, false);
  uint64_t DType = getV5ConstantOperand(Op.getOperand(4), "TCI data type", 31);
  uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "TCI layout", 31);
  EVT ResultVT = Op.getValueType();
  if (!ResultVT.isFixedLengthVector() ||
      !ResultVT.getScalarType().isInteger() || Rows > UINT64_MAX / Cols ||
      (ExplicitRow ? PhysicalCols : Rows * Cols) !=
          ResultVT.getVectorNumElements() ||
      (Layout != 0 && Layout != 29) || (Layout == 29 && Rows != 32) ||
      ((DType == LinxV5Op::DataType::S32 || DType == LinxV5Op::DataType::U32)
           ? ResultVT.getScalarSizeInBits() != 32
       : (DType == LinxV5Op::DataType::S64 || DType == LinxV5Op::DataType::U64)
           ? ResultVT.getScalarSizeInBits() != 64
           : true))
    report_fatal_error(
        "TCI requires an exact-shape integer S32/U32/S64/U64 Tile");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()),
                                      DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  if (ExplicitRow)
    Ops.push_back(DAG.getTargetConstant(PhysicalCols, DL, MVT::i64));
  return DAG.getNode(ExplicitRow ? LinxV5ISD::EW_TCI_ROW : LinxV5ISD::EW_TCI,
                     DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTCMPSGPR(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG, bool Masked) const {
  uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "TCMPS rows", 32, false);
  uint64_t Cols = getV5ConstantOperand(Op.getOperand(3), "TCMPS columns", 32, false);
  uint64_t DType = getV5ConstantOperand(Op.getOperand(4), "TCMPS data type", 31);
  uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "TCMPS layout", 31);
  uint64_t Cmp = getV5ConstantOperand(Op.getOperand(8), "TCMPS comparison", 5);
  EVT SourceVT = Op.getOperand(6).getValueType();
  const bool F32Profile = Masked && DType == LinxV5Op::DataType::FP32;
  const bool F16Profile = Masked && DType == LinxV5Op::DataType::FP16;
  if (!SourceVT.isFixedLengthVector() ||
      (!SourceVT.getScalarType().isInteger() && !F32Profile && !F16Profile) ||
      Rows > UINT64_MAX / Cols ||
      Rows * Cols != SourceVT.getVectorNumElements() || Rows * Cols > 64 ||
      Layout != 29 || Rows != 32 ||
      ((DType == LinxV5Op::DataType::S32 || DType == LinxV5Op::DataType::U32)
           ? SourceVT.getScalarSizeInBits() != 32
       : (DType == LinxV5Op::DataType::S64 || DType == LinxV5Op::DataType::U64)
           ? SourceVT.getScalarSizeInBits() != 64
       : F32Profile ? SourceVT != MVT::v32f32
       : F16Profile ? SourceVT != MVT::v64f16
                    : true) ||
      Op.getOperand(7).getValueType() != MVT::i64 ||
      (Masked &&
       ((!F32Profile && !F16Profile) || (F16Profile ? Cols != 2 : Cols != 1) ||
        Op.getOperand(9).getValueType() != MVT::i64)))
    report_fatal_error(
        "TCMPS GPR requires a 32-row exact-shape integer CUBE_M32 Tile; masked FP32 supports the same shape");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(DAG.getTargetConstant(Cmp, DL, MVT::i64));
  if (Masked)
    Ops.push_back(Op.getOperand(9));
  return DAG.getNode(Masked ? LinxV5ISD::EW_TCMPS_GPR_MASKED
                            : LinxV5ISD::EW_TCMPS_GPR, DL,
                     DAG.getVTList(MVT::i64, MVT::Other), Ops);
}


SDValue LinxV5TargetLowering::lowerIndexedTLSURange(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  SDValue Base;
  switch (cast<ConstantSDNode>(Op.getOperand(1))->getZExtValue()) {
  case Intrinsic::linx_experimental_ew_mgather_masked_range:
    Base = lowerElementwiseMGatherMasked(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mgather_gpr_masked_range:
    Base = lowerElementwiseMGatherGPRMasked(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mscatter_masked_range:
    Base = lowerElementwiseMScatterMasked(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mscatter_gpr_masked_range:
    Base = lowerElementwiseMScatterGPRMasked(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mgather_add_range:
    Base = lowerElementwiseMGatherAdd(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mgather_add_masked_range:
    Base = lowerElementwiseMGatherAddMasked(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mscatter_add_range:
    Base = lowerElementwiseMScatterAdd(DL, Op, DAG);
    break;
  case Intrinsic::linx_experimental_ew_mscatter_add_predicate_range:
    Base = lowerElementwiseMScatterAddPredicate(DL, Op, DAG);
    break;
  default: llvm_unreachable("unexpected indexed range intrinsic");
  }
  unsigned Count = Op.getNumOperands();
  SDValue Minimum = Op.getOperand(Count - 2);
  SDValue Maximum = Op.getOperand(Count - 1);
  if (Minimum.getValueType() != MVT::i64 || Maximum.getValueType() != MVT::i64)
    report_fatal_error("indexed TLSU range bounds must be i64 byte displacements");
  // An explicit singleton zero range must not become the absent selector pair.
  Register Bound = DAG.getMachineFunction().getRegInfo().createVirtualRegister(
      &LinxV5::GRNoR0RegClass);
  SDValue Chain = DAG.getCopyToReg(Base.getOperand(0), DL, Bound, Minimum);
  Minimum = DAG.getCopyFromReg(Chain, DL, Bound, MVT::i64);
  SmallVector<SDValue> Operands(Base->op_begin(), Base->op_end());
  Operands[0] = Minimum.getValue(1);
  Operands.push_back(Minimum);
  Operands.push_back(Maximum);
  Operands.push_back(DAG.getTargetConstant(Base.getOpcode(), DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_INDEXED_RANGE, DL, Base->getVTList(), Operands);
}

SDValue LinxV5TargetLowering::lowerElementwiseMGatherMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG, bool ExplicitLayout) const {
  getV5ConstantOperand(Op.getOperand(2), "masked MGATHER rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "masked MGATHER cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "masked MGATHER data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "masked MGATHER pad", 3);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  // TLSU contract uses LB0=columns and LB1=rows.
  for (unsigned Index : {3u, 2u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "masked MGATHER data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "masked MGATHER pad", 3), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  const unsigned Shift = ExplicitLayout ? 1 : 0;
  const uint64_t Layout =
      ExplicitLayout
          ? getV5ConstantOperand(Op.getOperand(6), "masked MGATHER layout", 31)
          : 0;
  if (Layout != 0 && Layout != 29)
    report_fatal_error("masked MGATHER requires NORM or CUBE_M32 layout");
  if (ExplicitLayout && Layout == 29) {
    const uint64_t Rows =
        cast<ConstantSDNode>(Op.getOperand(2))->getZExtValue();
    const uint64_t Cols =
        cast<ConstantSDNode>(Op.getOperand(3))->getZExtValue();
    const uint64_t DType =
        cast<ConstantSDNode>(Op.getOperand(4))->getZExtValue();
    EVT ResultVT = Op.getValueType();
    EVT IndexVT = Op.getOperand(8).getValueType();
    const bool Wide =
        DType == LinxV5Op::DataType::U64 || DType == LinxV5Op::DataType::S64;
    const bool Narrow =
        DType == LinxV5Op::DataType::U32 || DType == LinxV5Op::DataType::S32;
    if (Rows != 32 ||
        (Cols != 1 && Cols != 2 && Cols != 4 && Cols != 8 && Cols != 16) ||
        (!Wide && !Narrow) || !ResultVT.isFixedLengthVector() ||
        ResultVT.getVectorNumElements() != Rows * Cols ||
        ResultVT.getScalarType() != (Wide ? MVT::i64 : MVT::i32) ||
        !IndexVT.isFixedLengthVector() || IndexVT.getScalarType() != MVT::i64 ||
        IndexVT.getVectorNumElements() != Rows * Cols ||
        !Op.getOperand(9).getValueType().isFixedLengthVector() ||
        Op.getOperand(9).getValueType().getScalarType() != MVT::i8 ||
        Op.getOperand(9).getValueType().getVectorNumElements() !=
            std::max<uint64_t>(128, Rows * Cols))
      report_fatal_error("M32 tile-masked MGATHER requires matching admitted "
                         "integer lanes, paired i64 offsets and PredicateCell");
  }
  Ops.push_back(Op.getOperand(6 + Shift)); // base pointer
  Ops.push_back(Op.getOperand(7 + Shift)); // byte offsets
  Ops.push_back(Op.getOperand(8 + Shift)); // PredicateCell
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_MGATHER_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMGatherGPRMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  uint64_t Rows = getV5ConstantOperand(
      Op.getOperand(2), "GPR-masked MGATHER rows", 32, false);
  uint64_t Cols = getV5ConstantOperand(
      Op.getOperand(3), "GPR-masked MGATHER cols", 32, false);
  uint64_t DType = getV5ConstantOperand(
      Op.getOperand(4), "GPR-masked MGATHER data type", 31);
  uint64_t Pad = getV5ConstantOperand(
      Op.getOperand(5), "GPR-masked MGATHER pad", 3);
  uint64_t Layout = getV5ConstantOperand(
      Op.getOperand(6), "GPR-masked MGATHER layout", 31);
  uint64_t IndexDType = getV5ConstantOperand(
      Op.getOperand(7), "GPR-masked MGATHER index data type", 31);
  uint64_t PredInv = getV5ConstantOperand(
      Op.getOperand(12), "GPR-masked MGATHER PredInv", 1);
  uint64_t Zero = getV5ConstantOperand(
      Op.getOperand(13), "GPR-masked MGATHER Zero", 1);
  EVT ResultVT = Op.getValueType();
  EVT OffsetVT = Op.getOperand(9).getValueType();
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(11));
  const bool ValidValue =
      (DType == LinxV5Op::DataType::U32 &&
       (ResultVT == MVT::v32i32 || ResultVT == MVT::v64i32)) ||
      ((DType == LinxV5Op::DataType::S64 ||
        DType == LinxV5Op::DataType::U64) && ResultVT == MVT::v32i64) ||
      (DType == LinxV5Op::DataType::U16 && ResultVT == MVT::v64i16) ||
      (DType == LinxV5Op::DataType::FP32 && ResultVT == MVT::v32f32) ||
      (DType == LinxV5Op::DataType::FP16 && ResultVT == MVT::v64f16);
  if (!ValidValue ||
      (IndexDType != LinxV5Op::DataType::S64 &&
       IndexDType != LinxV5Op::DataType::U64) ||
      Layout != 29 || Rows != 32 ||
      ((DType == LinxV5Op::DataType::FP16 ||
        DType == LinxV5Op::DataType::U16 || ResultVT == MVT::v64i32)
           ? Cols != 2
           : Cols != 1) ||
      Pad != LinxV5Op::PadValue::Zero || PredInv != 0 || Zero != 1 ||
      MaskHigh == nullptr || MaskHigh->getZExtValue() != 0 ||
      OffsetVT != (Cols == 2 ? MVT::v64i64 : MVT::v32i64) ||
      Op.getOperand(10).getValueType() != MVT::i64 ||
      Op.getOperand(11).getValueType() != MVT::i64)
    report_fatal_error(
        "GPR-masked MGATHER requires an exact admitted S64/U64/U16/U32/FP16/FP32 "
        "element profile, S64/U64 byte "
        "offsets, CUBE_M32, one low GPR mask word and zero inactive lanes");

  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Cols, Rows}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Pad, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(8));  // base pointer
  Ops.push_back(Op.getOperand(9));  // U64 byte offsets
  Ops.push_back(Op.getOperand(10)); // execution-mask low word
  Ops.push_back(Op.getOperand(11)); // execution-mask high word
  Ops.push_back(DAG.getTargetConstant(PredInv, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Zero, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_MGATHER_GPR_MASKED, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMScatterMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  getV5ConstantOperand(Op.getOperand(2), "masked MSCATTER rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "masked MSCATTER cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "masked MSCATTER data type", 31);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  // TLSU contract uses LB0=columns and LB1=rows.
  for (unsigned Index : {3u, 2u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "masked MSCATTER data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getOperand(6).getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(5)); // base pointer
  Ops.push_back(Op.getOperand(6)); // source tile
  Ops.push_back(Op.getOperand(7)); // logical element offsets
  Ops.push_back(Op.getOperand(8)); // predicate tile
  return DAG.getNode(LinxV5ISD::EW_MSCATTER_MASKED, DL,
                     DAG.getVTList(MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMScatterGPRMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows = getV5ConstantOperand(
      Op.getOperand(2), "GPR-masked MSCATTER rows", 32, false);
  const uint64_t Cols = getV5ConstantOperand(
      Op.getOperand(3), "GPR-masked MSCATTER columns", 32, false);
  const uint64_t DataType = getV5ConstantOperand(
      Op.getOperand(4), "GPR-masked MSCATTER data type", 31);
  const uint64_t Layout = getV5ConstantOperand(
      Op.getOperand(5), "GPR-masked MSCATTER layout", 31);
  const uint64_t IndexDataType = getV5ConstantOperand(
      Op.getOperand(6), "GPR-masked MSCATTER index data type", 31);
  const uint64_t PredInv = getV5ConstantOperand(
      Op.getOperand(12), "GPR-masked MSCATTER PredInv", 1);
  const uint64_t Zero = getV5ConstantOperand(
      Op.getOperand(13), "GPR-masked MSCATTER Zero", 1);
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(11));
  const EVT ValueVT = Op.getOperand(9).getValueType();
  const bool ValidValue =
      ((DataType == LinxV5Op::DataType::S32 ||
        DataType == LinxV5Op::DataType::U32) &&
       (ValueVT == MVT::v32i32 || ValueVT == MVT::v64i32)) ||
      ((DataType == LinxV5Op::DataType::S64 ||
        DataType == LinxV5Op::DataType::U64) && ValueVT == MVT::v32i64) ||
      (DataType == LinxV5Op::DataType::U16 && ValueVT == MVT::v64i16) ||
      (DataType == LinxV5Op::DataType::FP32 && ValueVT == MVT::v32f32) ||
      (DataType == LinxV5Op::DataType::FP16 && ValueVT == MVT::v64f16);
  if (Rows != 32 ||
      ((DataType == LinxV5Op::DataType::FP16 ||
        DataType == LinxV5Op::DataType::U16 || ValueVT == MVT::v64i32)
           ? Cols != 2
           : Cols != 1) ||
      !ValidValue || Layout != 29 ||
      (IndexDataType != LinxV5Op::DataType::S64 &&
       IndexDataType != LinxV5Op::DataType::U64) ||
      Op.getOperand(8).getValueType() !=
          (Cols == 2 ? MVT::v64i64 : MVT::v32i64) ||
      Op.getOperand(10).getValueType() != MVT::i64 ||
      Op.getOperand(11).getValueType() != MVT::i64 || !MaskHigh ||
      MaskHigh->getZExtValue() != 0 || Zero != 0)
    report_fatal_error(
        "GPR-masked MSCATTER requires an exact admitted S64/U64/U16/S32/U32/FP16/FP32 "
        "element profile, S64/U64 "
        "byte offsets, CUBE_M32, one low mask word and merge-inactive stores");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Cols, Rows}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getOperand(9).getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(Op.getOperand(9));
  Ops.push_back(Op.getOperand(10));
  Ops.push_back(Op.getOperand(11));
  Ops.push_back(DAG.getTargetConstant(PredInv, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Zero, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_MSCATTER_GPR_MASKED, DL,
                     DAG.getVTList(MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMGatherAdd(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  getV5ConstantOperand(Op.getOperand(2), "MGATHER_ADD rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "MGATHER_ADD cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "MGATHER_ADD data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "MGATHER_ADD pad", 3);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {3u, 2u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "MGATHER_ADD data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "MGATHER_ADD pad", 3), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  return DAG.getNode(LinxV5ISD::EW_MGATHER_ADD, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMGatherAddMasked(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "masked MGATHER_ADD rows", 32, false);
  uint64_t Cols = getV5ConstantOperand(Op.getOperand(3), "masked MGATHER_ADD cols", 32, false);
  uint64_t DType = getV5ConstantOperand(Op.getOperand(4), "masked MGATHER_ADD data type", 31);
  uint64_t Pad = getV5ConstantOperand(Op.getOperand(5), "masked MGATHER_ADD pad", 3);
  uint64_t Layout = getV5ConstantOperand(Op.getOperand(6), "masked MGATHER_ADD layout", 31);
  EVT ResultVT = Op.getValueType();
  EVT OffsetVT = Op.getOperand(8).getValueType();
  EVT ValueVT = Op.getOperand(9).getValueType();
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(11));
  if (DType != LinxV5Op::DataType::U32 || Layout != 29 || Rows != 32 ||
      Cols > 2 || MaskHigh == nullptr || MaskHigh->getZExtValue() != 0 ||
      !ResultVT.isFixedLengthVector() || !OffsetVT.isFixedLengthVector() ||
      !ValueVT.isFixedLengthVector() ||
      !ResultVT.getScalarType().isInteger() ||
      !OffsetVT.getScalarType().isInteger() ||
      !ValueVT.getScalarType().isInteger() || Rows > UINT64_MAX / Cols ||
      Rows * Cols != ResultVT.getVectorNumElements() ||
      ResultVT.getVectorNumElements() != OffsetVT.getVectorNumElements() ||
      ResultVT.getVectorNumElements() != ValueVT.getVectorNumElements() ||
      ResultVT.getScalarSizeInBits() != 32 ||
      ValueVT.getScalarSizeInBits() != 32 ||
      OffsetVT.getScalarSizeInBits() != 64 ||
      Op.getOperand(10).getValueType() != MVT::i64 ||
      Op.getOperand(11).getValueType() != MVT::i64)
    report_fatal_error(
        "masked MGATHER_ADD requires exact-shape U32 values, U64 offsets, CUBE_M32 and one low GPR mask word");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Cols, Rows}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Pad, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(Op.getValueType()),
                                      DL, MVT::i64));
  for (unsigned Index = 7; Index <= 11; ++Index)
    Ops.push_back(Op.getOperand(Index));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(12), "masked MGATHER_ADD PredInv", 1),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(13), "masked MGATHER_ADD Zero", 1),
      DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_MGATHER_ADD_MASKED, DL,
                     DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseMScatterAddPredicate(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  uint64_t Rows = getV5ConstantOperand(
      Op.getOperand(2), "predicate atomic rows", 65535, false);
  uint64_t Columns = getV5ConstantOperand(
      Op.getOperand(3), "predicate atomic columns", 65535, false);
  uint64_t PhysicalColumns = getV5ConstantOperand(
      Op.getOperand(4), "predicate atomic physical columns", 65535, false);
  uint64_t DataType = getV5ConstantOperand(
      Op.getOperand(5), "predicate atomic data type", 31);
  uint64_t Layout = getV5ConstantOperand(
      Op.getOperand(6), "predicate atomic layout", 31);
  uint64_t Invert = getV5ConstantOperand(
      Op.getOperand(11), "predicate atomic inversion", 1);
  EVT ValueType = Op.getOperand(8).getValueType();
  EVT OffsetType = Op.getOperand(9).getValueType();
  EVT PredicateType = Op.getOperand(10).getValueType();
  unsigned ElementBits = DataType == LinxV5Op::DataType::U32 ||
                                 DataType == LinxV5Op::DataType::S32 ||
                                 DataType == LinxV5Op::DataType::FP32 ? 32 : 0;
  if (ElementBits == 0 || Layout != 29 ||
      Rows > 32 || Columns > PhysicalColumns ||
      !isPowerOf2_64(PhysicalColumns) || !ValueType.isFixedLengthVector() ||
      !OffsetType.isFixedLengthVector() || !PredicateType.isFixedLengthVector() ||
      ValueType.getScalarSizeInBits() != ElementBits ||
      !(ValueType.getScalarType().isInteger() ||
        (ValueType.getScalarType() == MVT::f32 &&
         DataType == LinxV5Op::DataType::FP32)) ||
      !OffsetType.getScalarType().isInteger() ||
      (OffsetType.getScalarSizeInBits() != 64 &&
       OffsetType.getScalarSizeInBits() != 32) ||
      (PredicateType.getScalarType() != MVT::i8 &&
       PredicateType.getScalarType() != MVT::i32) ||
      ValueType.getVectorNumElements() != 32 * PhysicalColumns ||
      ValueType.getSizeInBits() > 262144 * 8 ||
      OffsetType.getSizeInBits() < 32 * Columns * 64 ||
      OffsetType.getSizeInBits() % 2048 != 0 ||
      !isPowerOf2_64(OffsetType.getSizeInBits() / 2048) ||
      OffsetType.getSizeInBits() > 262144 * 8 ||
      PredicateType.getSizeInBits() < 32 * Columns * 8 ||
      PredicateType.getSizeInBits() % 1024 != 0 ||
      !isPowerOf2_64(PredicateType.getSizeInBits() / 1024) ||
      PredicateType.getSizeInBits() < 1024 ||
      PredicateType.getSizeInBits() > 262144 * 8)
    report_fatal_error("predicate MSCATTER_ADD requires Local CUBE M32, "
                       "exact physical S32/U32/FP32 values, 64-bit byte offsets "
                       "and a U8 PredicateCell carrier");
  SmallVector<SDValue> Operands = {
      Op.getOperand(0), Op.getOperand(8), Op.getOperand(9),
      Op.getOperand(10), Op.getOperand(7)};
  for (unsigned Index : {1u, 2u, 3u})
    while (Operands[Index].getOpcode() == ISD::BITCAST)
      Operands[Index] = Operands[Index].getOperand(0);
  for (uint64_t Immediate : {Rows, Columns, PhysicalColumns, DataType, Layout,
                             Invert})
    Operands.push_back(DAG.getTargetConstant(Immediate, DL, MVT::i64));
  return DAG.getNode(LinxV5ISD::EW_MSCATTER_ADD_PREDICATE, DL,
                     DAG.getVTList(MVT::Other), Operands);
}

SDValue LinxV5TargetLowering::lowerElementwiseMScatterAdd(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  getV5ConstantOperand(Op.getOperand(2), "MSCATTER_ADD rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "MSCATTER_ADD cols", 32, false);
  getV5ConstantOperand(Op.getOperand(4), "MSCATTER_ADD data type", 31);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {3u, 2u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "MSCATTER_ADD data type", 31),
      DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getOperand(6).getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(5));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  return DAG.getNode(LinxV5ISD::EW_MSCATTER_ADD, DL,
                     DAG.getVTList(MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTCmp(SDLoc &DL, SDValue Op,
                                                   SelectionDAG &DAG,
                                                   bool ExplicitRow) const {
  unsigned PhysicalCols = 0;
  if (ExplicitRow) {
    PhysicalCols = getV5ConstantOperand(Op.getOperand(9),
                                        "NORM physical columns", 2048, false);
    uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "NORM rows", 16,
                                        false);
    uint64_t ValidCols = getV5ConstantOperand(
        Op.getOperand(3), "NORM valid columns", 2048, false);
    uint64_t DType =
        getV5ConstantOperand(Op.getOperand(4), "NORM dtype", 31);
    bool U32Row =
        Rows == 1 && DType == LinxV5Op::DataType::U32 &&
        ((PhysicalCols == 128 && (ValidCols == 32 || ValidCols == 128)) ||
         (PhysicalCols == 2048 && ValidCols == 2048));
    bool F32Tile = DType == LinxV5Op::DataType::FP32 &&
                   PhysicalCols == 32 && ValidCols == 32 &&
                   (Rows == 1 || Rows == 16);
    if (getV5ConstantOperand(Op.getOperand(5), "NORM layout", 31) != 0 ||
        (!U32Row && !F32Tile))
      report_fatal_error("unsupported NORM numeric descriptor");
  }

  if (ExplicitRow) {
    uint64_t Rows = cast<ConstantSDNode>(Op.getOperand(2))->getZExtValue();
    uint64_t DType = cast<ConstantSDNode>(Op.getOperand(4))->getZExtValue();
    unsigned PhysicalElements = Rows * PhysicalCols;
    auto IsData = [&](EVT T) {
      return T.isFixedLengthVector() &&
             T.getScalarType() ==
                 (DType == LinxV5Op::DataType::FP32 ? MVT::f32 : MVT::i32) &&
             T.getVectorNumElements() == PhysicalElements;
    };
    auto IsPredicate = [&](EVT T) {
      return T.isFixedLengthVector() && T.getScalarType() == MVT::i32 &&
             T.getVectorNumElements() == std::max(32u, PhysicalElements / 4);
    };
    if (!IsPredicate(Op.getValueType()) ||
        !IsData(Op.getOperand(6).getValueType()) ||
        !IsData(Op.getOperand(7).getValueType()))
      report_fatal_error("NORM TCMP requires matching numeric sources and "
                         "packed predicate capacity");
  }
  getV5ConstantOperand(Op.getOperand(2), "TCMP rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "TCMP cols", 2048, false);
  getV5ConstantOperand(Op.getOperand(4), "TCMP data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "TCMP layout", 31);
  getV5ConstantOperand(Op.getOperand(8), "TCMP mode", 7);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {2u, 3u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "TCMP data type", 31), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "TCMP layout", 31), DL, MVT::i64));
  // Predicate vectors are packed, but Local allocations still have the
  // architectural 128-byte minimum. Do not widen them to U32 lane storage.
  uint64_t PredicateBytes =
      std::max(uint64_t(128), (Op.getValueType().getFixedSizeInBits() + 7) / 8);
  if (!isPowerOf2_64(PredicateBytes))
    report_fatal_error("TCMP predicate capacity must be a power of two");
  Ops.push_back(
      DAG.getTargetConstant(Log2_64(PredicateBytes) - 6, DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(8), "TCMP mode", 7), DL, MVT::i64));
  if (ExplicitRow)
    Ops.push_back(DAG.getTargetConstant(PhysicalCols, DL, MVT::i64));
  return DAG.getNode(ExplicitRow ? LinxV5ISD::EW_TCMP_ROW : LinxV5ISD::EW_TCMP,
                     DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTCmpGPR(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG, bool Masked) const {
  const uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "TCMP GPR rows", 32, false);
  const uint64_t Cols = getV5ConstantOperand(Op.getOperand(3), "TCMP GPR columns", 32, false);
  const uint64_t DataType = getV5ConstantOperand(Op.getOperand(4), "TCMP GPR data type", 31);
  const uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "TCMP GPR layout", 31);
  const uint64_t Cmp = getV5ConstantOperand(Op.getOperand(8), "TCMP GPR comparison", 5);
  const bool F32Profile = Masked && DataType == LinxV5Op::DataType::FP32;
  const bool F16Profile = Masked && DataType == LinxV5Op::DataType::FP16;
  const bool NativeI64 = !Masked && Cols == 1 &&
                         (DataType == LinxV5Op::DataType::S64 ||
                          DataType == LinxV5Op::DataType::U64);
  const bool DenseInteger = !Masked && Rows == 32 && Cols == 2 &&
                            (DataType == LinxV5Op::DataType::S32 ||
                             DataType == LinxV5Op::DataType::U32);
  EVT SourceVT = F16Profile     ? MVT::v64f16
                 : F32Profile   ? MVT::v32f32
                 : DenseInteger ? MVT::v64i32
                 : NativeI64    ? MVT::v32i64
                                : MVT::v32i32;
  if (Rows != 32 || ((F16Profile || DenseInteger) ? Cols != 2 : Cols != 1) ||
      Layout != 29 ||
      (!NativeI64 && DataType != LinxV5Op::DataType::S32 &&
       DataType != LinxV5Op::DataType::U32 && !F32Profile && !F16Profile) ||
      Op.getOperand(6).getValueType() != SourceVT ||
      Op.getOperand(7).getValueType() != SourceVT ||
      (Masked && ((!F32Profile && !F16Profile) ||
                  Op.getOperand(9).getValueType() != MVT::i64)))
    report_fatal_error(
        "TCMP GPR requires exact 32x1 S32/U32 CUBE_M32 Tiles; masked FP32 "
        "supports the same shape");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(DAG.getTargetConstant(Cmp, DL, MVT::i64));
  if (Masked)
    Ops.push_back(Op.getOperand(9));
  return DAG.getNode(Masked ? LinxV5ISD::EW_TCMP_GPR_MASKED
                            : LinxV5ISD::EW_TCMP_GPR, DL,
                     DAG.getVTList(MVT::i64, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTSel(SDLoc &DL, SDValue Op,
                                                   SelectionDAG &DAG,
                                                   bool ExplicitRow) const {
  unsigned PhysicalCols = 0;
  if (ExplicitRow) {
    PhysicalCols = getV5ConstantOperand(Op.getOperand(9),
                                        "NORM physical columns", 2048, false);
    uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "NORM rows", 16,
                                        false);
    uint64_t ValidCols = getV5ConstantOperand(
        Op.getOperand(3), "NORM valid columns", 2048, false);
    uint64_t DType =
        getV5ConstantOperand(Op.getOperand(4), "NORM dtype", 31);
    bool U32Row =
        Rows == 1 && DType == LinxV5Op::DataType::U32 &&
        ((PhysicalCols == 128 && (ValidCols == 32 || ValidCols == 128)) ||
         (PhysicalCols == 2048 && ValidCols == 2048));
    bool F32Tile = DType == LinxV5Op::DataType::FP32 &&
                   PhysicalCols == 32 && ValidCols == 32 &&
                   (Rows == 1 || Rows == 16);
    if (getV5ConstantOperand(Op.getOperand(5), "NORM layout", 31) != 0 ||
        (!U32Row && !F32Tile))
      report_fatal_error("unsupported NORM numeric descriptor");
  }

  if (ExplicitRow) {
    uint64_t Rows = cast<ConstantSDNode>(Op.getOperand(2))->getZExtValue();
    uint64_t DType = cast<ConstantSDNode>(Op.getOperand(4))->getZExtValue();
    unsigned PhysicalElements = Rows * PhysicalCols;
    auto IsData = [&](EVT T) {
      return T.isFixedLengthVector() &&
             T.getScalarType() ==
                 (DType == LinxV5Op::DataType::FP32 ? MVT::f32 : MVT::i32) &&
             T.getVectorNumElements() == PhysicalElements;
    };
    auto IsPredicate = [&](EVT T) {
      return T.isFixedLengthVector() && T.getScalarType() == MVT::i32 &&
             T.getVectorNumElements() == std::max(32u, PhysicalElements / 4);
    };
    if (!IsData(Op.getValueType()) ||
        !IsPredicate(Op.getOperand(6).getValueType()) ||
        !IsData(Op.getOperand(7).getValueType()) ||
        !IsData(Op.getOperand(8).getValueType()))
      report_fatal_error("NORM TSEL requires a packed predicate and matching "
                         "numeric carriers");
  }
  getV5ConstantOperand(Op.getOperand(2), "TSEL rows", 32, false);
  getV5ConstantOperand(Op.getOperand(3), "TSEL cols", 2048, false);
  getV5ConstantOperand(Op.getOperand(4), "TSEL data type", 31);
  getV5ConstantOperand(Op.getOperand(5), "TSEL layout", 31);
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0));
  for (unsigned Index : {2u, 3u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "TSEL data type", 31), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "TSEL layout", 31), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  Ops.push_back(Op.getOperand(8));
  if (ExplicitRow)
    Ops.push_back(DAG.getTargetConstant(PhysicalCols, DL, MVT::i64));
  return DAG.getNode(ExplicitRow ? LinxV5ISD::EW_TSEL_ROW : LinxV5ISD::EW_TSEL,
                     DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTSelGPR(
    SDLoc &DL, SDValue Op, SelectionDAG &DAG) const {
  const uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "TSEL GPR rows", 32, false);
  const uint64_t Cols = getV5ConstantOperand(Op.getOperand(3), "TSEL GPR columns", 32, false);
  const uint64_t DataType = getV5ConstantOperand(Op.getOperand(4), "TSEL GPR data type", 31);
  const uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "TSEL GPR layout", 31);
  const auto *MaskHigh = dyn_cast<ConstantSDNode>(Op.getOperand(7));
  EVT ResultVT = Op.getValueType();
  const bool F32Profile = DataType == LinxV5Op::DataType::FP32;
  const bool F16Profile = DataType == LinxV5Op::DataType::FP16;
  const bool U16Profile = DataType == LinxV5Op::DataType::U16;
  const bool NativeI64 = Cols == 1 &&
                         (DataType == LinxV5Op::DataType::S64 ||
                          DataType == LinxV5Op::DataType::U64);
  const bool DenseInteger = Cols == 2 && (DataType == LinxV5Op::DataType::S32 ||
                                          DataType == LinxV5Op::DataType::U32);
  if (Rows != 32 ||
      ((F16Profile || U16Profile || DenseInteger) ? Cols != 2 : Cols != 1) ||
      Layout != 29 ||
      (DataType != LinxV5Op::DataType::S32 &&
       DataType != LinxV5Op::DataType::U32 && !F32Profile && !F16Profile &&
       !U16Profile && !NativeI64) ||
      (U16Profile     ? ResultVT != MVT::v64i16
       : F16Profile   ? ResultVT != MVT::v64f16
       : F32Profile   ? ResultVT != MVT::v32f32
       : DenseInteger ? ResultVT != MVT::v64i32
       : NativeI64    ? ResultVT != MVT::v32i64
                      : ResultVT != MVT::v32i32) ||
      Op.getOperand(6).getValueType() != MVT::i64 || !MaskHigh ||
      MaskHigh->getZExtValue() != 0 ||
      Op.getOperand(8).getValueType() != ResultVT ||
      Op.getOperand(9).getValueType() != ResultVT)
    report_fatal_error(
        "TSEL GPR requires an exact admitted 32x1 or dense64 CUBE_M32 Tile "
        "and one low mask word");
  SmallVector<SDValue> Ops = {Op.getOperand(0)};
  for (uint64_t Dim : {Rows, Cols}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(Dim, DL, MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(DataType, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
  Ops.push_back(DAG.getTargetConstant(calculateVCallSizeMask(ResultVT), DL,
                                      MVT::i64));
  Ops.push_back(Op.getOperand(8));
  Ops.push_back(Op.getOperand(9));
  Ops.push_back(Op.getOperand(6));
  Ops.push_back(Op.getOperand(7));
  return DAG.getNode(LinxV5ISD::EW_TSEL_GPR, DL,
                     DAG.getVTList(ResultVT, MVT::Other), Ops);
}

SDValue LinxV5TargetLowering::lowerElementwiseTExpands(SDLoc &DL, SDValue Op,
                                                       SelectionDAG &DAG,
                                                       bool ExplicitRow) const {
  unsigned PhysicalCols = 0;
  if (ExplicitRow) {
    PhysicalCols = getV5ConstantOperand(Op.getOperand(7),
                                        "NORM physical columns", 2048, false);
    uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "NORM rows", 16,
                                        false);
    uint64_t ValidCols = getV5ConstantOperand(
        Op.getOperand(3), "NORM valid columns", 2048, false);
    uint64_t DType =
        getV5ConstantOperand(Op.getOperand(4), "NORM dtype", 31);
    bool U32Row =
        Rows == 1 && DType == LinxV5Op::DataType::U32 &&
        ((PhysicalCols == 128 && (ValidCols == 32 || ValidCols == 128)) ||
         (PhysicalCols == 2048 && ValidCols == 2048));
    bool F32Tile = DType == LinxV5Op::DataType::FP32 &&
                   PhysicalCols == 32 && ValidCols == 32 &&
                   (Rows == 1 || Rows == 16);
    if (getV5ConstantOperand(Op.getOperand(5), "NORM layout", 31) != 0 ||
        (!U32Row && !F32Tile))
      report_fatal_error("unsupported NORM numeric descriptor");
  }

  const uint64_t Rows = getV5ConstantOperand(Op.getOperand(2), "TEXPANDS rows", 32, false);
  const uint64_t Cols =
      getV5ConstantOperand(Op.getOperand(3), "TEXPANDS cols", 2048, false);
  const uint64_t DType = getV5ConstantOperand(Op.getOperand(4), "TEXPANDS data type", 31);
  const uint64_t Layout = getV5ConstantOperand(Op.getOperand(5), "TEXPANDS layout", 31);
  if (Op.getOperand(6).getValueType().isInteger() &&
      (DType != LinxV5Op::DataType::U32 || Layout != 0 || Rows != 1 ||
       (Cols != 128 && Cols != 2048 && !(ExplicitRow && Cols == 32)) ||
       !Op.getValueType().isFixedLengthVector() ||
       Op.getValueType().getVectorNumElements() !=
           (ExplicitRow ? PhysicalCols : Cols) ||
       Op.getValueType().getScalarType() != MVT::i32 ||
       Op.getOperand(6).getValueType() != MVT::i64))
    report_fatal_error("integer TEXPANDS requires an exact full NORM U32 row");
  if (DType == LinxV5Op::DataType::FP32) {
    bool AdmittedProfile =
        (Rows == 32 && Cols == 1 && Layout == 29 &&
         Op.getValueType() == MVT::v32f32) ||
        (ExplicitRow && Layout == 0 && Cols == 32 && PhysicalCols == 32 &&
         ((Rows == 1 && Op.getValueType() == MVT::v32f32) ||
          (Rows == 16 && Op.getValueType() == MVT::v512f32)));
    if (!AdmittedProfile || Op.getOperand(6).getValueType() != MVT::f32)
      report_fatal_error(
          "FP32 TEXPANDS requires an exact admitted f32 Tile and scalar");
  }
  SDValue Chain = Op.getOperand(0);
  SDValue Scalar = Op.getOperand(6);
  if (ExplicitRow && DType == LinxV5Op::DataType::U32) {
    // Keep an explicit source binding even for zero. Its temporary belongs to
    // the register allocator; no physical scratch register is borrowed.
    Register BoundScalar =
        DAG.getMachineFunction().getRegInfo().createVirtualRegister(
            &LinxV5::GRNoR0RegClass);
    Chain = DAG.getCopyToReg(Chain, DL, BoundScalar, Scalar);
    Scalar = DAG.getCopyFromReg(Chain, DL, BoundScalar, MVT::i64);
    Chain = Scalar.getValue(1);
  }
  SmallVector<SDValue> Ops;
  Ops.push_back(Chain);
  for (unsigned Index : {2u, 3u}) {
    Ops.push_back(DAG.getRegister(LinxV5::R0, MVT::i64));
    Ops.push_back(DAG.getTargetConstant(
        cast<ConstantSDNode>(Op.getOperand(Index))->getZExtValue(), DL,
        MVT::i64));
  }
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(4), "TEXPANDS data type", 31), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      getV5ConstantOperand(Op.getOperand(5), "TEXPANDS layout", 31), DL,
      MVT::i64));
  Ops.push_back(DAG.getTargetConstant(
      calculateVCallSizeMask(Op.getValueType()), DL, MVT::i64));
  Ops.push_back(Scalar);
  if (ExplicitRow)
    Ops.push_back(DAG.getTargetConstant(PhysicalCols, DL, MVT::i64));
  return DAG.getNode(ExplicitRow ? LinxV5ISD::EW_TEXPANDS_ROW
                                 : LinxV5ISD::EW_TEXPANDS,
                     DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
}

/// Intrinsic Ops for blk_matmul_shared:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type A; (6): Tile Element Type B;
/// (7): Local Tile Input A; (8): Shared SSA handle.
/// The Shared Right (B) is NOT a tile operand here — it is bound by the 32-bit B.IOS
/// at MC expansion. Node result is the implicit ACC tile plus Other.
SDValue LinxV5TargetLowering::lowerTemplateBLKShared(SDLoc &DL, SDValue Op,
                                                      SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementTypeA = Op.getOperand(5);
  unsigned TypeEnumA = cast<ConstantSDNode>(TileElementTypeA)->getZExtValue();
  Ops.push_back(DAG.getTargetConstant(TypeEnumA, DL, MVT::i64));

  SDValue TileElementTypeB = Op.getOperand(6);
  unsigned TypeEnumB = cast<ConstantSDNode>(TileElementTypeB)->getZExtValue();
  Ops.push_back(DAG.getTargetConstant(TypeEnumB, DL, MVT::i64));

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  Ops.push_back(DAG.getTargetConstant(TileSize, DL, MVT::i64));

  SDValue TileUseA = Op.getOperand(7);
  if (TileUseA.isUndef()) {
    Op->print(errs(), &DAG);
    report_fatal_error("\nPlease initialize tile register before use!");
  }
  Ops.push_back(TileUseA); // Local A tile (B is Shared, bound by B.IOS)

  Ops.push_back(Op.getOperand(8));

  SDValue VCall = DAG.getNode(
      LinxV5ISD::BLK_MATMUL_SHARED, DL,
      DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0) Chain; (1) IntNo; (2,3,4): Dimensions;
/// (5,6): AType, BType; (7~): Tile Input
SDValue LinxV5TargetLowering::lowerTemplateBLKMX(unsigned Opcode, SDLoc &DL,
                                                 SDValue Op, unsigned VUseNum,
                                                 SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-M
  Ops.push_back(Op.getOperand(3)); // Dim-N
  Ops.push_back(Op.getOperand(4)); // Dim-K

  SDValue AType = Op.getOperand(5);
  unsigned ATypeEnum = cast<ConstantSDNode>(AType)->getZExtValue();
  SDValue ATypeValue = DAG.getTargetConstant(ATypeEnum, DL, MVT::i64);
  Ops.push_back(ATypeValue);

  SDValue BType = Op.getOperand(6);
  unsigned BTypeEnum = cast<ConstantSDNode>(BType)->getZExtValue();
  SDValue BTypeValue = DAG.getTargetConstant(BTypeEnum, DL, MVT::i64);
  Ops.push_back(BTypeValue);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  for (unsigned i = 0; i < VUseNum; ++i) {
    SDValue TileUse = Op.getOperand(i + 7);
    Ops.push_back(TileUse); // Tile Uses
    if (TileUse.isUndef()) {
      Op->print(errs(), &DAG);
      report_fatal_error("\nPlease initialize tile register before use!");
    }
  }

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6): PadValue; (7): Layout
/// (8): DType, (9): Stride
SDValue LinxV5TargetLowering::lowerTLoad(unsigned Opcode, SDLoc &DL, SDValue Op,
                                         SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementType = Op.getOperand(5);
  unsigned TypeEnum = cast<ConstantSDNode>(TileElementType)->getZExtValue();
  SDValue TypeEnumValue = DAG.getTargetConstant(TypeEnum, DL, MVT::i64);
  Ops.push_back(TypeEnumValue);

  unsigned TileSize = calculateVCallSizeMask(Op.getValueType());
  SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
  Ops.push_back(SizeValue);

  SDValue PadValueType = Op.getOperand(6);
  SDValue LayoutType = Op.getOperand(7);
  unsigned PadValueTypeEnum =
      cast<ConstantSDNode>(PadValueType)->getSExtValue();
  unsigned LayoutTypeEnum = cast<ConstantSDNode>(LayoutType)->getSExtValue();
  Ops.push_back(DAG.getTargetConstant(LayoutTypeEnum, DL, MVT::i64)); // Layout
  Ops.push_back(
      DAG.getTargetConstant(PadValueTypeEnum, DL, MVT::i64)); // PadValue

  Ops.push_back(Op.getOperand(8)); // DTypePtr
  Ops.push_back(Op.getOperand(9)); // Stride

  SDValue VCall = DAG.getNode(
      Opcode, DL, DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
  return VCall;
}

/// Intrinsic Ops:
/// (0)Chain; (1)IntNo; (2,3,4): Dimensions;
/// (5): Tile Element Type; (6): Layout
/// (7): DType, (8): Stride, (9): Tile Input
SDValue LinxV5TargetLowering::lowerTStore(unsigned Opcode, SDLoc &DL,
                                          SDValue Op, SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;

  SDValue Chain = Op.getOperand(0);
  Ops.push_back(Chain);

  Ops.push_back(Op.getOperand(2)); // Dim-X
  Ops.push_back(Op.getOperand(3)); // Dim-Y
  Ops.push_back(Op.getOperand(4)); // Dim-Z

  SDValue TileElementType = Op.getOperand(5);
  unsigned TypeEnum = cast<ConstantSDNode>(TileElementType)->getZExtValue();
  SDValue TypeEnumValue = DAG.getTargetConstant(TypeEnum, DL, MVT::i64);
  Ops.push_back(TypeEnumValue);

  SDValue LayoutType = Op.getOperand(6);
  unsigned LayoutTypeEnum = cast<ConstantSDNode>(LayoutType)->getSExtValue();
  Ops.push_back(DAG.getTargetConstant(LayoutTypeEnum, DL, MVT::i64)); // Layout

  Ops.push_back(Op.getOperand(7)); // DTypePtr
  Ops.push_back(Op.getOperand(8)); // Stride

  SDValue TileUse = Op.getOperand(9);
  Ops.push_back(TileUse); // Tile Use
  if (TileUse.isUndef()) {
    Op->print(errs(), &DAG);
    report_fatal_error("\nPlease initialize tile register before use!");
  }

  SDValue VCall = DAG.getNode(Opcode, DL, DAG.getVTList(MVT::Other), Ops);
  return VCall;
}

SDValue LinxV5TargetLowering::lowerACCCVT(SDLoc &DL, SDValue Op,
                                          SelectionDAG &DAG) const {
  SmallVector<SDValue> Ops;
  Ops.push_back(Op.getOperand(0)); // chain
  Ops.push_back(Op.getOperand(2)); // M
  Ops.push_back(Op.getOperand(3)); // N
  Ops.push_back(Op.getOperand(4)); // SrcType
  Ops.push_back(Op.getOperand(5)); // DstType
  Ops.push_back(DAG.getConstant(calculateVCallSizeMask(Op.getValueType()), DL,
                                MVT::i64)); // TileSize
  Ops.push_back(Op.getOperand(6));          // Layout
  Ops.push_back(Op.getOperand(7));          // canon
  Ops.push_back(Op.getOperand(8));          // ACC tile
  SDValue Res = DAG.getNode(LinxV5ISD::BLK_ACCCVT, DL,
                            {Op.getValueType(), MVT::Other}, Ops);
  return Res;
}

SDValue LinxV5TargetLowering::lowerLOAD(SDValue Op, SelectionDAG &DAG) const {
  SDLoc DL(Op);
  LoadSDNode *LD = cast<LoadSDNode>(Op);
  EVT VT = LD->getValueType(0);

  // A sub-128 B memory region has no B.IOT SizeCode. Type legalization
  // normally widens such loads away (the small vector types are not
  // registered as tile registers), but a widened value can still feed a
  // small memory load, so keep the default scalar lowering here.
  if (VT.isFixedLengthVector() &&
      VT.getStoreSize() < TypeSize::Fixed(128))
    return scalarizeVectorLoad(LD, DAG).first;
  if (VT.isFixedLengthVector()) {
    SmallVector<SDValue> Ops;
    Ops.push_back(LD->getChain());
    bool Elementwise =
        DAG.getMachineFunction().getFunction().hasFnAttribute(
            "linx.elementwise");
    unsigned ColValid = Elementwise ? VT.getVectorNumElements() / 16
                                    : VT.getFixedSizeInBits() / 64;
    unsigned RowValid = Elementwise ? 16 : 1;
    unsigned Col = Elementwise ? 1 : ColValid;
    Ops.push_back(DAG.getTargetConstant(ColValid, DL, MVT::i64)); // Dim-X
    Ops.push_back(DAG.getTargetConstant(RowValid, DL, MVT::i64)); // Dim-Y
    Ops.push_back(DAG.getTargetConstant(Col, DL, MVT::i64)); // Dim-Z

    unsigned DataType = LinxV5Op::DataType::S64;
    if (VT.getVectorElementType() == MVT::f32)
      DataType = LinxV5Op::DataType::FP32;
    else if (VT.getVectorElementType() == MVT::f16)
      DataType = LinxV5Op::DataType::FP16;
    else if (VT.getVectorElementType() == MVT::i8)
      DataType = LinxV5Op::DataType::U8;
    else if (VT.getVectorElementType() == MVT::i16)
      DataType = LinxV5Op::DataType::S16;
    else if (VT.getVectorElementType() == MVT::i32)
      DataType = LinxV5Op::DataType::S32;
    SDValue DataTypeValue =
        DAG.getTargetConstant(DataType, DL, MVT::i64);
    Ops.push_back(DataTypeValue); // DataType
    unsigned TileSize = calculateVCallSizeMask(VT);
    SDValue SizeValue = DAG.getTargetConstant(TileSize, DL, MVT::i64);
    Ops.push_back(SizeValue);

    const unsigned Layout =
        Elementwise && VT.getVectorElementType() == MVT::f32
            ? LinxV5Op::ArgFormat::ND2M16
            : LinxV5Op::ArgFormat::NORM;
    Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));
    Ops.push_back(
        DAG.getTargetConstant(LinxV5Op::PadValue::Zero, DL, MVT::i64));

    Ops.push_back(LD->getBasePtr());
    SDValue RowStride = DAG.getRegister(LinxV5::R0, MVT::i64);
    if (Elementwise) {
      const unsigned ElementBytes =
          VT.getVectorElementType().getStoreSize().getFixedValue();
      RowStride = DAG.getConstant(ColValid * ElementBytes, DL, MVT::i64);
    }
    Ops.push_back(RowStride);
    SDValue VCall =
        DAG.getNode(LinxV5ISD::BLK_TLOAD, DL,
                    DAG.getVTList(Op.getValueType(), MVT::Other), Ops);
    return VCall;
  }
  return SDValue();
}

SDValue LinxV5TargetLowering::lowerSTORE(SDValue Op, SelectionDAG &DAG) const {
  StoreSDNode *ST = cast<StoreSDNode>(Op);
  SDLoc DL(Op);

  EVT ValueVT = ST->getValue().getValueType();
  EVT MemVT = ST->getMemoryVT();
  // Same SizeCode guard as lowerLOAD: a widened store value with a sub-128 B
  // memory region must not become BLK_TSTORE.
  if (ValueVT.isFixedLengthVector() && MemVT.getStoreSize() < TypeSize::Fixed(128))
    return scalarizeVectorStore(ST, DAG);
  if (ValueVT.isFixedLengthVector()) {
    SmallVector<SDValue> Ops;
    Ops.push_back(ST->getChain());
    bool Elementwise =
        DAG.getMachineFunction().getFunction().hasFnAttribute(
            "linx.elementwise");
    unsigned ColValid = Elementwise ? MemVT.getVectorNumElements() / 16
                                    : MemVT.getFixedSizeInBits() / 64;
    unsigned RowValid = Elementwise ? 16 : 1;
    unsigned Col = Elementwise ? 1 : ColValid;
    Ops.push_back(DAG.getTargetConstant(ColValid, DL, MVT::i64)); // Dim-X
    Ops.push_back(DAG.getTargetConstant(RowValid, DL, MVT::i64)); // Dim-Y
    Ops.push_back(DAG.getTargetConstant(Col, DL, MVT::i64)); // Dim-Z

    unsigned DataType = LinxV5Op::DataType::S64;
    if (MemVT.getVectorElementType() == MVT::f32)
      DataType = LinxV5Op::DataType::FP32;
    else if (MemVT.getVectorElementType() == MVT::f16)
      DataType = LinxV5Op::DataType::FP16;
    else if (MemVT.getVectorElementType() == MVT::i8)
      DataType = LinxV5Op::DataType::U8;
    else if (MemVT.getVectorElementType() == MVT::i16)
      DataType = LinxV5Op::DataType::S16;
    else if (MemVT.getVectorElementType() == MVT::i32)
      DataType = LinxV5Op::DataType::S32;
    SDValue DataTypeValue =
        DAG.getTargetConstant(DataType, DL, MVT::i64);
    Ops.push_back(DataTypeValue); // DataType

    const unsigned Layout =
        Elementwise && MemVT.getVectorElementType() == MVT::f32
            ? LinxV5Op::ArgFormat::M162ND
            : LinxV5Op::ArgFormat::NORM;
    Ops.push_back(DAG.getTargetConstant(Layout, DL, MVT::i64));

    Ops.push_back(ST->getBasePtr());
    SDValue RowStride = DAG.getRegister(LinxV5::R0, MVT::i64);
    if (Elementwise) {
      const unsigned ElementBytes =
          MemVT.getVectorElementType().getStoreSize().getFixedValue();
      RowStride = DAG.getConstant(ColValid * ElementBytes, DL, MVT::i64);
    }
    Ops.push_back(RowStride);
    Ops.push_back(ST->getValue());
    SDValue VCall =
        DAG.getNode(LinxV5ISD::BLK_TSTORE, DL, DAG.getVTList(MVT::Other), Ops);
    return VCall;
  }
  return SDValue();
}

SDValue LinxV5TargetLowering::LowerINTRINSIC_VOID(SDValue Op,
                                                  SelectionDAG &DAG) const {
  SDValue Chain = Op.getOperand(0);
  SDLoc DL(Op);
  unsigned IntNo = cast<ConstantSDNode>(Op.getOperand(1))->getZExtValue();
  auto calcNumTileUses = [](SDValue Op) {
    unsigned uNum = 0;
    for (auto &O : Op->ops()) {
      if (O.getValueType().isVector())
        ++uNum;
    }
    return uNum;
  };
  if (IntNo == Intrinsic::linx_scalar_dma ||
      IntNo == Intrinsic::linx_scalar_bwt ||
      IntNo == Intrinsic::linx_scalar_sync_core) {
    if (!Subtarget.isScalar() || Subtarget.enableLegacyISel())
      report_fatal_error("scalar system intrinsic requires native scalar codegen");
    unsigned Opc = IntNo == Intrinsic::linx_scalar_dma ? LinxV5::DMA
                   : IntNo == Intrinsic::linx_scalar_bwt ? LinxV5::BWT
                                                        : LinxV5::SYNC_CORE;
    SmallVector<SDValue, 3> Ops;
    if (IntNo == Intrinsic::linx_scalar_dma) {
      // LLVM follows memcpy's (destination, source) convention; the encoded
      // instruction uses [SrcL=source], SrcR=destination.
      Ops = {Op.getOperand(3), Op.getOperand(2)};
    } else if (IntNo == Intrinsic::linx_scalar_bwt) {
      Ops.push_back(Op.getOperand(2));
    }
    Ops.push_back(Chain);
    return SDValue(DAG.getMachineNode(Opc, DL, MVT::Other, Ops), 0);
  }
