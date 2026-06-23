//===-- LinxV5InstrFormats.td - LinxV5 Instruction Formats ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Format specifies the encoding used by the instruction. This is used by
// LinxV5MCCodeEmitter to determine which form of fixup to use. These
// definitions must be kept in-sync with LinxV5BaseInfo.h.
class InstFormat<bits<8> val> {
  bits<8> Value = val;
}
def InstFormatDefault               : InstFormat<0>;
def InstFormat_ADDI                 : InstFormat<1>;
def InstFormat_StoreI_UnScaled      : InstFormat<2>;
def InstFormat_LoadI_UnScaled       : InstFormat<3>;
def InstFormat_SIMT_BRANCH          : InstFormat<4>;
def InstFormat_SIMT_JUMP            : InstFormat<5>;
def InstFormat_REDUCE               : InstFormat<6>;
def InstFormat_Load_Symbol          : InstFormat<7>;
def InstFormat_Store_Symbol         : InstFormat<8>;
def InstFormat_BRANCH               : InstFormat<9>;
def InstFormat_Load_Symbol_TARGET_42  : InstFormat<10>;
def InstFormat_Store_Symbol_TARGET_42 : InstFormat<11>;
def InstFormat_BRANCH_22               : InstFormat<12>;
def InstFormat_Load_Symbol_TARGET_29  : InstFormat<13>;
def InstFormat_Store_Symbol_TARGET_29 : InstFormat<14>;
def InstFormatFP                      : InstFormat<15>;
def InstFormat_STACK_SIZE            : InstFormat<16>;

class InstFormatBSTARTBase<int val> : InstFormat<!add(32, val)>;
def InstFormat_BSTART_WITH_TARGET_12: InstFormatBSTARTBase<0>;
def InstFormat_BSTART_WITHOUT_TARGET_16 : InstFormatBSTARTBase<1>;
def InstFormat_BSTART_WITH_TARGET_17: InstFormatBSTARTBase<2>;
def InstFormat_BSTART_WITH_TARGET_25: InstFormatBSTARTBase<3>;
def InstFormat_BSTART_WITH_TARGET_29: InstFormatBSTARTBase<4>;
def InstFormat_BSTART_WITH_TARGET_42: InstFormatBSTARTBase<5>;
def InstFormat_BSTART_WITHOUT_TARGET_32: InstFormatBSTARTBase<6>;

def InstFormatBlockModifier : InstFormat<64>;

//=== ------------------------------ PE MASK --------------------------------
// There are some instructions in the private encoding space are same in
// different block types. Like `div` in std and fp block. We need to accurately
// describe the encoding namespace of these instructions to promote the block
// type of every group of instructions.
// @val 8bits PE Mask, the maximun number of block types is 8.
// @DecoderNS DecoderNamespace list, every DecoderNamespace will include this
// instruction.
// DecoderNamespace:
//  "" : Shared encoding space. now simt block is the only 64bits instruction.
//       So simt block also use this space.
//  "STD" : std private space
//  "FP"  : fp private space
class PEType<bits<8> val, list<string> DecoderNS> {
  bits<8> Value = val;
  list<string> DecoderNamespaces = DecoderNS;
}
def PET_ALL     : PEType<0xff, [""]>;
def PET_STD     : PEType<0b00000001, [""]>;
def PET_FP      : PEType<0b00000010, [""]>;
def PET_SYS     : PEType<0b00000100, [""]>;
def PET_SIMT    : PEType<0b00001000, [""]>;

def PET_STDFP   : PEType<0b00000011, ["", ""]>;

//=== --------------------------- Tile Op Property -----------------------------
class TileOpProperty<bits<4> val, list<string> DecoderNS> {
  bits<4> Value = val;
  list<string> DecoderNamespaces = DecoderNS;
}

def TileOp_Not   :  TileOpProperty<1, [""]>;
def TileOp_VEC   :  TileOpProperty<2, [""]>;
def TileOp_MTC   :  TileOpProperty<3, [""]>;
def TileOp_CUBE  :  TileOpProperty<4, [""]>;
def TileOp_TEPL  :  TileOpProperty<5, [""]>;

//===----------------------------------------------------------------------===//
// LinxV5 specific DAG Nodes.
//===----------------------------------------------------------------------===//

// Target-independent type requirements, but with target-specific formats.
def SDT_CallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                       SDTCisVT<1, i32>]>;
def SDT_CallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                     SDTCisVT<1, i32>]>;

// Target-dependent type requirements.
def SDT_LinxV5SelectCC : SDTypeProfile<1, 5, [SDTCisSameAs<1, 2>,
                                             SDTCisSameAs<0, 4>,
                                             SDTCisSameAs<4, 5>]>;

def SDT_CVT : SDTypeProfile<1, 1, []>;

// Target-independent nodes, but with target-specific formats.
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_CallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_CallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def linxv5_ret_flag  : SDNode<"LinxV5ISD::RET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def linxv5_uret_flag : SDNode<"LinxV5ISD::URET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def linxv5_sret_flag : SDNode<"LinxV5ISD::SRET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def linxv5_mret_flag : SDNode<"LinxV5ISD::MRET_FLAG", SDTNone,
                             [SDNPHasChain, SDNPOptInGlue]>;
def linxv5_selectcc  : SDNode<"LinxV5ISD::SELECT_CC", SDT_LinxV5SelectCC>;
def linxv5_sllw      : SDNode<"LinxV5ISD::SLLW", SDTIntShiftOp>;
def linxv5_sraw      : SDNode<"LinxV5ISD::SRAW", SDTIntShiftOp>;
def linxv5_srlw      : SDNode<"LinxV5ISD::SRLW", SDTIntShiftOp>;
def linxv5_divw      : SDNode<"LinxV5ISD::DIVW",  SDTIntBinOp>;
def linxv5_divuw     : SDNode<"LinxV5ISD::DIVUW", SDTIntBinOp>;
def linxv5_remuw     : SDNode<"LinxV5ISD::REMUW", SDTIntBinOp>;
def linxv5_addlo     : SDNode<"LinxV5ISD::ADDlo", SDTIntBinOp, []>;

def linxv5_fcvts     : SDNode<"LinxV5ISD::FCVTS", SDT_CVT>;
def linxv5_fcvtu     : SDNode<"LinxV5ISD::FCVTU", SDT_CVT>;
def linxv5_bitcast   : SDNode<"LinxV5ISD::BITCAST", SDT_CVT>;

def SDT_LinxV5MergePredication : SDTypeProfile<0, 1, []>;
def linxv5_merge_pred: SDNode<"LinxV5ISD::MERGE_PREDICATION", SDT_LinxV5MergePredication, [SDNPHasChain]>;

