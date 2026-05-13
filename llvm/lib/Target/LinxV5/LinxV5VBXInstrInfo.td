//===-- LinxV5InstrInfo.td - Target Description for LinxV5 ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the LinxV5 instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//

class ImmAsmOperand<string prefix, int width, string suffix> : AsmOperandClass {
  let Name = prefix # "Imm" # width # suffix;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", Name);
}

class SImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"S", width, suffix> {
}

class UImmAsmOperand<int width, string suffix = "">
    : ImmAsmOperand<"U", width, suffix> {
}

def UImmLog2XLenAsmOperand : AsmOperandClass {
  let Name = "UImmLog2XLen";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidUImmLog2XLen";
}

def uimm2 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<2>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<2>;
  let DecoderMethod = "decodeUImmOperand<2>";
  let OperandType = "OPERAND_UIMM2";
  let OperandNamespace = "LinxV5Op";
}

def uimm3 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<3>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<3>;
  let DecoderMethod = "decodeUImmOperand<3>";
  let OperandType = "OPERAND_UIMM3";
  let OperandNamespace = "LinxV5Op";
}

def uimm4 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<4>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<4>;
  let DecoderMethod = "decodeUImmOperand<4>";
  let OperandType = "OPERAND_UIMM4";
  let OperandNamespace = "LinxV5Op";
}

def simm4 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<4>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<4>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<7>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<4>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM4";
  let OperandNamespace = "LinxV5Op";
}

def uimm5 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<5>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<5>;
  let DecoderMethod = "decodeUImmOperand<5>";
  let OperandType = "OPERAND_UIMM5";
  let OperandNamespace = "LinxV5Op";
}

def simm5 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<5>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<5>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<5>";
  let OperandType = "OPERAND_SIMM5";
  let OperandNamespace = "LinxV5Op";
}

def uimm6 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<6>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<6>;
  let DecoderMethod = "decodeUImmOperand<6>";
  let OperandType = "OPERAND_UIMM6";
  let OperandNamespace = "LinxV5Op";
}

def uimm7 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<7>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<7>;
  let DecoderMethod = "decodeUImmOperand<7>";
  let OperandType = "OPERAND_UIMM7";
  let OperandNamespace = "LinxV5Op";
}

def simm7 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<7>(Imm) && !(Imm & 0x3);}]> {
  let ParserMatchClass = SImmAsmOperand<7>;
  let DecoderMethod = "decodeSImmOperand<7>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<7>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM7";
  let OperandNamespace = "LinxV5Op";
}

def simm8 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<8>(Imm) && !(Imm & 0x7);}]> {
  let ParserMatchClass = SImmAsmOperand<8>;
  let DecoderMethod = "decodeSImmOperand<8>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<8>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM8";
  let OperandNamespace = "LinxV5Op";
}

class uimm10vt<ValueType vt> : Operand<vt>, ImmLeaf<vt, [{return isUInt<10>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<10>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<10>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<10>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM10";
  let OperandNamespace = "LinxV5Op";
}

class uimm14vt<ValueType vt> : Operand<vt>, ImmLeaf<vt, [{return isUInt<14>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<14>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<14>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<14>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM14";
  let OperandNamespace = "LinxV5Op";
}

class simm10vt<ValueType vt> : Operand<vt>, ImmLeaf<vt, [{return isInt<10>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<10>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<10>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<10>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM10";
  let OperandNamespace = "LinxV5Op";
}

class simm14vt<ValueType vt> : Operand<vt>, ImmLeaf<vt, [{return isInt<14>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<14>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<14>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<14>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM14";
  let OperandNamespace = "LinxV5Op";
}

def uimm10 : uimm10vt<i64>;
def simm10 : simm10vt<i64>;
def uimm10i32 : uimm10vt<i32>;
def simm10i32 : simm10vt<i32>;

def uimm14 : uimm14vt<i64>;
def simm14 : simm14vt<i64>;
def uimm14i32 : uimm14vt<i32>;
def simm14i32 : simm14vt<i32>;

class simm12vt<ValueType vt> : Operand<vt>, ImmLeaf<vt, [{return isInt<12>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<12>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<12>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<12>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM12";
  let OperandNamespace = "LinxV5Op";
}

def simm12 : simm12vt<iAny>;
def simm12i64 : simm12vt<i64>;

def simm24i64 : Operand<i64>, ImmLeaf<i64, [{return isInt<24>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<24>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<24>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<24>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM24";
  let OperandNamespace = "LinxV5Op";
}

def uimm15 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<15>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<15>;
  let DecoderMethod = "decodeUImmOperand<15>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<15>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM15";
  let OperandNamespace = "LinxV5Op";
}

def uimm16 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<16>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<16>;
  let DecoderMethod = "decodeUImmOperand<16>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<16>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM16";
  let OperandNamespace = "LinxV5Op";
}

def uimm17 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<17>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<17>;
  let DecoderMethod = "decodeUImmOperand<17>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<17>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM17";
  let OperandNamespace = "LinxV5Op";
}

def uimm19 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<19>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<19>;
  let DecoderMethod = "decodeUImmOperand<19>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<19>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM19";
  let OperandNamespace = "LinxV5Op";
}

class UImm20Operand : Operand<XLenVT> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<20>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM20";
  let OperandNamespace = "LinxV5Op";
}

def Simm21Lsb0JALAsmOperand : SImmAsmOperand<21, "Lsb0JAL"> {
  let ParserMethod = "parseJALOffset";
}

// A 21-bit signed immediate where the least significant bit is zero.
def simm21_lsb0_jal : Operand<OtherVT> {
  let ParserMatchClass = Simm21Lsb0JALAsmOperand;
  let PrintMethod = "printBranchOperand";
  let DecoderMethod = "decodeSImmOperandAndLsl1<21>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// Standalone (codegen-only) immleaf patterns.
def simm32     : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<32>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<32>;
  let DecoderMethod = "decodeSImmOperand<32>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<32>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM32";
  let OperandNamespace = "LinxV5Op";
}

def uimm32     : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<32>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<32>;
  let DecoderMethod = "decodeUImmOperand<32>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<32>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM32";
  let OperandNamespace = "LinxV5Op";
}