def SDT_LinxV5CopyP : SDTypeProfile<1, 0, []>;
def SDT_LinxV5Copy2P : SDTypeProfile<0, 1, []>;
def linxv5_copy_p: SDNode<"LinxV5ISD::CopyP", SDT_LinxV5CopyP, [SDNPHasChain]>;
def linxv5_copy2_p: SDNode<"LinxV5ISD::Copy2P", SDT_LinxV5Copy2P, [SDNPHasChain]>;
def linxv5_copy2_p_term: SDNode<"LinxV5ISD::Copy2PTerm", SDT_LinxV5Copy2P, [SDNPHasChain]>;

def SDT_LinxV5MergeCF : SDTypeProfile<1, 2, []>;
def linxv5_merge_cf: SDNode<"LinxV5ISD::MERGE_CF", SDT_LinxV5MergeCF, [SDNPHasChain]>;

def SDT_LinxV5ImplicitDef : SDTypeProfile<1, 0, []>;
def linxv5_implicit_def: SDNode<"LinxV5ISD::IMPLICIT_DEF", SDT_LinxV5ImplicitDef, []>;

class TriOpFrag<dag res> : PatFrag<(ops node:$LHS, node:$MHS, node:$RHS), res>;

class worth_biopw<SDPatternOperator N>
    : PatFrag<(ops node:$lhs, node:$rhs),
              (sext_inreg (N node:$lhs, node:$rhs), i32), [{
  return isWorthOpW(Node);
}]>;

class worth_sw_ext_base<SDPatternOperator N>
    : PatFrag<(ops node:$n),
              (N node:$n), [{
  return !isWorthOpW(Node);
}]>;

def worth_shlext : PatFrag<(ops node:$base, node:$shamt), (shl node:$base, node:$shamt), [{
  return isWorthShlext(Node);
}]>;

//def PseudoCmp : PatFrag<(ops node:$base, node:$shamt), (shl node:$base, node:$shamt), [{
//  return Node->hasUniform();
//}]>;

class biopw<SDPatternOperator Op> :
    PatFrag<(ops node:$lhs, node:$rhs), (sext_inreg (Op node:$lhs, node:$rhs), i32)>;

def uw_ext : PatFrag<(ops node:$in), (and node:$in, 0xffffffff)>;
def sw_ext : PatFrag<(ops node:$in), (sext_inreg node:$in, i32)>;
def worth_sw_ext : worth_sw_ext_base<sw_ext>;
def uh_ext : PatFrag<(ops node:$in), (and node:$in, 0xffff)>;
def sh_ext : PatFrag<(ops node:$in), (sext_inreg node:$in, i16)>;
def ub_ext : PatFrag<(ops node:$in), (and node:$in, 0xff)>;
def sb_ext : PatFrag<(ops node:$in), (sext_inreg node:$in, i8)>;
def neg_ext : PatFrag<(ops node:$in), (ineg node:$in)>;
def not_ext : PatFrag<(ops node:$in), (not node:$in)>;

class LinxV5InstOpcode<string name, bits<3> val> {
  string Name = name;
  bits<3> Value = val;
}

def OPC_Arithmetic_Operation_64           : LinxV5InstOpcode<"OPC_Arithmetic_Operation_64",         0b000>;
def OPC_Arithmetic_Operation_64_I         : LinxV5InstOpcode<"OPC_Arithmetic_Operation_64_I",       0b001>;
def OPC_Arithmetic_Operation_32           : LinxV5InstOpcode<"OPC_Arithmetic_Operation_32",         0b010>;
def OPC_Arithmetic_Operation_32_I         : LinxV5InstOpcode<"OPC_Arithmetic_Operation_32_I",       0b011>;
def OPC_Compare_Instruction               : LinxV5InstOpcode<"OPC_Compare_Instruction",             0b100>;
def OPC_Compare_Instruction_I             : LinxV5InstOpcode<"OPC_Compare_Instruction_I",           0b101>;
def OPC_MultiCycle_MUL_Instruction        : LinxV5InstOpcode<"OPC_MultiCycle_MUL_Instruction",      0b100>;
def OPC_MultiCycle_DIV_Instruction        : LinxV5InstOpcode<"OPC_MultiCycle_DIV_Instruction",      0b101>;
def OPC_Load_Register_Offset              : LinxV5InstOpcode<"OPC_Load_Register_Offset",            0b000>;
def OPC_Load_Immediate_Offset             : LinxV5InstOpcode<"OPC_Load_Immediate_Offset",           0b001>;
def OPC_Load_Unscaled                     : LinxV5InstOpcode<"OPC_Load_Unscaled",                   0b010>;
def OPC_SetCommit                         : LinxV5InstOpcode<"OPC_SetCommit",                       0b110>;
def OPC_SetCommit_I                       : LinxV5InstOpcode<"OPC_SetCommit_I",                     0b111>;
def OPC_Bit_Operation                     : LinxV5InstOpcode<"OPC_Bit_Operation",                   0b110>;
def OPC_3SrcInt_Operation                 : LinxV5InstOpcode<"OPC_3SrcInt_Operation",               0b111>;
def OPC_load_symbol                       : LinxV5InstOpcode<"OPC_load_symbol",                     0b011>;
def OPC_Store                             : LinxV5InstOpcode<"OPC_Store",                           0b100>;
def OPC_Store_I                           : LinxV5InstOpcode<"OPC_Store_I",                         0b101>;
def OPC_store_symbol                      : LinxV5InstOpcode<"OPC_store_symbol",                    0b110>;
def OPC_SSR_Access                        : LinxV5InstOpcode<"OPC_SSR_Access",                      0b011>;
def OPC_MAX                               : LinxV5InstOpcode<"OPC_MAX",                             0b101>;
def OPC_Execution_Control                 : LinxV5InstOpcode<"OPC_Execution_Control",               0b010>;
def OPC_Convert                           : LinxV5InstOpcode<"OPC_Convert",                         0b110>;
def OPC_FloatAlgorithm                    : LinxV5InstOpcode<"OPC_FloatAlgorithm",                  0b111>;
def OPC_2Src_Floating_Point               : LinxV5InstOpcode<"OPC_2Src_Floating_Point",             0b101>;
def OPC_3Src_Floating_Point               : LinxV5InstOpcode<"OPC_3Src_Floating_Point",             0b100>;
def OPC_Branch_Instruction                : LinxV5InstOpcode<"OPC_Branch_Instruction",              0b010>;

class LinxV5InstType<string name, bits<3> val> {
  string Name = name;
  bits<3> Value = val;
}

def Type_000                              : LinxV5InstType<"Type_000",                              0b000>;
def Type_001                              : LinxV5InstType<"Type_001",                              0b001>;
def Type_010                              : LinxV5InstType<"Type_010",                              0b010>;
def Type_011                              : LinxV5InstType<"Type_011",                              0b011>;
def Type_100                              : LinxV5InstType<"Type_100",                              0b100>;
def Type_101                              : LinxV5InstType<"Type_101",                              0b101>;
def Type_110                              : LinxV5InstType<"Type_110",                              0b110>;
def Type_111                              : LinxV5InstType<"Type_111",                              0b111>;

def uimm12 : Operand<iAny>, ImmLeaf<iAny, [{return isUInt<12>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<12>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<12>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<12>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM12";
  let OperandNamespace = "LinxV5Op";
}

def uimm8 : Operand<iAny>, ImmLeaf<iAny, [{return isUInt<8>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<8>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<8>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<8>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM8";
  let OperandNamespace = "LinxV5Op";
}

// store load symbol
def simm17 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<17, "">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<17>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<16, 0>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

def uimm24 : Operand<iAny>, ImmLeaf<iAny, [{return isUInt<24>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<24>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<24>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<24>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_UIMM24";
  let OperandNamespace = "LinxV5Op";
}

def NegXForm : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(-N->getSExtValue(), SDLoc(N), MVT::i64);
}]>;
def neg_uimm12 : ImmLeaf<iAny, [{return isUInt<12>(-Imm);}], NegXForm>;

// A 43-bit signed immediate where the least significant bit is zero: bits[42:1]
def simm43_lsb0_bnext : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<43, "Lsb0BNext">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<43>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<42, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 30-bit signed immediate where the least significant bit is zero: bits[29:1]
def simm30_lsb0_bnext : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<30, "Lsb0BNext">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<30>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<29, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 26-bit signed immediate where the least significant bit is zero: bits[25:1]
def simm26_lsb0_bnext : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<26, "Lsb0BNext">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<26>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<25, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 18-bit signed immediate where the least significant bit is zero: bits[17:1]
def simm18_lsb0_bnext : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<18, "Lsb0BNext">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<18>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<17, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 11-bit signed immediate where the least significant bit is zero: bits[10:1]
def simm11_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<11, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<11>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<10, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

def UIMM3Plus1AsmOperand : AsmOperandClass {
  let Name = "Uimm3Plus1";
  let ParserMethod = "parseImmediate";
  let RenderMethod = "addImmOperands";
}

// A 6-bit unsigned immediate where the least significant bit is zero: bits[5:1]
def uimm6_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = UImmAsmOperand<6, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeUImmOperandAndLsl1<6>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedUInt<5, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 33-bit unsigned immediate where the least significant bit is zero: bits[32:1]
def uimm33_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = UImmAsmOperand<33, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeUImmOperandAndLsl1<33>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedUInt<32, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 21-bit unsigned immediate where the least significant bit is zero: bits[20:1]
def uimm21_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = UImmAsmOperand<21, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeUImmOperandAndLsl1<21>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedUInt<20, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 13-bit signed immediate where the least significant bit is zero.
def simm13_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<13, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<13>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<12, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

// A 15-bit signed immediate where the least significant three bits are zero.
def simm15_lsl3 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<15, "Lsl3">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr3";
  let DecoderMethod = "decodeSImmOperandAndLsl3<15>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<12, 3>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

def simm23_lsb0 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<23, "Lsb0">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeSImmOperandAndLsl1<23>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<22, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}


def UIMM6Plus1AsmOperand : AsmOperandClass {
  let Name = "Uimm6Plus1";
  let RenderMethod = "addImmOperands";
}

// A 3-bit unsigned immediate represent N in bfi: bits[27:25]
def uimm3_plus1 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<3>(Imm - 1);}]> {
  let ParserMatchClass = UIMM3Plus1AsmOperand;
  let EncoderMethod = "getImmOpValueMinus1";
  let DecoderMethod = "decodeUImmOperandAndPlus1<3>";
  let OperandType = "OPERAND_UIMM3_64";
  let OperandNamespace = "LinxV5Op";
}

// A 6-bit unsigned immediate represent N in BXS/BXU/BFI: bits[31:26]
def uimm6_plus1 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<6>(Imm - 1);}]> {
  let ParserMatchClass = UIMM6Plus1AsmOperand;
  let EncoderMethod = "getImmOpValueMinus1";
  let DecoderMethod = "decodeUImmOperandAndPlus1<6>";
  let OperandType = "OPERAND_UIMM6_64";
  let OperandNamespace = "LinxV5Op";
}

def uimm6_rev : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<6>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<6, "Rev">;
  let DecoderMethod = "decodeUImmOperand<6>";
  let OperandType = "OPERAND_UIMM6";
  let OperandNamespace = "LinxV5Op";
}

def uimm6_revPlus1 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<6>(Imm - 1);}]> {
  let ParserMatchClass = UImmAsmOperand<6, "RevPlus1">;
  let EncoderMethod = "getImmOpValueMinus1";
  let DecoderMethod = "decodeUImmOperandAndPlus1<6>";
  let OperandType = "OPERAND_UIMM6_64";
  let OperandNamespace = "LinxV5Op";
}

class SImm20Operand : Operand<XLenVT> {
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<20>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<20>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_SIMM20";
  let OperandNamespace = "LinxV5Op";
}

class SImm32Operand : Operand<XLenVT> {
  let EncoderMethod = "getImmOpValue";
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

class SImmShiftAsmOperand<int num, int shift> : AsmOperandClass {
  let Name = "SImm"#num#"Shift"#shift;
  let DiagnosticType = !strconcat("Invalid", Name);
  let RenderMethod = "addImmShiftedOperands<"#shift#">";
  let PredicateMethod = "isSImmShifted<"#num#", "#shift#">";
}

// simm printing shifted
class simm_pshl<int num, int shift = 0> : Operand<XLenVT> {
  let EncoderMethod = "getImmOpValue";
  let PrintMethod = "printImmShifted<"#shift#">";
  let ParserMatchClass = SImmShiftAsmOperand<num, shift>;
  let DecoderMethod = "decodeSImmOperand<"#num#">";
}

class ImmShiftNAsmOperand<string prefix, int num> : AsmOperandClass {
  let Name = prefix # "Imm"#num#"ShiftN";
  let RenderMethod = "addImmOperands";
  let PredicateMethod = "is"#prefix#"ImmShiftN<"#num#">";
}

// encoding in (simm12 << 5) & shift, code-gen in origin simm
def simm12_shln : Operand<i64>, ImmLeaf<i64, [{return isInt<12>(Imm);}]> {
  let ParserMatchClass = ImmShiftNAsmOperand<"S", 12>;
  let DecoderMethod = "decodeSImmShiftN<12>";
  let EncoderMethod = "getSImmShiftN<12>";
}

// encoding in (uimm12 << 5) & shift, code-gen in origin uimm
def uimm12_shln : Operand<i64>, ImmLeaf<i64, [{return isUInt<12>(Imm);}]> {
  let ParserMatchClass = ImmShiftNAsmOperand<"U", 12>;
  // let PrintMethod = "printImmShiftN<false, 12>";
  let DecoderMethod = "decodeUImmShiftN<12>";
  let EncoderMethod = "getUImmShiftN<12>";
}

// encoding in (simm24 << 5) & shift, code-gen in origin simm
def simm24_shln : Operand<i64>, ImmLeaf<i64, [{return isInt<24>(Imm);}]> {
  let ParserMatchClass = ImmShiftNAsmOperand<"S", 24>;
  let DecoderMethod = "decodeSImmShiftN<24>";
  let EncoderMethod = "getSImmShiftN<24>";
}

// encoding in (uimm24 << 5) & shift, code-gen in origin uimm
def uimm24_shln : Operand<i64>, ImmLeaf<i64, [{return isUInt<24>(Imm);}]> {
  let ParserMatchClass = ImmShiftNAsmOperand<"U", 24>;
  // let PrintMethod = "printImmShiftN<false, 24>";
  let DecoderMethod = "decodeUImmShiftN<24>";
  let EncoderMethod = "getUImmShiftN<24>";
}

// Super-Class for LinxV5 Inst
class LinxV5InstBase : Instruction {
  InstFormat Format = InstFormatDefault;
  PEType PE = PET_ALL;
  TileOpProperty TOP = TileOp_Not;