def simm29     : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<29>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<29>;
  let DecoderMethod = "decodeSImmOperand<29>";
  let EncoderMethod = "getImmOpValue";
  let PrintMethod = "printBranchOperand";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<29>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

def simm42     : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<42>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<42>;
  let DecoderMethod = "decodeSImmOperand<42>";
  let EncoderMethod = "getImmOpValue";
  let PrintMethod = "printBranchOperand";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<42>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

def BareSymbol : AsmOperandClass {
  let Name = "BareSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidBareSymbol";
  let ParserMethod = "parseBareSymbol";
}

def CallSymbol : AsmOperandClass {
  let Name = "CallSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidCallSymbol";
  let ParserMethod = "parseCallSymbol";
}

// A bare symbol used in call/tail only.
def call_symbol : Operand<XLenVT> {
  let ParserMatchClass = CallSymbol;
}

def PseudoJumpSymbol : AsmOperandClass {
  let Name = "PseudoJumpSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidPseudoJumpSymbol";
  let ParserMethod = "parsePseudoJumpSymbol";
}

def TPRelAddSymbol : AsmOperandClass {
  let Name = "TPRelAddSymbol";
  let RenderMethod = "addImmOperands";
  let DiagnosticType = "InvalidTPRelAddSymbol";
  let ParserMethod = "parseOperandWithModifier";
}

// A parameterized register class alternative to i32imm/i64imm from Target.td.
def ixlenimm : Operand<XLenVT>;

// Addressing modes.
// Necessary because a frameindex can't be matched directly in a pattern.
def AddrFI : ComplexPattern<iPTR, 1, "SelectAddrFI", [frameindex], []>;

//===----------------------------------------------------------------------===//
// Operand and SDNode transformation definitions.
//===----------------------------------------------------------------------===//

def linxuimm8 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<8>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<8>;
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_UIMM8";
  let OperandNamespace = "LinxV5Op";
}

def linxsimm64_symbol : Operand<XLenVT> {
  let ParserMatchClass = SImmAsmOperand<64>;
  let DecoderMethod = "decodeSImmOperand<64>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<64>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM64";
  let OperandNamespace = "LinxV5Op";
}

// A bare symbol.
def linxbaresymbol : Operand<XLenVT> {
  let ParserMatchClass = BareSymbol;
}

def linxbareblock : Operand<OtherVT> {
	let PrintMethod = "printBareBlock";
}

// A mask value that won't affect significant shift bits.
def linximmbottomxlenset : ImmLeaf<XLenVT, [{
  if (Subtarget->isScalar())
    return countTrailingOnes<uint64_t>(Imm) >= 6;
  return countTrailingOnes<uint64_t>(Imm) >= 5;
}]>;


//===----------------------------------------------------------------------===//
// Instruction Formats
//===----------------------------------------------------------------------===//

include "LinxV5InstrFormats.td"

/// Predicates

def assertsexti32 : PatFrag<(ops node:$src), (assertsext node:$src), [{
  return cast<VTSDNode>(N->getOperand(1))->getVT().bitsLE(MVT::i32);
}]>;
def sexti32 : PatFrags<(ops node:$src),
                       [(sext_inreg node:$src, i32),
                        (assertsexti32 node:$src)]>;
def assertzexti32 : PatFrag<(ops node:$src), (assertzext node:$src), [{
  return cast<VTSDNode>(N->getOperand(1))->getVT().bitsLE(MVT::i32);
}]>;
def zexti32 : PatFrags<(ops node:$src),
                       [(and node:$src, 0xffffffff),
                        (assertzexti32 node:$src)]>;

let usesCustomInserter = 1 in
class SelectCC_rrirr<RegisterClass valty, RegisterClass cmpty>
    : Pseudo<(outs valty:$dst),
             (ins cmpty:$lhs, cmpty:$rhs, ixlenimm:$imm,
              valty:$truev, valty:$falsev),
             [(set i64:$dst, (linxv5_selectcc i64:$lhs, i64:$rhs,
              (XLenVT imm:$imm), i64:$truev, i64:$falsev))]>;

def Select_GPR_Using_CC_GPR : SelectCC_rrirr<MCSrc, MCSrc>;

let Predicates = [isScalar], isBarrier = 1, isBranch = 1, isTerminator = 1 in
def PseudoBR : Pseudo<(outs), (ins simm21_lsb0_jal:$imm20), [(br bb:$imm20)]>;

let isBarrier = 1, isReturn = 1, isTerminator = 1 in
def PseudoRET : Pseudo<(outs), (ins), [(linxv5_ret_flag)]>;

//===----------------------------------------------------------------------===//
// Pseudo Instruction Class Templates
//===----------------------------------------------------------------------===//

class VBX_PSEUDO_RR<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs1, VBXTR:$rs2), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs1, $rs2");
}

class VBX_PSEUDOSTORE_RR<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs2, VBXTR:$rs1), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 1;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs2, $rs1");
}

class VBX_PSEUDOLOAD_R<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 1;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs");
}

class VBX_PSEUDOFCVT_R<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs");
}


class VBX_PSEUDOBRCOND_RRs<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1, VBXTR:$rs2, linxbareblock:$block), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1, $rs2, $block");
}

class VBX_PSEUDOSETCCOND_RR<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1, VBXTR:$rs2), [], opcodestr> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
  let isBranch = 0;
  let isTerminator = 0;
  let AsmString = !strconcat(opcodestr, "\t$rs1, $rs2");
}

class VBX_PSEUDOSETC_R<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1), [], opcodestr> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1");
}

class VBX_PSEUDOBRIND<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let isBarrier = 1;
  let isBranch = 1;
  let isIndirectBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1");
}

class VBX_PSEUDOUNARY_R<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs1), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs1");
}

class VBX_PSEUDOJUMP_s<string opcodestr> : Pseudo<(outs), (ins linxbareblock:$block), [], opcodestr> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$block");
}

class VBX_PSEUDOJUMP_R<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1), [], opcodestr> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
  let isBarrier = 1;
  let isBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1");
}

class VBX_PSEUDOCONDJUMP_R<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1, VBXTR:$rs2), [], opcodestr> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1, $rs2");
}

class VBX_PSEUDOCONDJUMP_s<string opcodestr> : Pseudo<(outs), (ins VBXTR:$rs1, linxbareblock:$block), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let isBranch = 1;
  let isTerminator = 1;
  let AsmString = !strconcat(opcodestr, "\t$rs1, $block");
}

class VBX_PSEUDO_NOP<string opcodestr> : Pseudo<(outs), (ins), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = opcodestr;
}

class VBX_PSEUDOSELECT_RRR<string opcodestr> : Pseudo<(outs VBXTR:$rd), (ins VBXTR:$rs1, VBXTR:$rs2, VBXTR:$sel), [], opcodestr> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let AsmString = !strconcat(opcodestr, "\t$rd, $rs1, $rs2, $sel");
}

//===----------------------------------------------------------------------===//
// Pseudo LinxV5 Instructions
//===----------------------------------------------------------------------===//
def PseudoVBXADD : VBX_PSEUDO_RR<"vbx_add">;
def PseudoVBXSUB : VBX_PSEUDO_RR<"vbx_sub">;
def PseudoVBXAND : VBX_PSEUDO_RR<"vbx_and">;
def PseudoVBXOR : VBX_PSEUDO_RR<"vbx_or">;
def PseudoVBXXOR : VBX_PSEUDO_RR<"vbx_xor">;
def PseudoVBXSRL : VBX_PSEUDO_RR<"vbx_srl">;
def PseudoVBXSRA : VBX_PSEUDO_RR<"vbx_sra">;
def PseudoVBXSLL : VBX_PSEUDO_RR<"vbx_sll">;

def PseudoVBXFADD_H : VBX_PSEUDO_RR<"vbx_fadd_h">;
def PseudoVBXFSUB_H : VBX_PSEUDO_RR<"vbx_fsub_h">;
def PseudoVBXFMUL_H : VBX_PSEUDO_RR<"vbx_fmul_h">;
def PseudoVBXFDIV_H : VBX_PSEUDO_RR<"vbx_fdiv_h">;
def PseudoVBXFABS_H : VBX_PSEUDO_RR<"vbx_fabs_h">;
def PseudoVBXFSQRT_H : VBX_PSEUDO_RR<"vbx_fsqrt_h">;

def PseudoVBXFADD_S : VBX_PSEUDO_RR<"vbx_fadd_s">;
def PseudoVBXFSUB_S : VBX_PSEUDO_RR<"vbx_fsub_s">;
def PseudoVBXFMUL_S : VBX_PSEUDO_RR<"vbx_fmul_s">;
def PseudoVBXFDIV_S : VBX_PSEUDO_RR<"vbx_fdiv_s">;
def PseudoVBXFABS_S : VBX_PSEUDO_RR<"vbx_fabs_s">;
def PseudoVBXFSQRT_S : VBX_PSEUDO_RR<"vbx_fsqrt_s">;