  let DecoderNamespaces = PE.DecoderNamespaces;

  let Namespace = "LinxV5";

  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;

  let TSFlags{7-0} = Format.Value;
  let TSFlags{15-8} = PE.Value;
  let TSFlags{19-16} = TOP.Value;

  bit isHeaderOnly = 0;
  let TSFlags{48} = isHeaderOnly;

  bit isDisassembleOnly = 0;
  let TSFlags{49} = isDisassembleOnly;

  bit HasDepOutput = 0;
  let TSFlags{50} = HasDepOutput;
}

class LinxV5Inst<dag outs, dag ins, string opcodestr, string argstr> : LinxV5InstBase {
  field bits<32> Inst;
  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<32> SoftFail = 0;
  let Size = 4;

  bits<3> Opcode = 0;
  let Inst{6-4} = Opcode;

  dag OutOperandList = outs;
  dag InOperandList = ins;

  let AsmString = opcodestr # "\t" # argstr;
}

class Pseudo<dag outs, dag ins, list<dag> pattern = [], string opcodestr = "", string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  let Pattern = pattern;
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

class LinxV5Inst_A<bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<5> SrcD;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{31-27} = SrcD;
  let Inst{26-25} = SrcRType;
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Inst{6-4} = opcode.Value;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;
}

// Type A Inst which Imm7 always be 0b0000000. Friendly for llvm mir.
class LinxV5Inst_A_Z7<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                      dag outs, dag ins, string opcodestr>
    : LinxV5Inst_A<0b00, Func, Type, opcode, outs, ins, opcodestr> {
  let SrcD = 0b00000;

  let AsmString = opcodestr # "\t$SrcL, $SrcR, $DstR";
}

class LinxV5Inst_B<bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<5> Imm5;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{31-27} = Imm5;
  let Inst{26-25} = SrcRType;
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Inst{6-4} = opcode.Value;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$SrcL, $SrcR" # "$Imm5, $DstR";
}

class LinxV5Inst_B_SYS<bits<4> Imm, bits<3> order, bits<3> Func,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  let PE = PET_SYS;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{31-28} = Imm;
  let Inst{27-25} = order;
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b101;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t" # argstr;
}

// Type B Inst which Imm5 always be 0b00000. Friendly for llvm mir.
class LinxV5Inst_B_Z5<bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr>
    : LinxV5Inst_B<SrcRType, Func, Type, opcode, outs, ins, opcodestr> {
  let Imm5 = 0b00000;

  let AsmString = opcodestr # "\t$SrcL, $SrcR" # ", $DstR";
}

// Type F Inst for Convert.
class LinxV5Inst_F_Convert<bits<2> SrcType, bits<5> DstType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  let PE = PET_FP;
  bits<5> SrcL;
  bits<5> DstR;

  let Inst{31-27} = DstType;
  let Inst{26-25} = SrcType;
  let Inst{24-20} = 0b00000;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Inst{6-4} = opcode.Value;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$SrcL" # ", $DstR";
}

class LinxV5Inst_C<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<7> Imm7;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{31-25} = Imm7;
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = DstR;
  let Inst{6-4} = opcode.Value;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$SrcL, $SrcR" # ", $DstR";
}

class LinxV5Inst_D<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<12> Imm12;
  bits<5> SrcR;
  bits<5> SrcL;

  let Inst{31-25} = Imm12{6-0};
  let Inst{24-20} = SrcR;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7} = Imm12{11-7};
  let Inst{6-4} = opcode.Value;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$SrcL, $SrcR, $Imm12";
}

class LinxV5Inst_IJ<bits<3> Func, LinxV5InstType Type, bits<3> opcode,
                   dag outs, dag ins, string opcodestr, string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<22> Imm22;
  bits<5> SrcR;
  bits<5> SrcL;

  let Inst{31-15} = Imm22{16-0};
  let Inst{14-12} = Func;
  let Inst{11-7} = Imm22{21-17};
  let Inst{6-4} = opcode;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$Imm22";
}

class LinxV5Inst_E<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : LinxV5Inst<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<12> Imm12;

  let Inst{31-20} = Imm12;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7}  = DstR;
  let Inst{6-4}   = opcode.Value;
  let Inst{3-1}   = Type.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, $Imm12, $DstR";
}

class LinxV5Inst_E_MN<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : LinxV5Inst<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<6> ImmM;
  bits<6> ImmN;

  let Inst{31-26} = ImmM;
  let Inst{25-20} = ImmN;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7}  = DstR;
  let Inst{6-4}   = opcode.Value;
  let Inst{3-1}   = Type.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, $ImmM, $ImmN, $DstR";
}

class LinxV5Inst_E_Z6<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : LinxV5Inst<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<6> Imm6;

  let Inst{31-26} = 0;
  let Inst{25-20} = Imm6;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7}  = DstR;
  let Inst{6-4}   = opcode.Value;
  let Inst{3-1}   = Type.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, $Imm6, $DstR";
}

class LinxV5Inst_E_Z5<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : LinxV5Inst<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<5> Imm5;

  let Inst{31-25} = 0;
  let Inst{24-20} = Imm5;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7}  = DstR;
  let Inst{6-4}   = opcode.Value;
  let Inst{3-1}   = Type.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, $Imm5, $DstR";
}

class LinxV5Inst_B_Z5_ZDst<bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode, dag outs, dag ins, string opcodestr>
    : LinxV5Inst_B_Z5<SrcRType, Func, Type, opcode, outs, ins, opcodestr> {
  let DstR{4-0} = 0b00000;

  let AsmString = opcodestr # "\t$SrcL, $SrcR";
}