def PseudoVBXFADD_D : VBX_PSEUDO_RR<"vbx_fadd_d">;
def PseudoVBXFSUB_D : VBX_PSEUDO_RR<"vbx_fsub_d">;
def PseudoVBXFMUL_D : VBX_PSEUDO_RR<"vbx_fmul_d">;
def PseudoVBXFDIV_D : VBX_PSEUDO_RR<"vbx_fdiv_d">;
def PseudoVBXFABS_D : VBX_PSEUDO_RR<"vbx_fabs_d">;
def PseudoVBXFSQRT_D : VBX_PSEUDO_RR<"vbx_fsqrt_d">;

def PseudoVBXFEQ_H : VBX_PSEUDO_RR<"vbx_feq_h">;
def PseudoVBXFLT_H : VBX_PSEUDO_RR<"vbx_flt_h">;
def PseudoVBXFLE_H : VBX_PSEUDO_RR<"vbx_fle_h">;

def PseudoVBXFEQ_S : VBX_PSEUDO_RR<"vbx_feq_s">;
def PseudoVBXFLT_S : VBX_PSEUDO_RR<"vbx_flt_s">;
def PseudoVBXFLE_S : VBX_PSEUDO_RR<"vbx_fle_s">;
def PseudoVBXFGE_S : VBX_PSEUDO_RR<"vbx_fge_fs">;

def PseudoVBXFEQ_D : VBX_PSEUDO_RR<"vbx_feq_d">;
def PseudoVBXFLT_D : VBX_PSEUDO_RR<"vbx_flt_d">;
def PseudoVBXFLE_D : VBX_PSEUDO_RR<"vbx_fle_d">;
def PseudoVBXFGE_D : VBX_PSEUDO_RR<"vbx_fge_fd">;

def PseudoVBXFCVT_SL_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_sl_h">;
def PseudoVBXFCVT_SL_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_sl_s">;
def PseudoVBXFCVT_SL_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_sl_d">;
def PseudoVBXFCVT_UL_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_ul_h">;
def PseudoVBXFCVT_UL_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_ul_s">;
def PseudoVBXFCVT_UL_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_ul_d">;
def PseudoVBXFCVT_SW_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_sw_h">;
def PseudoVBXFCVT_SW_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_sw_s">;
def PseudoVBXFCVT_SW_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_sw_d">;
def PseudoVBXFCVT_UW_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_uw_h">;
def PseudoVBXFCVT_UW_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_uw_s">;
def PseudoVBXFCVT_UW_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_uw_d">;

def PseudoVBXFCVT_H_SL : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_sl">;
def PseudoVBXFCVT_S_SL : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_sl">;
def PseudoVBXFCVT_D_SL : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_sl">;
def PseudoVBXFCVT_H_UL : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_ul">;
def PseudoVBXFCVT_S_UL : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_ul">;
def PseudoVBXFCVT_D_UL : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_ul">;
def PseudoVBXFCVT_H_SW : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_sw">;
def PseudoVBXFCVT_S_SW : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_sw">;
def PseudoVBXFCVT_D_SW : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_sw">;
def PseudoVBXFCVT_H_UW : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_uw">;
def PseudoVBXFCVT_S_UW : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_uw">;
def PseudoVBXFCVT_D_UW : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_uw">;

def PseudoVBXFCVT_H_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_s">;
def PseudoVBXFCVT_H_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_h_d">;
def PseudoVBXFCVT_S_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_h">;
def PseudoVBXFCVT_S_D : VBX_PSEUDOFCVT_R<"vbx_fcvt_s_d">;
def PseudoVBXFCVT_D_H : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_h">;
def PseudoVBXFCVT_D_S : VBX_PSEUDOFCVT_R<"vbx_fcvt_d_s">;

def PseudoVBXADDW : VBX_PSEUDO_RR<"vbx_addw">;
def PseudoVBXSUBW : VBX_PSEUDO_RR<"vbx_subw">;
def PseudoVBXANDW : VBX_PSEUDO_RR<"vbx_andw">;
def PseudoVBXORW : VBX_PSEUDO_RR<"vbx_orw">;
def PseudoVBXXORW : VBX_PSEUDO_RR<"vbx_xorw">;
def PseudoVBXSRLW : VBX_PSEUDO_RR<"vbx_srlw">;
def PseudoVBXSRAW : VBX_PSEUDO_RR<"vbx_sraw">;
def PseudoVBXSLLW : VBX_PSEUDO_RR<"vbx_sllw">;