class LinxV5Inst_F<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : LinxV5Inst<outs, ins, opcstr, ""> {
  bits<5> SrcL;
  bits<12> Imm12;
  bits<5>  Imm5;

  let Inst{31-20} = Imm12;
  let Inst{19-15} = SrcL;
  let Inst{14-12} = Func;
  let Inst{11-7}  = Imm5;
  let Inst{6-4}   = opcode.Value;
  let Inst{3-1}   = Type.Value;
  let Inst{0}     = 0b1;

  let AsmString = opcstr # "\t$SrcL, $Imm12";
}

class LinxV5Inst_I<LinxV5InstType Type, bits<3> opcode, dag outs, dag ins, string opcodestr,
                     string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<20> imm20;
  bits<5> DstR;

  let Inst{31-12} = imm20;
  let Inst{11-7} = DstR;
  let Inst{6-4} = opcode;
  let Inst{3-1} = Type.Value;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$imm20, $DstR";
}

class LinxV5CInst<bits<3> Opcode, dag outs, dag ins> : LinxV5InstBase {
  field bits<16> Inst;
  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<16> SoftFail = 0;
  let Size = 2;

  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;

  let Namespace = "LinxV5";

  dag OutOperandList = outs;
  dag InOperandList = ins;

  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
}

class LinxV5CInst_A<bits<3> Opcode, bits<2> flag, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> SrcR;
  bits<5> SrcL;

  let Inst{15-11} = SrcR;
  let Inst{10-6} = SrcL;
  let Inst{5-4} = flag;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;
  let AsmString = opcodestr # "\t$SrcL, $SrcR";
}

class LinxV5CInst_B<bits<3> Opcode, bits<2> flag, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> SrcL;
  bits<5> Imm;

  let Inst{15-11} = Imm;
  let Inst{10-6} = SrcL;
  let Inst{5-4} = flag;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;
  let AsmString = opcodestr # "\t$Imm, $SrcL";
}

class LinxV5CInst_C<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> SrcL;
  bits<5> Imm;

  let Inst{15-11} = Func;
  let Inst{10-6} = SrcL;
  let Inst{5-4} = 0b01;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcL";
}

class LinxV5CInst_D<bits<3> Opcode, bits<2> flag, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> DstR;
  bits<5> SrcL;

  let Inst{15-11} = DstR;
  let Inst{10-6} = SrcL;
  let Inst{5-4} = flag;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcL, $DstR";
}

class LinxV5CInst_E<bits<3> Opcode, bits<2> flag, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> Imm;
  bits<5> DstR;

  let Inst{15-11} = DstR;
  let Inst{10-6} = Imm;
  let Inst{5-4} = flag;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$Imm, $DstR";
}

class LinxV5CInst_F<bits<3> Opcode, bits<5> Func, dag outs, dag ins, string opcodestr>
    : LinxV5CInst<Opcode, outs, ins> {
  bits<5> Imm;

  let Inst{15-11} = Func;
  let Inst{10-6} = Imm;
  let Inst{5-4} = 0b10;
  let Inst{3-1} = Opcode;
  let Inst{0} = 0b0;
}

class LinxV5Inst_BSTART<bits<3> branchType, bits<1> dcp, bits<4> blockType, dag outs, dag ins, string opcodestr,
                     string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {

  let Inst{31-15} = 0x0;
  let Inst{14-12} = branchType;
  let Inst{11} = dcp;
  let Inst{10-7} = blockType;
  let Inst{6-3} = 0b0000;
  let Inst{2-1} = 00;
  let Inst{0} = 0b1;

  let AsmString = opcodestr;
}

class LinxV5Inst_TMPL<bits<3>Func, dag outs, dag ins, string opcodestr,
                     string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<15> Imm15;
  bits<5> SrcB;
  bits<5> SrcE;

  let Inst{31-25} = Imm15{9-3};
  let Inst{24-20} = SrcE;
  let Inst{19-15} = SrcB;
  let Inst{14-12} = Func;
  let Inst{11-7}  = Imm15{14-10};
  let Inst{6-4}   = 0b100;
  let Inst{3-1}   = 0b000;
  let Inst{0}   = 0b1;

 let AsmString = opcodestr # "\t[$SrcB ~ $SrcE], sp!, $Imm15";

 let isHeaderOnly = 1;
}

class LinxV5Inst_TMPL_MEM<bits<3>Func, dag outs, dag ins, string opcodestr,
                     string argstr = "">
    : LinxV5Inst<outs, ins, opcodestr, argstr> {
  bits<5> Src0;
  bits<5> Src1;
  bits<5> Src2;

  let Inst{31-27} = Src2;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = Src1;
  let Inst{19-15} = Src0;
  let Inst{14-12} = Func;
  let Inst{11-7}  = 0b00000;
  let Inst{6-4}   = 0b011;
  let Inst{3-1}   = 0b000;
  let Inst{0}   = 0b1;

 let AsmString = opcodestr # "\t[$Src0, $Src1, $Src2]";

 let isHeaderOnly = 1;
}

def getPseudoMap : InstrMapping {
  let FilterClass = "P2I";
  let RowFields = ["PseudoName"];
  let ColFields = ["IsInstr"];
  let KeyCol = ["0"];
  let ValueCols = [["1"]];
}

class P2I<string name, bit instr> {
  string PseudoName = name;
  bit IsInstr = instr;
}

// --------------------------------- SIMT Format ---------------------------------
class V5Inst_SIMT<dag outs, dag ins, string opcodestr, string argstr> : LinxV5InstBase {
  let PE = PET_SIMT;
  field bits<64> Inst;
  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<64> SoftFail = 0;
  let Size = 8;

  let Namespace = "LinxV5";

  dag OutOperandList = outs;
  dag InOperandList = ins;

  let TSFlags{7-0} = Format.Value;
  let TSFlags{15-8} = PE.Value;

  let AsmString = opcodestr # "\t" # argstr;

  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
}

class V5Inst_SIMT_A<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcstr, argstr> {
  bits<7> SrcD;
  bits<3> SrcDType;
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;

  let Inst{63-59} = SrcD{4-0};
  let Inst{58-57} = 0b00;
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-29} = SrcDType;
  let Inst{28-27} = SrcD{6-5};
  let Inst{26-25} = 0b00;
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9}  = DstType;
  let Inst{8-7}   = Dst{6-5};
  let Inst{6-4}   = 0b111;
  let Inst{3-1}   = 0b111;
  let Inst{0}     = 0b1;
}