def PseudoVBXMUL : VBX_PSEUDO_RR<"vbx_mul">;
def PseudoVBXMULH : VBX_PSEUDO_RR<"vbx_mulh">;
def PseudoVBXMULHU : VBX_PSEUDO_RR<"vbx_mulhu">;
def PseudoVBXMULHSU : VBX_PSEUDO_RR<"vbx_mulhsu">;
def PseudoVBXDIV : VBX_PSEUDO_RR<"vbx_div">;
def PseudoVBXDIVU : VBX_PSEUDO_RR<"vbx_divu">;
def PseudoVBXREM : VBX_PSEUDO_RR<"vbx_rem">;
def PseudoVBXREMU : VBX_PSEUDO_RR<"vbx_remu">;

def PseudoVBXMULW : VBX_PSEUDO_RR<"vbx_mulw">;
def PseudoVBXDIVW : VBX_PSEUDO_RR<"vbx_divw">;
def PseudoVBXDIVUW : VBX_PSEUDO_RR<"vbx_divuw">;
def PseudoVBXREMW : VBX_PSEUDO_RR<"vbx_remw">;
def PseudoVBXREMUW : VBX_PSEUDO_RR<"vbx_remuw">;

def PseudoVBXCMP_EQ : VBX_PSEUDO_RR<"vbx_cmp_eq">;
def PseudoVBXCMP_NE : VBX_PSEUDO_RR<"vbx_cmp_ne">;
def PseudoVBXCMP_LT : VBX_PSEUDO_RR<"vbx_cmp_lt">;
def PseudoVBXCMP_LTU : VBX_PSEUDO_RR<"vbx_cmp_ltu">;
def PseudoVBXCMP_GE : VBX_PSEUDO_RR<"vbx_cmp_ge">;
def PseudoVBXCMP_GEU : VBX_PSEUDO_RR<"vbx_cmp_geu">;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isCall = 0, isTerminator = 0 in {
  def PseudoVBXCONST : Pseudo<(outs VBXTR:$rd), (ins linxsimm64_symbol:$imm64_symbol), []>;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, isCall = 0, isTerminator = 0 in {
  def PseudoVBXSYSGET : Pseudo<(outs VBXTR:$rd), (ins uimm16:$rs), []>;
}

def PseudoVBXSB : VBX_PSEUDOSTORE_RR<"vbx_sb">;
def PseudoVBXSH : VBX_PSEUDOSTORE_RR<"vbx_sh">;
def PseudoVBXSW : VBX_PSEUDOSTORE_RR<"vbx_sw">;
def PseudoVBXSD : VBX_PSEUDOSTORE_RR<"vbx_sd">;

def PseudoVBXLB : VBX_PSEUDOLOAD_R<"vbx_lb">;
def PseudoVBXLBU : VBX_PSEUDOLOAD_R<"vbx_lbu">;
def PseudoVBXLH : VBX_PSEUDOLOAD_R<"vbx_lh">;
def PseudoVBXLHU : VBX_PSEUDOLOAD_R<"vbx_lhu">;
def PseudoVBXLW : VBX_PSEUDOLOAD_R<"vbx_lw">;

def PseudoVBXLWU : VBX_PSEUDOLOAD_R<"vbx_lwu">;
def PseudoVBXLD : VBX_PSEUDOLOAD_R<"vbx_ld">;

// branch and setbpc insts
def PseudoVBXBEQ : VBX_PSEUDOBRCOND_RRs<"vbx_beq">;
def PseudoVBXBNE : VBX_PSEUDOBRCOND_RRs<"vbx_bne">;
def PseudoVBXBLT : VBX_PSEUDOBRCOND_RRs<"vbx_blt">;
def PseudoVBXBLTU : VBX_PSEUDOBRCOND_RRs<"vbx_bltu">;
def PseudoVBXBGE : VBX_PSEUDOBRCOND_RRs<"vbx_bge">;
def PseudoVBXBGEU : VBX_PSEUDOBRCOND_RRs<"vbx_bgeu">;

def PseudoBRIND : Pseudo<(outs ), (ins MCSrc:$rs1), []> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
  let isBarrier = 1;
  let isBranch = 1;
  let isIndirectBranch = 1;
  let isTerminator = 1;
}

def PseudoVBXSETBPC_EQ : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_eq">;
def PseudoVBXSETBPC_NE : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_ne">;
def PseudoVBXSETBPC_LT : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_lt">;
def PseudoVBXSETBPC_LTU : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_ltu">;
def PseudoVBXSETBPC_GE : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_ge">;
def PseudoVBXSETBPC_GEU : VBX_PSEUDOSETCCOND_RR<"vbx_setbpc_geu">;

def PseudoVBXSETBPC : VBX_PSEUDOSETC_R<"vbx_setbpc">;
def PseudoVBXSETBPC_TRAP : VBX_PSEUDOSETC_R<"vbx_setbpc_trap">;

def PseudoVBXCOPY : VBX_PSEUDOUNARY_R<"vbx_copy">;
def PseudoVBXNOT : VBX_PSEUDOUNARY_R<"vbx_not">;

def PseudoVBXJ : VBX_PSEUDOJUMP_s<"vbx_j">;
def PseudoVBXJR : VBX_PSEUDOJUMP_R<"vbx_jr">;
def PseudoVBXCONDJR : VBX_PSEUDOCONDJUMP_R<"vbx_bcond">;
def PseudoVBXCONDJ :  VBX_PSEUDOCONDJUMP_s<"vbx_bcondi">;

def PseudoVBXNOP : VBX_PSEUDO_NOP<"vbx_nop">;
def PseudoVBXMBBGroupEndPoint : VBX_PSEUDO_NOP<"vbx_end_point">;

def PseudoVBXBNEXT_FALL : VBX_PSEUDO_NOP<"bnext.fall">;
def PseudoVBXBNEXT_DIRECT : VBX_PSEUDOJUMP_s<"bnext.direct">;
def PseudoVBXBNEXT_CALL : VBX_PSEUDOJUMP_s<"bnext.call">;
def PseudoVBXBNEXT_COND : VBX_PSEUDOJUMP_s<"bnext.cond">;
def PseudoVBXBNEXT_IND : VBX_PSEUDO_NOP<"bnext.ind">;
def PseudoVBXBNEXT_INDCALL : VBX_PSEUDO_NOP<"bnext.indcall">;
def PseudoVBXBNEXT_RET : VBX_PSEUDO_NOP<"bnext.ret">;
def PseudoVBXBNEXT_CONCAT : VBX_PSEUDO_NOP<"bnext.concat">;

def PseudoVBXSELECT : VBX_PSEUDOSELECT_RRR<"vbx_select">;

// These are pseudo inst use for vbx lowering
let hasSideEffects = 0, mayLoad = 0, mayStore = 0, isCall = 0, isTerminator = 0 in {
def PseudoVBXADDR_HI : Pseudo<(outs VBXTR:$rd1), (ins linxbaresymbol:$label, linxbaresymbol:$addrhi), []>;
def PseudoVBXADDR_LO : Pseudo<(outs VBXTR:$rd1), (ins VBXTR:$src1, linxbaresymbol:$addrlo), []>;
}
let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in
def PseudoLABEL : Pseudo<(outs), (ins linxbaresymbol:$label), []> {
  let AsmString = "$label" # ":\n\t";
}

/// Generic pattern classes
class PatTrTr<SDPatternOperator OpNode, LinxV5Inst Inst>
    : Pat<(OpNode i64:$rs1, i64:$rs2), (Inst i64:$rs1, i64:$rs2)>;

def linxSRLIWPat : PatFrag<(ops node:$A, node:$B),
                       (srl (and node:$A, imm), node:$B), [{
  return MatchSRLIW(N);
}]>;

/// Simple arithmetic operations

let Predicates = [isScalar, useVBXISel] in {
def : PatTrTr<add, PseudoVBXADD>;
def : PatTrTr<sub, PseudoVBXSUB>;
def : PatTrTr<and, PseudoVBXAND>;
def : PatTrTr<or, PseudoVBXOR>;
def : PatTrTr<xor, PseudoVBXXOR>;
} // end Predicates = isScalar

// Match both a plain shift and one where the shift amount is masked (this is
// typically introduced when the legalizer promotes the shift amount and
// zero-extends it). For LinxV5, the mask is unnecessary as shifts in the base
// ISA only read the least significant 5 bits (RV32I) or 6 bits (RV64I).
class linxshiftop<SDPatternOperator operator>
    : PatFrags<(ops node:$val, node:$count),
               [(operator node:$val, node:$count),
                (operator node:$val, (and node:$count, linximmbottomxlenset))]>;
class linxshiftopw<SDPatternOperator operator>
    : PatFrags<(ops node:$val, node:$count),
               [(operator node:$val, node:$count),
                (operator node:$val, (and node:$count, (XLenVT 31)))]>;