class V5Inst_SIMT_A_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT_A<Func, FuncLo, Opcode, Type, outs, ins, opcstr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_A_Store_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT_A<Func, FuncLo, Opcode, Type, outs, ins, opcstr> {
  let SrcL{6-5} = 0b01;
  let SrcR{6-5} = 0b01;
  let SrcD{6-5} = 0b01;
  let Inst{28-27} = 0b01;
  let Inst{21-20} = 0b01;
  let Inst{16-15} = 0b01;
}

class V5Inst_SIMT_A_C<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcstr, argstr> {
  bits<7> SrcD;
  bits<3> SrcDType;
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<2> SIMTRegOp;

  let Inst{63-59} = SrcD{4-0};
  let Inst{58-57} = SIMTRegOp;
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-29} = SrcDType;
  let Inst{28-27} = SrcD{6-5};
  let Inst{26-25} = 0b00;
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9}  = DstType;
  let Inst{8-7}   = Dst{6-5};
  let Inst{6-4}   = 0b111;
  let Inst{3-1}   = 0b111;
  let Inst{0}     = 0b1;
}

class V5Inst_SIMT_B<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<7> Dst;
  bits<3> DstType;
  bits<5> Shamt;

  let Inst{63-59} = Shamt;
  let Inst{58-57} = 0b00;
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-27} = 0b00000;
  let Inst{26-25} = 0b00;
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_B_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_B<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_B_T<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<7> Dst;
  bits<3> DstType;
  bits<5> Shamt;
  bits<2> SIMTRegOpcode;

  let Inst{63-59} = Shamt;
  let Inst{58-57} = SIMTRegOpcode;
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-27} = 0b00000;
  let Inst{26-25} = 0b00;
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_B_T_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_B_T<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_D<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcstr, argstr> {
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;

  let Inst{63-59} = 0b00000;
  let Inst{58-57} = 0b00;
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-29} = 0b000;
  let Inst{28-27} = 0b00;
  let Inst{26-25} = 0b00;
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = 0b000;
  let Inst{11-9}  = DstType;
  let Inst{8-7}   = Dst{6-5};
  let Inst{6-4}   = 0b111;
  let Inst{3-1}   = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_D_Scar<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcstr, string argstr = "">
    : V5Inst_SIMT_D<Func, Opcode, Type, outs, ins, opcstr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_E<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> SrcR;
  bits<3> SrcRType;
  bits<12> imm12;
  bits<12> rc_imm12;

  let Inst{63-57} = imm12{6-0};
  let Inst{56-52} = SrcR{4-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = imm12{11-7};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-25} = rc_imm12{6-0};
  let Inst{24-22} = SrcRType;
  let Inst{21-20} = SrcR{6-5};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-7} = rc_imm12{11-7};
  let Inst{6-3} = 0b1111;
  let Inst{2-0} = 0b111;
}

class V5Inst_SIMT_E_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_E<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let SrcL{6-5} = 0b01;
  let SrcR{6-5} = 0b01;
  let Inst{16-15} = 0b01;
  let Inst{21-20} = 0b01;
}

class V5Inst_SIMT_F<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<12> Imm12;

  let Inst{63-52} = Imm12;
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_F_LDSD<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<24> Imm24;

  let Inst{63-52} = Imm24{11-0};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-20} = Imm24{23-12};
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_F_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_F<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_F_Load_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_F_LDSD<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_F_Scar_P<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_F<Func, FuncLo, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b10;
  let Inst{8-7} = 0b10;
}

class V5Inst_SIMT_F_RD<bits<3> FuncHi, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;

  let Inst{63-52} = 0b000000000000;
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = FuncHi;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = FuncLo;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_F_Shiftri<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<6> Shamt;

  let Inst{63-58} = 0b000000;
  let Inst{57-52} = Shamt;
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 0b1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = 0b000;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_F_Shiftri_Scar<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_F_Shiftri<Func, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_F_BIT<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<6> Imms;
  bits<6> Imml;

  let Inst{63-58} = Imms;
  let Inst{57-52} = Imml;
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = 0b000;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
}

class V5Inst_SIMT_F_BIT_Scar<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT_F_BIT<Func, Opcode, Type, outs, ins, opcodestr> {
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
}

class V5Inst_SIMT_F_Buffer<bits<3> Func, bits<3> Opcode, bits<3> Type, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> SrcL;
  bits<3> SrcLType;
  bits<7> Dst;
  bits<3> DstType;
  bits<10> Imm10;

  let Inst{63-59} = Imm10{4-0};
  let Inst{58-57} = 0b00;
  let Inst{56-52} = Imm10{9-5};
  let Inst{51-47} = SrcL{4-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = Type;
  let Inst{32} = 1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-17} = SrcLType;
  let Inst{16-15} = SrcL{6-5};
  let Inst{14-12} = 0b000;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-3} = 0b1111;
  let Inst{2-0} = 0b111;
  let Inst{0} = 0b1;
}

// A 25-bit signed immediate where the least significant bit is zero.
def simm25_lsl3 : Operand<OtherVT> {
  let ParserMatchClass = SImmAsmOperand<25, "Lsl3">;
  let PrintMethod = "printBranchOperand";
  let EncoderMethod = "getImmOpValueAsr3";
  let DecoderMethod = "decodeSImmOperandAndLsl3<25>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<22, 3>(Imm);
    return MCOp.isBareSymbolRef();
  }];
  let OperandType = "OPERAND_PCREL";
}

class V5Inst_SIMT_J<bits<3> Opcode, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {
  bits<7> Dst;
  bits<3> DstType;
  bits<20> Imm20;

  let Inst{63-44} = Imm20;
  let Inst{43-39} = Dst{4-0};
  let Inst{38-36} = Opcode;
  let Inst{35-33} = 0b011;
  let Inst{32} = 0b1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-12} = 0b00000000;
  let Inst{11-9} = DstType;
  let Inst{8-7} = Dst{6-5};
  let Inst{6-4} = 0b111;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$Imm20, " # "$Dst" # "$DstType";
}

class V5Inst_SIMT_K<dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_SIMT<outs, ins, opcodestr, argstr> {

  let Inst{63-47} = 0b00000000000000000;
  let Inst{46-44} = 0b000;
  let Inst{43-39} = 0b00000;
  let Inst{38-36} = 0b000;
  let Inst{35-33} = 0b000;
  let Inst{32} = 0b1;

  let Inst{31-20} = 0b000000000000;
  let Inst{19-12} = 0b00000000;
  let Inst{11-9} = 0b000;
  let Inst{8-7} = 0b00;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;
  let AsmString = opcodestr;
}

//===--------------------------------------LinxV5 48bits Scalar inst format-----------------------------------------------===
class V5Inst_48_base<dag outs, dag ins, string opcodestr, string argstr> : LinxV5InstBase {
  field bits<48> Inst;

  field bits<48> SoftFail = 0;
  let Size = 6;

  let Namespace = "LinxV5";

  dag OutOperandList = outs;
  dag InOperandList = ins;

  let AsmString = opcodestr # "\t" # argstr;

  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
}

class LinxV5Inst_48_B_PRF<bits<5> Model, bits<2> SrcRType, bits<2> Layer,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  let PE = PET_SYS;
  bits<5> Imm5;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{47-43} = Imm5;
  let Inst{42-41} = SrcRType;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = 0b111;
  let Inst{27-23} = DstR;
  let Inst{22-20} = 0b000;
  let Inst{19-17} = 0b100;
  let Inst{16} = 1;
  let Inst{15-11} = Model;
  let Inst{10-6} = 0b00000;
  let Inst{5-4} = Layer;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;
}

class LinxV5Inst_48_E<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : V5Inst_48_base<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<24> Imm24;

  let Inst{47-36} = Imm24{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23}  = DstR;
  let Inst{22-20}   = opcode.Value;
  let Inst{19-17}   = Type.Value;
  let Inst{16} = 1;
  let Inst{15-4} = Imm24{23-12};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcstr # "\t$SrcL, $Imm24, $DstR";
}

class LinxV5Inst_48_E2<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : V5Inst_48_base<outs, ins, opcstr, ""> {
  bits<5> DstR;

  bits<5> SrcL;
  bits<22> Imm22;

  let Inst{47-36} = Imm22{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23}  = DstR;
  let Inst{22-20}   = opcode.Value;
  let Inst{19-17}   = Type.Value;
  let Inst{16} = 1;
  let Inst{15-6} = Imm22{21-12};
  let Inst{5-4} = 0b00;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;
}

class LinxV5Inst_48_E3<bits<2> LoOp, LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : V5Inst_48_base<outs, ins, opcstr, ""> {
  bits<5> DstR;
  bits<5> DstR1;

  bits<5> SrcL;
  bits<17> Imm17;

  let Inst{47-36} = Imm17{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23}  = DstR;
  let Inst{22-20}   = opcode.Value;
  let Inst{19-17}   = Type.Value;
  let Inst{16} = 1;
  let Inst{15-11} = DstR1;
  let Inst{10-6} = Imm17{16-12};
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcstr # "\t[$SrcL, $Imm17], $DstR, $DstR1";
}

class LinxV5Inst_48_E_PRFI<bits<5> Model, bits<2> Layer,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  let PE = PET_SYS;
  bits<17> Imm17;
  bits<5> SrcL;

  bits<5> DstR;

  let Inst{47-36} = Imm17{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = 0b111;
  let Inst{27-23} = DstR;
  let Inst{22-20} = 0b010;
  let Inst{19-17} = 0b100;
  let Inst{16} = 1;
  let Inst{15-11} = Model;
  let Inst{10-6} = Imm17{16-12};
  let Inst{5-4} = Layer;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;
}

class LinxV5Inst_48_F<LinxV5InstType Type, LinxV5InstOpcode opcode, bits<3> Func, dag outs, dag ins, string opcstr>
    : V5Inst_48_base<outs, ins, opcstr, ""> {
  bits<5> Imm5;
  bits<5> SrcL;
  bits<24> Imm24;

  let Inst{47-36} = Imm24{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23}  = Imm5;
  let Inst{22-20}   = opcode.Value;
  let Inst{19-17}   = Type.Value;
  let Inst{16} = 1;
  let Inst{15-4} = Imm24{23-12};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcstr # "\t$SrcL, $Imm12";
}

class LinxV5Inst_48_I<LinxV5InstType Type, bits<3> opcode, dag outs, dag ins, string opcodestr,
                     string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<32> imm32;
  bits<5> DstR;

  let Inst{47-28} = imm32{19-0};
  let Inst{27-23}  = DstR;
  let Inst{22-20} = opcode;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-4} = imm32{31-20};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$imm32, $DstR";
}

class V5Inst_48_SSR<bits<3> Func, dag outs, dag ins, string opcodestr, string argstr = "">
      : V5Inst_48_base<outs, ins, opcodestr, argstr>  {
  bits<24> Imm24;
  bits<5> Dst;
  bits<5> SrcL;

  let Inst{47-36} = Imm24{11-0};
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = Dst;
  let Inst{22-20} = 0b011;
  let Inst{19-17} = 0b101;
  let Inst{16} = 1;
  let Inst{15-4} = Imm24{23-12};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t" # argstr;
}

class LinxV5Inst_48_A<bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcD;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;
  bits<5> DstR1;

  let Inst{47-43} = SrcD;
  let Inst{42-41} = SrcRType;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = DstR;
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = DstR1;
  let Inst{10-4} = 0;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;
}

class LinxV5Inst_48_A2<bits<2> LoOp, bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string explicit_lsl, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcD;
  bits<5> SrcD1;
  bits<5> SrcR;
  bits<5> SrcL;

  let Inst{47-43} = SrcD;
  let Inst{42-41} = SrcRType;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = 0;
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = 0;
  let Inst{10-6} = SrcD1;
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcD, $SrcD1, [$SrcL, ${SrcR}"#explicit_lsl#"]";
}

class LinxV5Inst_48_A3<bits<2> LoOp, bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string explicit_lsl, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcD;
  bits<5> DstR;
  bits<5> SrcR;
  bits<5> SrcL;

  let Inst{47-43} = SrcD;
  let Inst{42-41} = SrcRType;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = 0;
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = DstR;
  let Inst{10-6} = 0;
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcD, [$SrcL, ${SrcR}"#explicit_lsl#"], $DstR";
}

// Type A Inst which Imm7 always be 0b0000000. Friendly for llvm mir.
class LinxV5Inst_48_A_Z7<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                      dag outs, dag ins, string opcodestr>
    : LinxV5Inst_48_A<0b00, Func, Type, opcode, outs, ins, opcodestr> {
  let SrcD = 0b00000;

  let AsmString = opcodestr # "\t$SrcL, $SrcR, $DstR, $DstR1";
}

class LinxV5Inst_48_C<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<7> Imm7;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;
  bits<5> DstR1;

  let Inst{47-41} = Imm7;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = DstR;
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = DstR1;
  let Inst{10-4} = 0;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcL, $SrcR" # ", $DstR, $DstR1";
}

class LinxV5Inst_48_B<bits<2> LoOp, bits<2> SrcRType, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                     dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<5> Imm5;
  bits<5> SrcR;
  bits<5> SrcL;

  bits<5> DstR;
  bits<5> DstR1;

  let Inst{47-43} = Imm5;
  let Inst{42-41} = SrcRType;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = DstR;
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = DstR1;
  let Inst{10-6} = 0;
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t[$SrcL, $SrcR" # "$Imm5], $DstR, $DstR1";
}