let Predicates = [isScalar, useVBXISel] in {
def : PatTrTr<linxshiftop<shl>, PseudoVBXSLL>;
def : PatTrTr<linxshiftop<srl>, PseudoVBXSRL>;
def : PatTrTr<linxshiftop<sra>, PseudoVBXSRA>;

def : Pat<(sext_inreg (add i64:$rs1, i64:$rs2), i32),
          (PseudoVBXADDW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (sub i64:$rs1, i64:$rs2), i32),
          (PseudoVBXSUBW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (and i64:$rs1, i64:$rs2), i32),
          (PseudoVBXANDW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (or i64:$rs1, i64:$rs2), i32),
          (PseudoVBXORW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (xor i64:$rs1, i64:$rs2), i32),
          (PseudoVBXXORW i64:$rs1, i64:$rs2)>;
def : PatTrTr<linxshiftopw<linxv5_srlw>, PseudoVBXSRLW>;
def : PatTrTr<linxshiftopw<linxv5_sraw>, PseudoVBXSRAW>;
def : PatTrTr<linxshiftopw<linxv5_sllw>, PseudoVBXSLLW>;

def : PatTrTr<seteq, PseudoVBXCMP_EQ>;
def : PatTrTr<setne, PseudoVBXCMP_NE>;
def : PatTrTr<setlt, PseudoVBXCMP_LT>;
def : PatTrTr<setult, PseudoVBXCMP_LTU>;
def : PatTrTr<setge, PseudoVBXCMP_GE>;
def : PatTrTr<setuge, PseudoVBXCMP_GEU>;
def : Pat<(setugt i64:$rs1, i64:$rs2), (PseudoVBXCMP_LTU i64:$rs2, i64:$rs1)>;
def : Pat<(setule i64:$rs1, i64:$rs2), (PseudoVBXCMP_GEU i64:$rs2, i64:$rs1)>;
def : Pat<(setgt i64:$rs1, i64:$rs2), (PseudoVBXCMP_LT i64:$rs2, i64:$rs1)>;
def : Pat<(setle i64:$rs1, i64:$rs2), (PseudoVBXCMP_GE i64:$rs2, i64:$rs1)>;

let Predicates = [isScalar, useVBXISel] in {
def : PatTrTr<mul, PseudoVBXMUL>;
def : PatTrTr<mulhs, PseudoVBXMULH>;
def : PatTrTr<mulhu, PseudoVBXMULHU>;
// No ISDOpcode for mulhsu
def : PatTrTr<sdiv, PseudoVBXDIV>;
def : PatTrTr<udiv, PseudoVBXDIVU>;
def : PatTrTr<srem, PseudoVBXREM>;
def : PatTrTr<urem, PseudoVBXREMU>;
}

let Predicates = [isScalar, useVBXISel] in {
def : Pat<(sext_inreg (mul i64:$rs1, i64:$rs2), i32),
          (PseudoVBXMULW i64:$rs1, i64:$rs2)>;

def : PatTrTr<linxv5_divw, PseudoVBXDIVW>;
def : PatTrTr<linxv5_divuw, PseudoVBXDIVUW>;
def : PatTrTr<linxv5_remuw, PseudoVBXREMUW>;

// Handle the specific cases where using PseudoVBXDIVU/PseudoVBXREMU would be correct and result
// in fewer instructions than emitting PseudoVBXDIVUW/PseudoVBXREMUW then zero-extending the
// result.
def : Pat<(and (linxv5_divuw (assertzexti32 i64:$rs1),
                            (assertzexti32 i64:$rs2)), 0xffffffff),
          (PseudoVBXDIVU i64:$rs1, i64:$rs2)>;
def : Pat<(and (linxv5_remuw (assertzexti32 i64:$rs1),
                            (assertzexti32 i64:$rs2)), 0xffffffff),
          (PseudoVBXREMU i64:$rs1, i64:$rs2)>;

// Although the sexti32 operands may not have originated from an i32 srem,
// this pattern is safe as it is impossible for two sign extended inputs to
// produce a result where res[63:32]=0 and res[31]=1.
def : Pat<(srem (sexti32 i64:$rs1), (sexti32 i64:$rs2)),
          (PseudoVBXREMW i64:$rs1, i64:$rs2)>;
def : Pat<(sext_inreg (srem (sexti32 i64:$rs1),
                            (sexti32 i64:$rs2)), i32),
          (PseudoVBXREMW i64:$rs1, i64:$rs2)>;
}

/// sext and zext

def : Pat<(sext_inreg i64:$rs1, i32), (PseudoVBXADDW i64:$rs1, (PseudoVBXCONST 0))>;

} // end Predicates = isScalar

/// Stores
def v3_store : PatFrag<(ops node:$val, node:$ptr),
                    (unindexedstore node:$val, node:$ptr)> {
  let IsStore = true;
  let MemoryVT = i64;
}
def v3_truncstorei8 : PatFrag<(ops node:$val, node:$ptr),
                           (truncstore node:$val, node:$ptr)> {
  let IsStore = true;
  let MemoryVT = i8;
}
def v3_truncstorei16 : PatFrag<(ops node:$val, node:$ptr),
                            (truncstore node:$val, node:$ptr)> {
  let IsStore = true;
  let MemoryVT = i16;
}
def v3_truncstorei32 : PatFrag<(ops node:$val, node:$ptr),
                            (truncstore node:$val, node:$ptr)> {
  let IsStore = true;
  let MemoryVT = i32;
}

multiclass linxStPat<PatFrag StoreOp, LinxV5Inst Inst, ValueType vt = XLenVT> {
  def : Pat<(StoreOp (vt i64:$rs2), i64:$rs1), (Inst vt:$rs2, i64:$rs1)>;
  def : Pat<(StoreOp (vt i64:$rs2), AddrFI:$rs1), (Inst vt:$rs2, AddrFI:$rs1)>;
}

let Predicates = [isScalar, useVBXISel] in {
defm : linxStPat<v3_truncstorei8, PseudoVBXSB>;
defm : linxStPat<v3_truncstorei16, PseudoVBXSH>;
defm : linxStPat<v3_truncstorei32, PseudoVBXSW>;
defm : linxStPat<v3_store, PseudoVBXSD>;
} // end Predicates = isScalar