class LinxV5Inst_48_D<bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<22> Imm22;
  bits<5> SrcR;
  bits<5> SrcD;

  let Inst{47-41} = Imm22{6-0};
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcD;
  let Inst{30-28} = Func;
  let Inst{27-23} = Imm22{11-7};
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-6} = Imm22{21-12};
  let Inst{5-4} = 0b00;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcD, $SrcR, $Imm22";
}

class LinxV5Inst_48_D2<bits<2> LoOp, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<17> Imm17;
  bits<5> SrcR;
  bits<5> SrcD;
  bits<5> SrcD1;

  let Inst{47-41} = Imm17{6-0};
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcD;
  let Inst{30-28} = Func;
  let Inst{27-23} = Imm17{11-7};
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = Imm17{16-12};
  let Inst{10-6} = SrcD1;
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcD, $SrcD1, [$SrcR, $Imm17]";
}

class LinxV5Inst_48_D3<bits<2> LoOp, bits<3> Func, LinxV5InstType Type, LinxV5InstOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<17> Imm17;
  bits<5> SrcR;
  bits<5> SrcD;
  bits<5> DstR;

  let Inst{47-41} = Imm17{6-0};
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcD;
  let Inst{30-28} = Func;
  let Inst{27-23} = Imm17{11-7};
  let Inst{22-20} = opcode.Value;
  let Inst{19-17} = Type.Value;
  let Inst{16} = 0b1;
  let Inst{15-11} = DstR;
  let Inst{10-6} = Imm17{16-12};
  let Inst{5-4} = LoOp;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcD, [$SrcR, $Imm17], $DstR";
}

class V5Inst_48_A_D<bits<4> Flag, bits<3> Mask, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcL;
  bits<5> SrcD;
  bits<5> SrcR;
  bits<5> Dst;

  let Inst{47-44} = Flag;
  let Inst{43-41} = Mask;
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = 0b110;
  let Inst{27-23} = Dst;
  let Inst{22-20} = 0b000;
  let Inst{19-17} = 0b101;
  let Inst{16-11} = 0b100000;
  let Inst{10-6} = SrcD;
  let Inst{5-4} = 0b00;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t[$SrcL], " # "$SrcR, $SrcD, $Dst";
}

class V5Inst_48_J<bits<3> Opcode,  dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<32> Imm32;
  bits<5> Dst;

  let Inst{47-28} = Imm32{19-0};
  let Inst{27-23} = Dst;
  let Inst{22-20} = Opcode;
  let Inst{19-17} = 0b110;
  let Inst{16} = 0b1;
  let Inst{15-4} = Imm32{31-20};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$Imm32, $Dst";
}

class V5Inst_48_M_A<bits<3> Func,  dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_48_base<outs, ins, opcodestr, argstr> {
  bits<32> Imm19;
  bits<5> SrcR;
  bits<5> SrcL;
  bits<5> Dst;

  let Inst{47-41} = Imm19{6-0};
  let Inst{40-36} = SrcR;
  let Inst{35-31} = SrcL;
  let Inst{30-28} = Func;
  let Inst{27-23} = Dst;
  let Inst{22-20} = 0b100;
  let Inst{19-17} = 0b110;
  let Inst{16} = 0b1;
  let Inst{15-4} = Imm19{18-7};
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b0;

  let AsmString = opcodestr # "\t$SrcL, $SrcR, $Imm19, $Dst";
}
//===--------------------------------------LinxV5 64bits Scalar inst format-----------------------------------------------===
class V5Inst_64_base<dag outs, dag ins, string opcodestr, string argstr> : LinxV5InstBase {
  let PE = PET_SIMT;
  field bits<64> Inst;

  field bits<64> SoftFail = 0;
  let Size = 8;

  let Namespace = "LinxV5";

  dag OutOperandList = outs;
  dag InOperandList = ins;

  let TSFlags{7-0} = Format.Value;
  let TSFlags{15-8} = PE.Value;

  let AsmString = opcodestr # "\t" # argstr;

  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 0;
}

class V5Inst_64_J_F<bits<3> Func, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_64_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcL;
  bits<5> Dst;
  bits<32> Imm32;

  let Inst{63-52} = Imm32{11-0};
  let Inst{51-47} = SrcL;
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst;
  let Inst{38-36} = 0b001;
  let Inst{35-33} = 0b010;
  let Inst{32} = 0b1;

  let Inst{31-12} = Imm32{31-12};
  let Inst{11-7} = 0b00000;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t$SrcL, " # "\t$Imm32, " # "$Dst";
}

class V5Inst_64_K_H<bits<3> Func, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_64_base<outs, ins, opcodestr, argstr> {
  bits<5> Dst;
  bits<42> Imm42;

  let Inst{63-47} = Imm42{16-0};
  let Inst{46-44} = Func;
  let Inst{43-39} = Dst;
  let Inst{38-36} = 0b011;
  let Inst{35-33} = 0b100;
  let Inst{32} = 0b1;

  let Inst{31-7} = Imm42{41-17};
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "\t[$Imm42], " # "$Dst";
}

class V5Inst_64_K_G<bits<3> Func, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_64_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcL;
  bits<42> Imm42;

  let Inst{63-52} = Imm42{11-0};
  let Inst{51-47} = SrcL;
  let Inst{46-44} = Func;
  let Inst{43-39} = Imm42{16-12};
  let Inst{38-36} = 0b110;
  let Inst{35-33} = 0b100;
  let Inst{32} = 0b1;

  let Inst{31-7} = Imm42{41-17};
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = opcodestr  # "\t$SrcL, " # "[$Imm42]";
}

class V5Inst_64_A_D<bits<4> Flag, bits<3> Mask, dag outs, dag ins, string opcodestr, string argstr = "">
    : V5Inst_64_base<outs, ins, opcodestr, argstr> {
  bits<5> SrcL;
  bits<5> SrcD0;
  bits<5> SrcD1;
  bits<5> SrcR0;
  bits<5> SrcR1;
  bits<5> Dst0;
  bits<5> Dst1;

  let Inst{63-60} = Flag;
  let Inst{59-57} = Mask;
  let Inst{56-52} = SrcR0;
  let Inst{51-47} = SrcL;
  let Inst{46-44} = 0b110;
  let Inst{43-39} = Dst0;
  let Inst{38-36} = 0b000;
  let Inst{35-33} = 0b101;
  let Inst{32} = 0b1;

  let Inst{31-27} = SrcD1;
  let Inst{26-25} = 0b00;
  let Inst{24-20} = SrcR1;
  let Inst{19-15} = SrcD0;
  let Inst{14-12} = 0b000;
  let Inst{11-7} = Dst1;
  let Inst{6-4} = 0b000;
  let Inst{3-1} = 0b111;
  let Inst{0} = 0b1;

  let AsmString = opcodestr # "[$SrcL], " # "$SrcR0, $SrcR1, $SrcD0, $SrcD1, $Dst0, $Dst1";
}