/// Loads

multiclass linxLdPat<PatFrag LoadOp, LinxV5Inst Inst, ValueType vt = XLenVT> {
  def : Pat<(vt (LoadOp i64:$rs1)), (Inst i64:$rs1)>;
  def : Pat<(vt (LoadOp AddrFI:$rs1)), (Inst AddrFI:$rs1)>;
}

let Predicates = [isScalar, useVBXISel] in {
defm : linxLdPat<sextloadi8, PseudoVBXLB>;
defm : linxLdPat<extloadi8, PseudoVBXLB>;
defm : linxLdPat<sextloadi16, PseudoVBXLH>;
defm : linxLdPat<extloadi16, PseudoVBXLH>;
defm : linxLdPat<sextloadi32, PseudoVBXLW>;
defm : linxLdPat<extloadi32, PseudoVBXLW>;
defm : linxLdPat<zextloadi8, PseudoVBXLBU>;
defm : linxLdPat<zextloadi16, PseudoVBXLHU>;
defm : linxLdPat<zextloadi32, PseudoVBXLWU>;
defm : linxLdPat<load, PseudoVBXLD>;
} // end Predicates = isScalar

/// Branches and jumps

// Match `(brcond (CondOp ..), ..)` and lower to the appropriate LinxV5 branch
// instruction.
class linxBccPat<PatFrag CondOp, Pseudo Inst>
    : Pat<(brcond (XLenVT (CondOp i64:$rs1, i64:$rs2)), bb:$block),
          (Inst i64:$rs1, i64:$rs2, linxbareblock:$block)>;

let Predicates = [isScalar, useVBXISel] in {
def : linxBccPat<seteq, PseudoVBXBEQ>;
def : linxBccPat<setne, PseudoVBXBNE>;
def : linxBccPat<setlt, PseudoVBXBLT>;
def : linxBccPat<setge, PseudoVBXBGE>;
def : linxBccPat<setult, PseudoVBXBLTU>;
def : linxBccPat<setuge, PseudoVBXBGEU>;
} // end Predicates = isScalar

let Predicates = [isScalar, useVBXISel] in {
// Extra patterns are needed for a brcond without a setcc (i.e. where the
// condition was calculated elsewhere).
def : Pat<(brcond i64:$cond, bb:$block), (PseudoVBXBNE i64:$cond, (PseudoVBXCONST 0), bb:$block)>;
// In this pattern, the `(xor $cond, 1)` functions like (boolean) `not`, as the
// `brcond` only uses the lowest bit.
def : Pat<(brcond (XLenVT (xor i64:$cond, 1)), bb:$block),
          (PseudoVBXBEQ i64:$cond, (PseudoVBXCONST 0), bb:$block)>;

class linxBccSwapPat<PatFrag CondOp, Pseudo InstBcc>
    : Pat<(brcond (XLenVT (CondOp i64:$rs1, i64:$rs2)), bb:$block),
          (InstBcc i64:$rs2, i64:$rs1, bb:$block)>;

// Condition codes that don't have matching LinxV5 branch instructions, but
// are trivially supported by swapping the two input operands
def : linxBccSwapPat<setgt, PseudoVBXBLT>;
def : linxBccSwapPat<setle, PseudoVBXBGE>;
def : linxBccSwapPat<setugt, PseudoVBXBLTU>;
def : linxBccSwapPat<setule, PseudoVBXBGEU>;

def : Pat<(brind i64:$rs1), (PseudoBRIND i64:$rs1)>;
} // end Predicates = isScalar

/// RV64 patterns

//===---------------- Here are some define may confict with V3 and V4 ---------===//

// PseudoCALL is a pseudo instruction which will eventually expand to auipc
// and jalr while encoding. This is desirable, as an auipc+jalr pair with
// R_Linx_CALL and R_Linx_RELAX relocations can be be relaxed by the linker
// if the offset fits in a signed 21-bit immediate.
// Define AsmString to print "call" when compile with -S flag.
// Define isCodeGenOnly = 0 to support parsing assembly "call" instruction.
let isCall = 1, isTerminator = 1, Defs = [R10], isCodeGenOnly = 0 in
def PseudoCALL : Pseudo<(outs), (ins call_symbol:$func), []> {
  let AsmString = "call\t$func";
}

// PseudoTAIL is a pseudo instruction similar to PseudoCALL and will eventually
// expand to auipc and jalr while encoding.
// Define AsmString to print "tail" when compile with -S flag.
let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1, Uses = [R1],
    isCodeGenOnly = 0 in
def PseudoTAIL : Pseudo<(outs), (ins call_symbol:$dst), []> {
  let AsmString = "tail\t$dst";
}

// Pessimistically assume the stack pointer will be clobbered
let Defs = [R1], Uses = [R1] in {
def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                              [(callseq_start timm:$amt1, timm:$amt2)]>;
def ADJCALLSTACKUP   : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                              [(callseq_end timm:$amt1, timm:$amt2)]>;
} // Defs = [R1], Uses = [R1]
