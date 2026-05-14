//===------- LinxV5InstrInfoSIMT.td - LinxV5 SIMT instructions ---*- tablegen -*------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------------------------===//

def SDT_LinxV5Reduce : SDTypeProfile<1, 1, []>;
def linxv5_rdadd : SDNode<"LinxV5ISD::RDADD", SDT_LinxV5Reduce, [SDNPHasChain]>; // <dst> = RDADD <src0>
def linxv5_rdor  : SDNode<"LinxV5ISD::RDOR",  SDT_LinxV5Reduce, [SDNPHasChain]>; // <dst> = RDOR  <src0>
def linxv5_rdmax : SDNode<"LinxV5ISD::RDMAX", SDT_LinxV5Reduce, [SDNPHasChain]>; // <dst> = RDMAX <src0>
def linxv5_rdmin : SDNode<"LinxV5ISD::RDMIN", SDT_LinxV5Reduce, [SDNPHasChain]>; // <dst> = RDMIN <src0>

def SDT_LinxV5Shuffle : SDTypeProfile<1, 3, []>;
def linxv5_shfl_up   : SDNode<"LinxV5ISD::SHFLUP",   SDT_LinxV5Shuffle, [SDNPHasChain]>; // <dst> = SHFLUP   <src0>, <src1>, <src2>
def linxv5_shfl_down : SDNode<"LinxV5ISD::SHFLDOWN", SDT_LinxV5Shuffle, [SDNPHasChain]>; // <dst> = SHFLDOWN <src0>, <src1>, <src2>
def linxv5_shfl_idx  : SDNode<"LinxV5ISD::SHFLIDX",  SDT_LinxV5Shuffle, [SDNPHasChain]>; // <dst> = SHFLIDX  <src0>, <src1>, <src2>
def SDT_LinxV5Shuffle_Xor : SDTypeProfile<1, 2, []>;
def linxv5_shfl_xor  : SDNode<"LinxV5ISD::SHFLXOR",  SDT_LinxV5Shuffle_Xor, [SDNPHasChain]>; // <dst> = SHFLXOR  <src0>, <src1>

def SDT_LinxV5FPArith : SDTypeProfile<1, 1, []>;
def linxv5_fabs  : SDNode<"LinxV5ISD::FABS",  SDT_LinxV5FPArith, [SDNPHasChain]>; // <dst> = FABS  <src0>
def linxv5_fsqrt : SDNode<"LinxV5ISD::FSQRT", SDT_LinxV5FPArith, [SDNPHasChain]>; // <dst> = FSQRT <src0>
def linxv5_fexp  : SDNode<"LinxV5ISD::FEXP",  SDT_LinxV5FPArith, [SDNPHasChain]>; // <dst> = fexp  <src0>

def SDT_LinxV5TwoSrcFloat : SDTypeProfile<1, 2, []>;
def linxv5_max  : SDNode<"LinxV5ISD::MAX", SDT_LinxV5TwoSrcFloat, [SDNPHasChain]>; // <dst> = MAX   <src0>, <src1>
def linxv5_min  : SDNode<"LinxV5ISD::MIN", SDT_LinxV5TwoSrcFloat, [SDNPHasChain]>; // <dst> = MIN   <src0>, <src1>

// As the llvm-tblgen crashes (iPTR (sext i32)), we should use the ComplexPattern
// to match the extended offset pattern.
def SIMTLoadADDRrr :
    ComplexPattern<iPTR, 4, "SelectSIMTLoadADDRrr", [], [], 1>;

def SIMTStoreADDRrr_unscaled :
    ComplexPattern<iPTR, 4, "SelectSIMTStoreADDRrr_unscaled", [], [], 1>;
class SIMTStoreADDRrr_scaled<int shift> :
    ComplexPattern<iPTR, 4, "SelectSIMTStoreADDRrr_scaled<"#shift#">", [], [], 2>;

def SIMTADDRri_unscaled :
    ComplexPattern<iPTR, 2, "SelectADDRri_unscaled", [], [], 3>;
class SIMTADDRri_scaled<int shift> :
    ComplexPattern<iPTR, 2, "SelectADDRri_scaled<"#shift#">", [], [], 4>;

class ContinuousLoadADDRrr<int scale> :
    ComplexPattern<iPTR, 4, "SelectContinuousLoadADDRrr<"#scale#">", [], [], 2>;
class ContinuousStoreADDRrr<int scale> :
    ComplexPattern<iPTR, 4, "SelectContinuousStoreADDRrr<"#scale#">", [], [], 3>;
class ContinuousADDRri<int scale> :
    ComplexPattern<iPTR, 2, "SelectContinuousADDRri<"#scale#">", [], [], 5>;

def SIMTIntRegAsmOperand : AsmOperandClass {
  let Name = "SIMTIntReg";
  let ParserMethod = "parseSIMTIntReg";
  let RenderMethod = "addRegOperands";
}

def SIMTDstIntRegAsmOperand : AsmOperandClass {
  let Name = "SIMTDstIntReg";
  let ParserMethod = "parseSIMTDstVecRWithArrow";
  let RenderMethod = "addRegOperands";
}

def SIMTDstIntRegReduceAsmOperand : AsmOperandClass {
  let Name = "SIMTDstIntRegReduce";
  let ParserMethod = "parseSIMTDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def SIMTSrcReg : RegisterOperand<SIMT_SRC> {
  let ParserMatchClass = SIMTIntRegAsmOperand;
  let PrintMethod = "printSIMTSrcReg";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTSrcScalarReg : RegisterOperand<SIMT_SRC_Scalar> {
  let ParserMatchClass = SIMTIntRegAsmOperand;
  let PrintMethod = "printSIMTSrcReg";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTSrcVecReg : RegisterOperand<SIMT_SRC_Vec> {
  let ParserMatchClass = SIMTIntRegAsmOperand;
  let PrintMethod = "printSIMTSrcReg";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstIntScalarRegAsmOperand : AsmOperandClass {
  let Name = "SIMTDstIntScalarReg";
  let ParserMethod = "parseSIMTDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def SIMTDstReg : RegisterOperand<SIMT_DST> {
  let ParserMatchClass = SIMTDstIntRegAsmOperand;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstScalarReg : RegisterOperand<SIMT_DST_Scalar> {
  let ParserMatchClass = SIMTDstIntScalarRegAsmOperand;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstIntScalarRegAsmOperandP : AsmOperandClass {
  let Name = "SIMTDstIntScalarRegP";
  let ParserMethod = "parseSIMTDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def SIMTDstScalarRegP : RegisterOperand<SIMT_DST_Scalar> {
  let ParserMatchClass = SIMTDstIntScalarRegAsmOperandP;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstCMPIntScalarRegAsmOperand : AsmOperandClass {
  let Name = "SIMTDstCMPIntScalarReg";
  let ParserMethod = "parseSIMTDstRWithArrow";
  let RenderMethod = "addRegOperands";
}

def SIMTDstCMPScalarReg : RegisterOperand<SIMT_DST_CMP_Scalar> {
  let ParserMatchClass = SIMTDstCMPIntScalarRegAsmOperand;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstVecReg : RegisterOperand<SIMT_DST_Vec> {
  let ParserMatchClass = SIMTDstIntRegAsmOperand;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

def SIMTDstRegReduce : RegisterOperand<SIMT_DST_Scalar> {
  let ParserMatchClass = SIMTDstIntRegReduceAsmOperand;
  let PrintMethod = "printDstRWithArrow";
  let EncoderMethod = "encodingSIMTReg";
}

class SIMTIntSrcRegType<bits<3> val> {
  bits<3> Value = val;
}
def SIMT_Int_SrcReg_UD  :  SIMTIntSrcRegType<0>;
def SIMT_Int_SrcReg_UW :  SIMTIntSrcRegType<1>;
def SIMT_Int_SrcReg_UH :  SIMTIntSrcRegType<2>;
def SIMT_Int_SrcReg_UB :  SIMTIntSrcRegType<3>;
def SIMT_Int_SrcReg_SD  :  SIMTIntSrcRegType<4>;
def SIMT_Int_SrcReg_SW :  SIMTIntSrcRegType<5>;
def SIMT_Int_SrcReg_SH :  SIMTIntSrcRegType<6>;
def SIMT_Int_SrcReg_SB :  SIMTIntSrcRegType<7>;

def SIMTIntSrcRegTypeAsmOperand : AsmOperandClass {
  let Name = "SIMTIntSrcRegType";
  let ParserMethod = "parseSIMTIntSrcRegType";
  let RenderMethod = "addSIMTRegType";
}

def SIMTIntSrcRegType : Operand<i8> {
  let ParserMatchClass = SIMTIntSrcRegTypeAsmOperand;
  let PrintMethod = "printSIMTIntSrcRegType";
}

class SIMTFloatSrcRegType<bits<3> val> {
  bits<3> Value = val;
}
def SIMT_Float_SrcReg_FD  :  SIMTFloatSrcRegType<0>;
def SIMT_Float_SrcReg_FS  :  SIMTFloatSrcRegType<1>;
def SIMT_Float_SrcReg_FH  :  SIMTFloatSrcRegType<2>;
def SIMT_Float_SrcReg_FB  :  SIMTFloatSrcRegType<3>;
// Reserve
def SIMT_Float_SrcReg_BF  :  SIMTFloatSrcRegType<6>;
def SIMT_Float_SrcReg_FLB :  SIMTFloatSrcRegType<7>;

def SIMTFloatSrcRegTypeAsmOperand : AsmOperandClass {
  let Name = "SIMTFloatSrcRegType";
  let ParserMethod = "parseSIMTFloatSrcRegType";
  let RenderMethod = "addSIMTRegType";
}

def SIMTFloatSrcRegType : Operand<i8> {
  let ParserMatchClass = SIMTFloatSrcRegTypeAsmOperand;
  let PrintMethod = "printSIMTFloatSrcRegType";
}

class SIMTShamtImmAsmOperand<int shift> : AsmOperandClass {
  let Name = "SIMTShamtImmPlus"#shift;
  let DiagnosticType = !strconcat("Invalid", Name);
  let ParserMethod = "parseSIMTShamtImm";
  let RenderMethod = "addShamtImmOperands<"#shift#">";
  let PredicateMethod = "isSIMTShamtImmPlus<"#shift#">";
}

// simm printing shifted
class SIMTShamtImm<int shift = 0> :  Operand<i64>, ImmLeaf<i64, [{return isUInt<5>(Imm);}]> {
  let ParserMatchClass = SIMTShamtImmAsmOperand<shift>;
  let PrintMethod = "printShamtImmPlus<"#shift#">";
  let DecoderMethod = "decodeUImmOperand<5>";
}

class SIMTDstRegType<bits<2> val> {
  bits<2> Value = val;
}
def SIMT_DstReg_D  :  SIMTDstRegType<0>;
def SIMT_DstReg_W  :  SIMTDstRegType<1>;
def SIMT_DstReg_H  :  SIMTDstRegType<2>;
def SIMT_DstReg_B  :  SIMTDstRegType<3>;

def SIMTDstRegTypeAsmOperand : AsmOperandClass {
  let Name = "SIMTDstRegType";
  let ParserMethod = "parseSIMTDstRegType";
  let RenderMethod = "addSIMTRegType";
}

def SIMTDstRegType : Operand<i8> {
  let ParserMatchClass = SIMTDstRegTypeAsmOperand;
  let PrintMethod = "printSIMTDstRegType";
}

class SIMTRegOpAsmOperand<string suffix> : AsmOperandClass {
  let Name = "SIMTRegOp" # suffix;
  let ParserMethod = "parseSIMTRegOp" # suffix;
  let RenderMethod = "addSIMTRegOp";
}

def SIMTRegOpAU : Operand<i8> {
  let ParserMatchClass = SIMTRegOpAsmOperand<"AU">;
  let PrintMethod = "printSIMTRegOp";
}

def SIMTRegOpLU : Operand<i8> {
  let ParserMatchClass = SIMTRegOpAsmOperand<"LU">;
  let PrintMethod = "printSIMTRegOp";
}

//===============Uniform & Divergent====================
class UniformTernaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR, node:$SrcA), (N node:$SrcL, node:$SrcR, node:$SrcA), [{
  return !N->isDivergent();
}]>;
class UniformOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR), (N node:$SrcL, node:$SrcR), [{
  return !N->isDivergent();
}]>;

class UniformUnaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  return !N->isDivergent();
}]>;


class Uniform<SDPatternOperator N>
  : PatFrag<(ops ), (N ), [{
  return !N->isDivergent();
}]>;

class DivergentTernaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR, node:$SrcA), (N node:$SrcL, node:$SrcR, node:$SrcA), [{
  return N->isDivergent();
}]>;

class DivergentOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR), (N node:$SrcL, node:$SrcR), [{
  return N->isDivergent();
}]>;

class Divergent<SDPatternOperator N>
  : PatFrag<(ops ), (N ), [{
  return N->isDivergent();
}]>;

class DivergentUnaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  return N->isDivergent();
}]>;

class LoopRegUnaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  SDValue Operand = N->getOperand(0);
  return isLoopReg(Operand);
}]>;

class RIORegUnaryOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  SDValue Operand = N->getOperand(0);
  return isRIOReg(Operand);
}]>;


class DivergentSetccOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR, node:$Cond), (N node:$SrcL, node:$SrcR, node:$Cond), [{
  return N->isDivergent();
}]>;


class UniformSetccOp<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR, node:$Cond), (N node:$SrcL, node:$SrcR, node:$Cond), [{
  return !N->isDivergent();
}]>;

class SIMT_AUrr_T_N<bits<3> Func,  string opcstr> : V5Inst_SIMT_B_T<Func, 0b000, 0b000, 0b010, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType,
                    SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
                    SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode,
                    SIMTShamtImm<0>:$Shamt, Sat:$sat), opcstr>, Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<1> sat;
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType" # "$SIMTRegOpcode" # "$Shamt, " # "$Dst" # "$DstType, $sat";
}

class SIMT_AUrr_T_N_Scar<bits<3> Func,  string opcstr> : V5Inst_SIMT_B_T_Scar<Func, 0b000, 0b000, 0b010, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType,
                    SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
                    SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode,
                    SIMTShamtImm<0>:$Shamt, Sat:$sat), opcstr>, Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<1> sat;
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType" # "$SIMTRegOpcode" # "$Shamt, " # "$Dst" # "$DstType, $sat";
}

let Uses = [SIMT_P] in {
let isReMaterializable = 1 in
def SIMT_ADD : SIMT_AUrr_T_N<0b000, "v.add">;
let isReMaterializable = 1 in
def SIMT_SUB : SIMT_AUrr_T_N<0b001, "v.sub">;
let isReMaterializable = 1 in
def SIMT_ADD_SCAR : SIMT_AUrr_T_N_Scar<0b000, "l.add">;
let isReMaterializable = 1 in
def SIMT_SUB_SCAR : SIMT_AUrr_T_N_Scar<0b001, "l.sub">;
}
def : InstAlias<"v.add\t$SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOpcode$Shamt, $Dst$DstType", (SIMT_ADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode, SIMTShamtImm<0>:$Shamt, 0), 0>;
def : InstAlias<"v.sub\t$SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOpcode$Shamt, $Dst$DstType", (SIMT_SUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode, SIMTShamtImm<0>:$Shamt, 0), 0>;
def : InstAlias<"l.add\t$SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOpcode$Shamt, $Dst$DstType", (SIMT_ADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode, SIMTShamtImm<0>:$Shamt, 0), 0>;
def : InstAlias<"l.sub\t$SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOpcode$Shamt, $Dst$DstType", (SIMT_SUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOpcode, SIMTShamtImm<0>:$Shamt, 0), 0>;

class SIMT_LUrr_T_N<bits<3> Func,  string opcstr> : V5Inst_SIMT_B_T<Func, 0b000, 0b000, 0b010, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType,
                    SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
                    SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpLU:$SIMTRegOpcode,
                    SIMTShamtImm<0>:$Shamt), opcstr>, Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType" # "$SIMTRegOpcode" # "$Shamt, " # "$Dst" # "$DstType";
}

class SIMT_LUrr_T_N_Scar<bits<3> Func,  string opcstr> : V5Inst_SIMT_B_T_Scar<Func, 0b000, 0b000, 0b010, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType,
                    SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
                    SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpLU:$SIMTRegOpcode,
                    SIMTShamtImm<0>:$Shamt), opcstr>, Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType" # "$SIMTRegOpcode" # "$Shamt, " # "$Dst" # "$DstType";
}

let Uses = [SIMT_P] in {
def SIMT_AND : SIMT_LUrr_T_N<0b010, "v.and">;
def SIMT_OR  : SIMT_LUrr_T_N<0b011, "v.or">;
def SIMT_XOR : SIMT_LUrr_T_N<0b100, "v.xor">;
def SIMT_AND_SCAR : SIMT_LUrr_T_N_Scar<0b010, "l.and">;
def SIMT_OR_SCAR  : SIMT_LUrr_T_N_Scar<0b011, "l.or">;
def SIMT_XOR_SCAR : SIMT_LUrr_T_N_Scar<0b100, "l.xor">;
}

multiclass SIMT_Base_RR_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
  def : Pat<(i32 (OpNode i32:$SrcL, i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
  def : Pat<(i16 (OpNode i16:$SrcL, i16:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value), 0, 0)>;
  def : Pat<(i8 (OpNode i8:$SrcL, i8:$SrcR)),
            (Inst (i8 SIMT_DstReg_B.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value), 0, 0)>;
}

multiclass SIMT_Base_RR_S_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0, 0)>;
  def : Pat<(i32 (OpNode i32:$SrcL, i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0, 0)>;
  def : Pat<(i16 (OpNode i16:$SrcL, i16:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value), 0, 0, 0)>;
  def : Pat<(i8 (OpNode i8:$SrcL, i8:$SrcR)),
            (Inst (i8 SIMT_DstReg_B.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value), 0, 0, 0)>;
}

let Predicates = [isSIMT] in {
defm : SIMT_Base_RR_S_Pat<DivergentOp<add>, SIMT_ADD>;
defm : SIMT_Base_RR_S_Pat<DivergentOp<sub>, SIMT_SUB>;
defm : SIMT_Base_RR_Pat<DivergentOp<and>, SIMT_AND>;
defm : SIMT_Base_RR_Pat<DivergentOp<or>, SIMT_OR>;
defm : SIMT_Base_RR_Pat<DivergentOp<xor>, SIMT_XOR>;

defm : SIMT_Base_RR_S_Pat<UniformOp<add>, SIMT_ADD_SCAR>;
defm : SIMT_Base_RR_S_Pat<UniformOp<sub>, SIMT_SUB_SCAR>;
defm : SIMT_Base_RR_Pat<UniformOp<and>, SIMT_AND_SCAR>;
defm : SIMT_Base_RR_Pat<UniformOp<or>, SIMT_OR_SCAR>;
defm : SIMT_Base_RR_Pat<UniformOp<xor>, SIMT_XOR_SCAR>;

}

multiclass SIMT_Base_RR_Shift_S_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (shl (i64 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm, 0)>;

  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (shl (i32 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm, 0)>;

  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$SrcL)), (i64 (shl (i64 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm, 0)>;
  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$SrcL)), (i32 (shl (i32 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm, 0)>;

  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (shl i64:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, SIMTShamtImm<0>:$imm, 0)>;
  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (shl i32:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, SIMTShamtImm<0>:$imm, 0)>;
  def : Pat<(i16 (OpNode i16:$SrcL, (i16 (shl i16:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_H.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value), 0, SIMTShamtImm<0>:$imm, 0)>;
  def : Pat<(i8 (OpNode i8:$SrcL, (i8 (shl i8:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_B.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value), 0, SIMTShamtImm<0>:$imm, 0)>;
}

multiclass SIMT_Base_RR_Shift_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (shl (i64 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm)>;

  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (shl (i32 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm)>;

  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$SrcL)), (i64 (shl (i64 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm)>;
  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$SrcL)), (i32 (shl (i32 (LoopRegUnaryOp<zext> i16:$Src)), SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, SIMTShamtImm<0>:$imm)>;

  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (shl i64:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, SIMTShamtImm<0>:$imm)>;
  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (shl i32:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, SIMTShamtImm<0>:$imm)>;
  def : Pat<(i16 (OpNode i16:$SrcL, (i16 (shl i16:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_H.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value), 0, SIMTShamtImm<0>:$imm)>;
  def : Pat<(i8 (OpNode i8:$SrcL, (i8 (shl i8:$SrcR, SIMTShamtImm<0>:$imm)))),
            (Inst (i8 SIMT_DstReg_B.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value), 0, SIMTShamtImm<0>:$imm)>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_Base_RR_Shift_S_Pat<DivergentOp<add>, SIMT_ADD>;
  defm : SIMT_Base_RR_Shift_S_Pat<DivergentOp<sub>, SIMT_SUB>;
  defm : SIMT_Base_RR_Shift_Pat<DivergentOp<and>, SIMT_AND>;
  defm : SIMT_Base_RR_Shift_Pat<DivergentOp<or>, SIMT_OR>;
  defm : SIMT_Base_RR_Shift_Pat<DivergentOp<xor>, SIMT_XOR>;

  defm : SIMT_Base_RR_Shift_S_Pat<UniformOp<add>, SIMT_ADD_SCAR>;
  defm : SIMT_Base_RR_Shift_S_Pat<UniformOp<sub>, SIMT_SUB_SCAR>;
  defm : SIMT_Base_RR_Shift_Pat<UniformOp<and>, SIMT_AND_SCAR>;
  defm : SIMT_Base_RR_Shift_Pat<UniformOp<or>, SIMT_OR_SCAR>;
  defm : SIMT_Base_RR_Shift_Pat<UniformOp<xor>, SIMT_XOR_SCAR>;
}

multiclass SIMT_Base_RR_Shift_CVT_Pat<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (IcvtOp SourceVT:$SrcR)))), (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$SrcR, (i8 SourceRType), 0, 0)>;
  def : Pat<(VT (OpNode VT:$SrcL, (VT (shl (VT (IcvtOp SourceVT:$Src)), SIMTShamtImm<0>:$imm)))), (Inst (i8 DType), VT:$SrcL, (i8 RType), SourceVT:$Src, (i8 SourceRType), 0, SIMTShamtImm<0>:$imm)>;
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (shl (VT (IcvtOp SourceVT:$Src)), SIMTShamtImm<0>:$imm)))), (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$Src, (i8 SourceRType), 0, SIMTShamtImm<0>:$imm)>;
}

multiclass SIMT_Base_RR_Shift_CVT_Pat_RS<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (IcvtOp SourceVT:$SrcR)))), (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$SrcR, (i8 SourceRType), 0, 0, 0)>;
  def : Pat<(VT (OpNode VT:$SrcL, (VT (shl (VT (IcvtOp SourceVT:$Src)), SIMTShamtImm<0>:$imm)))), (Inst (i8 DType), VT:$SrcL, (i8 RType), SourceVT:$Src, (i8 SourceRType), 0, SIMTShamtImm<0>:$imm, 0)>;
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (shl (VT (IcvtOp SourceVT:$Src)), SIMTShamtImm<0>:$imm)))), (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$Src, (i8 SourceRType), 0, SIMTShamtImm<0>:$imm, 0)>;
}

multiclass SIMT_Base_RR_Shift_Extend_CVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_Shift_Extend_CVT_Pat_RS<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_Shift_ZeroExtend_CVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_Shift_ZeroExtend_CVT_Pat_RS<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_Shift_CVT_Pat_RS<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_Shift_High_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_RR_Shift_Extend_CVT_Pat<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_RR_Shift_Extend_CVT_Pat<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_RR_Shift_ZeroExtend_CVT_Pat<N, I, RIORegUnaryOp<zext>>;
}

multiclass SIMT_Base_RR_Shift_High_Pat_RS<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_RR_Shift_Extend_CVT_Pat_RS<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_RR_Shift_Extend_CVT_Pat_RS<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_RR_Shift_ZeroExtend_CVT_Pat_RS<N, I, RIORegUnaryOp<zext>>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_Base_RR_Shift_High_Pat_RS<DivergentOp<add>, SIMT_ADD>;
  defm : SIMT_Base_RR_Shift_High_Pat_RS<DivergentOp<sub>, SIMT_SUB>;
  defm : SIMT_Base_RR_Shift_High_Pat<DivergentOp<and>, SIMT_AND>;
  defm : SIMT_Base_RR_Shift_High_Pat<DivergentOp<or>, SIMT_OR>;
  defm : SIMT_Base_RR_Shift_High_Pat<DivergentOp<xor>, SIMT_XOR>;

  defm : SIMT_Base_RR_Shift_High_Pat_RS<UniformOp<add>, SIMT_ADD_SCAR>;
  defm : SIMT_Base_RR_Shift_High_Pat_RS<UniformOp<sub>, SIMT_SUB_SCAR>;
  defm : SIMT_Base_RR_Shift_High_Pat<UniformOp<and>, SIMT_AND_SCAR>;
  defm : SIMT_Base_RR_Shift_High_Pat<UniformOp<or>, SIMT_OR_SCAR>;
  defm : SIMT_Base_RR_Shift_High_Pat<UniformOp<xor>, SIMT_XOR_SCAR>;
}

class SIMT_ALUrr_ReserveShmat<RegisterOperand DstRegType, RegisterOperand SrcRegType, bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_B<Func, 0b000, Opcode, 0b010,
      (outs DstRegType:$Dst),
                    (ins SIMTDstRegType:$DstType, SrcRegType:$SrcL, SIMTIntSrcRegType:$SrcLType, SrcRegType:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>,
                     Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType, " # "$Dst" # "$DstType";
  let Shamt = 0b00000;
}

multiclass SIMT_ALUrr_ReserveShmat_VS< bits<3> Func, bits<3> Opcode, string opcstr> {
  def ""  : SIMT_ALUrr_ReserveShmat<SIMTDstVecReg, SIMTSrcVecReg, Func, Opcode, "v." # opcstr>;
  def "_SCAR" : SIMT_ALUrr_ReserveShmat<SIMTDstScalarReg, SIMTSrcScalarReg, Func, Opcode, "l." # opcstr> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
}

class SIMT_ALUrr_ReserveShmat_CMP_P<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_B<Func, 0b000, Opcode, 0b010,
      (outs ), (ins SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>, Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let Dst = 0b1011100;
  let DstType = 0b000;
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$SrcR" # "$SrcRType, " # "->p";
  let Shamt = 0b00000;
}

multiclass SIMT_ALUrr_CMP_ReserveShmat_VS< bits<3> Func, bits<3> Opcode, string opcstr> {
  def ""  : SIMT_ALUrr_ReserveShmat<SIMTDstVecReg, SIMTSrcVecReg, Func, Opcode, "v." # opcstr>;
  let Defs = [SIMT_P] in
  def "_P"  : SIMT_ALUrr_ReserveShmat_CMP_P<Func, Opcode, "v." # opcstr>;
  def "_SCAR" : SIMT_ALUrr_ReserveShmat<SIMTDstCMPScalarReg, SIMTSrcScalarReg, Func, Opcode, "l." # opcstr> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
}

let Uses = [SIMT_P] in {
  defm SIMT_SRL : SIMT_ALUrr_ReserveShmat_VS<0b101, 0b000, "srl">;
  defm SIMT_SRA : SIMT_ALUrr_ReserveShmat_VS<0b110, 0b000, "sra">;
  defm SIMT_SLL : SIMT_ALUrr_ReserveShmat_VS<0b111, 0b000, "sll">;

  defm SIMT_CMP_EQ  : SIMT_ALUrr_CMP_ReserveShmat_VS<0b000, 0b100, "cmp.eq">;
  defm SIMT_CMP_NE  : SIMT_ALUrr_CMP_ReserveShmat_VS<0b001, 0b100, "cmp.ne">;
  defm SIMT_CMP_AND : SIMT_ALUrr_CMP_ReserveShmat_VS<0b010, 0b100, "cmp.and">;
  defm SIMT_CMP_OR  : SIMT_ALUrr_CMP_ReserveShmat_VS<0b011, 0b100, "cmp.or">;
  defm SIMT_CMP_LT  : SIMT_ALUrr_CMP_ReserveShmat_VS<0b100, 0b100, "cmp.lt">;
  defm SIMT_CMP_GE  : SIMT_ALUrr_CMP_ReserveShmat_VS<0b101, 0b100, "cmp.ge">;
  defm SIMT_CMP_LTU : SIMT_ALUrr_CMP_ReserveShmat_VS<0b110, 0b100, "cmp.ltu">;
  defm SIMT_CMP_GEU : SIMT_ALUrr_CMP_ReserveShmat_VS<0b111, 0b100, "cmp.geu">;
}

multiclass SIMT_CMP_Pat<PatFrag Op, CondCode Cond, V5Inst_SIMT Inst> {
  def : Pat<(i64 (Op i64:$SrcL, i64:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i64 (Op i32:$SrcL, i32:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value))>;
  def : Pat<(i64 (Op i16:$SrcL, i16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value))>;
  def : Pat<(i64 (Op i8:$SrcL, i8:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value))>;
}

multiclass SIMT_CMP_Pat_Swap<PatFrag Op, CondCode Cond, V5Inst_SIMT Inst> {
  def : Pat<(i64 (Op i64:$SrcL, i64:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i64 (Op i32:$SrcL, i32:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value))>;
  def : Pat<(i64 (Op i16:$SrcL, i16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value))>;
  def : Pat<(i64 (Op i8:$SrcL, i8:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value))>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETEQ, SIMT_CMP_EQ>;
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETNE, SIMT_CMP_NE>;
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETLT, SIMT_CMP_LT>;
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETULT, SIMT_CMP_LTU>;
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETGE, SIMT_CMP_GE>;
  defm : SIMT_CMP_Pat<DivergentSetccOp<setcc>, SETUGE, SIMT_CMP_GEU>;

  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETEQ, SIMT_CMP_EQ_SCAR>;
  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETNE, SIMT_CMP_NE_SCAR>;
  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETLT, SIMT_CMP_LT_SCAR>;
  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETULT, SIMT_CMP_LTU_SCAR>;
  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETGE, SIMT_CMP_GE_SCAR>;
  defm : SIMT_CMP_Pat<UniformSetccOp<setcc>, SETUGE, SIMT_CMP_GEU_SCAR>;

  defm : SIMT_CMP_Pat_Swap<DivergentSetccOp<setcc>, SETGT, SIMT_CMP_LT>;
  defm : SIMT_CMP_Pat_Swap<DivergentSetccOp<setcc>, SETUGT, SIMT_CMP_LTU>;
  defm : SIMT_CMP_Pat_Swap<DivergentSetccOp<setcc>, SETLE, SIMT_CMP_GE>;
  defm : SIMT_CMP_Pat_Swap<DivergentSetccOp<setcc>, SETULE, SIMT_CMP_GEU>;

  defm : SIMT_CMP_Pat_Swap<UniformSetccOp<setcc>, SETGT, SIMT_CMP_LT_SCAR>;
  defm : SIMT_CMP_Pat_Swap<UniformSetccOp<setcc>, SETUGT, SIMT_CMP_LTU_SCAR>;
  defm : SIMT_CMP_Pat_Swap<UniformSetccOp<setcc>, SETLE, SIMT_CMP_GE_SCAR>;
  defm : SIMT_CMP_Pat_Swap<UniformSetccOp<setcc>, SETULE, SIMT_CMP_GEU_SCAR>;
}

multiclass SIMT_ALUrr_ReserveShmat_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value),
                  i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value),
                  i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i32 (OpNode i32:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value),
                  i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value),
                  i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i32 (OpNode i32:$SrcL, (i64 (zext i32:$SrcR)))),
            (Inst (i8 SIMT_DstReg_W.Value),
                  i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value),
                  i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value))>;
  def : Pat<(i16 (OpNode i16:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value),
                  i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value),
                  i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i16 (OpNode i16:$SrcL, (i64 (zext i16:$SrcR)))),
            (Inst (i8 SIMT_DstReg_H.Value),
                  i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value),
                  i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value))>;
  def : Pat<(i8 (OpNode i8:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_B.Value),
                  i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value),
                  i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i8 (OpNode i8:$SrcL, (i64 (zext i8:$SrcR)))),
            (Inst (i8 SIMT_DstReg_B.Value),
                  i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value),
                  i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value))>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_ALUrr_ReserveShmat_Pat<srl, SIMT_SRL>;
  defm : SIMT_ALUrr_ReserveShmat_Pat<sra, SIMT_SRA>;
  defm : SIMT_ALUrr_ReserveShmat_Pat<shl, SIMT_SLL>;
}

class SIMT_Loadrr<bits<3> Func, bits<1> Loc, bits<1> Brg, bits<1> Con, string opcstr, string Lane> :
      V5Inst_SIMT_B<Func, {Loc, Brg, Con},  0b000, 0b100, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<0>:$Shamt), opcstr>,
                     Sched<[WriteLD, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t[$SrcL" # "$SrcLType"# Lane # ", $SrcR" # "$SrcRType" # "$Shamt], " # "$Dst" # "$DstType";
  let mayLoad = 1;
}

multiclass SIMT_Loadrr_LG<bits<3> Func, bits<1> Brg, bits<1> Con, string opcstr, string Lane> {
  def "" : SIMT_Loadrr<Func, 0b1, Brg, Con, opcstr # ".local", Lane>;
  def _GLOBAL : SIMT_Loadrr<Func, 0b0, Brg, Con, opcstr, Lane>;
}

multiclass SIMT_Loadrr_Brg<bits<3> Func, bits<1> Con, string opcstr, string Lane> {
  defm "" : SIMT_Loadrr_LG<Func, 0b0, Con, opcstr, Lane>;
  defm _BRG : SIMT_Loadrr_LG<Func, 0b1, Con, opcstr # ".brg", Lane>;
}

class SIMT_Loadrr_Scar<bits<3> Func, bits<3> FuncLo, string opcstr> :
      V5Inst_SIMT_B_Scar<Func, FuncLo, 0b000, 0b100, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<0>:$Shamt), opcstr>,
                     Sched<[WriteLD, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t[$SrcL" # "$SrcLType" # ", $SrcR" # "$SrcRType" # "$Shamt], " # "$Dst" # "$DstType";
  let mayLoad = 1;
}

multiclass SIMT_Loadrr_Scar_LG<bits<3> Func, string opcstr> {
  def "" : SIMT_Loadrr_Scar<Func, 0b100, opcstr # ".local">;
  def _GLOBAL : SIMT_Loadrr_Scar<Func, 0b000, opcstr>;
}

let Uses = [SIMT_P] in {
defm SIMT_LB  : SIMT_Loadrr_Brg<0b000, 0b0, "v.lb", "">;
defm SIMT_LH  : SIMT_Loadrr_Brg<0b001, 0b0, "v.lh", "">;
defm SIMT_LW  : SIMT_Loadrr_Brg<0b010, 0b0, "v.lw", "">;
defm SIMT_LD  : SIMT_Loadrr_Brg<0b011, 0b0, "v.ld", "">;
defm SIMT_LBU : SIMT_Loadrr_Brg<0b100, 0b0, "v.lbu", "">;
defm SIMT_LHU : SIMT_Loadrr_Brg<0b101, 0b0, "v.lhu", "">;
defm SIMT_LWU : SIMT_Loadrr_Brg<0b110, 0b0, "v.lwu", "">;

defm SIMT_Continuous_LB  : SIMT_Loadrr_Brg<0b000, 0b1, "v.lb", ", lc0">;
defm SIMT_Continuous_LH  : SIMT_Loadrr_Brg<0b001, 0b1, "v.lh", ", lc0<<1">;
defm SIMT_Continuous_LW  : SIMT_Loadrr_Brg<0b010, 0b1, "v.lw", ", lc0<<2">;
defm SIMT_Continuous_LD  : SIMT_Loadrr_Brg<0b011, 0b1, "v.ld", ", lc0<<3">;
defm SIMT_Continuous_LBU : SIMT_Loadrr_Brg<0b100, 0b1, "v.lbu", ", lc0">;
defm SIMT_Continuous_LHU : SIMT_Loadrr_Brg<0b101, 0b1, "v.lhu", ", lc0<<1">;
defm SIMT_Continuous_LWU : SIMT_Loadrr_Brg<0b110, 0b1, "v.lwu", ", lc0<<2">;

defm SIMT_LB_SCAR  : SIMT_Loadrr_Scar_LG<0b000, "l.lb">;
defm SIMT_LH_SCAR  : SIMT_Loadrr_Scar_LG<0b001, "l.lh">;
defm SIMT_LW_SCAR  : SIMT_Loadrr_Scar_LG<0b010, "l.lw">;
defm SIMT_LD_SCAR  : SIMT_Loadrr_Scar_LG<0b011, "l.ld">;
defm SIMT_LBU_SCAR : SIMT_Loadrr_Scar_LG<0b100, "l.lbu">;
defm SIMT_LHU_SCAR : SIMT_Loadrr_Scar_LG<0b101, "l.lhu">;
defm SIMT_LWU_SCAR : SIMT_Loadrr_Scar_LG<0b110, "l.lwu">;
}

class LoadAsLocal<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  auto *LoadN = cast<LoadSDNode>(N);
  return LoadN->getPointerInfo().getAddrSpace() == 6;
}]>;

class LoadAsGlobal<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL), (N node:$SrcL), [{
  auto *LoadN = cast<LoadSDNode>(N);
  return LoadN->getPointerInfo().getAddrSpace() != 6;
}]>;

let Predicates = [isSIMT] in {
multiclass SIMT_LD_Pat<PatFrag LoadOp, ValueType Type, int DstType, V5Inst_SIMT Inst> {
  def : Pat<(Type (LoadOp (SIMTLoadADDRrr iPTR:$SrcL, i64:$SrcR, i8:$SrcRType, SIMTShamtImm<0>:$Shamt))),
            (Inst DstType, iPTR:$SrcL, SIMT_Int_SrcReg_SD.Value, i64:$SrcR, i8:$SrcRType, SIMTShamtImm<0>:$Shamt)>;
}

multiclass SIMT_LD_Addrspace_Pat<PatFrag LoadOp, ValueType Type, int DstType, V5Inst_SIMT Inst> {
  defm : SIMT_LD_Pat<LoadAsLocal<LoadOp>, Type, DstType, Inst>;
  defm : SIMT_LD_Pat<LoadAsGlobal<LoadOp>, Type, DstType, !cast<V5Inst_SIMT>(Inst # "_GLOBAL")>;
}

multiclass Continuous_Load_Pat<PatFrag LoadOp, ValueType Type, int scale, int DstType, Instruction Inst> {
  def : Pat<(Type (LoadOp (ContinuousLoadADDRrr<scale> iPTR:$SrcL, i64:$SrcR, i8:$SrcRType, SIMTShamtImm<0>:$Shamt))),
            (Inst DstType, iPTR:$SrcL, SIMT_Int_SrcReg_SD.Value, i64:$SrcR, i8:$SrcRType, SIMTShamtImm<0>:$Shamt)>;
}

multiclass Continuous_Load_Addrspace_Pat<ValueType Type, int scale, int DstType, Instruction Inst> {
  defm : Continuous_Load_Pat<LoadAsLocal<load>, Type, scale, DstType, Inst>;
  defm : Continuous_Load_Pat<LoadAsGlobal<load>, Type, scale, DstType, !cast<V5Inst_SIMT>(Inst # "_GLOBAL")>;
}
}

defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LW>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LB>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LB>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<sextloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LB>;

defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LW>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LB>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LB>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<extloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LB>;

defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LWU>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LHU>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LHU>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LBU>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LBU>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<zextloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LBU>;

defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, i64, SIMT_DstReg_D.Value, SIMT_LD>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, i32, SIMT_DstReg_W.Value, SIMT_LW>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, i16, SIMT_DstReg_H.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, i8,  SIMT_DstReg_B.Value, SIMT_LB>;

defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, f64,  SIMT_DstReg_D.Value, SIMT_LD>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, f32,  SIMT_DstReg_W.Value, SIMT_LW>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, f16,  SIMT_DstReg_H.Value, SIMT_LH>;
defm : SIMT_LD_Addrspace_Pat<DivergentUnaryOp<load>, bf16, SIMT_DstReg_H.Value, SIMT_LH>;

defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LW_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LB_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LB_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<sextloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LB_SCAR>;

defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LW_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LB_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LB_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<extloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LB_SCAR>;

defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi32>, i64, SIMT_DstReg_D.Value, SIMT_LWU_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi16>, i64, SIMT_DstReg_D.Value, SIMT_LHU_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi16>, i32, SIMT_DstReg_W.Value, SIMT_LHU_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi8>,  i64, SIMT_DstReg_D.Value, SIMT_LBU_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi8>,  i32, SIMT_DstReg_W.Value, SIMT_LBU_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<zextloadi8>,  i16, SIMT_DstReg_H.Value, SIMT_LBU_SCAR>;

defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, i64, SIMT_DstReg_D.Value, SIMT_LD_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, i32, SIMT_DstReg_W.Value, SIMT_LW_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, i16, SIMT_DstReg_H.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, i8,  SIMT_DstReg_B.Value, SIMT_LB_SCAR>;

defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, f64,  SIMT_DstReg_D.Value, SIMT_LD_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, f32,  SIMT_DstReg_W.Value, SIMT_LW_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, f16,  SIMT_DstReg_H.Value, SIMT_LH_SCAR>;
defm : SIMT_LD_Addrspace_Pat<UniformUnaryOp<load>, bf16, SIMT_DstReg_H.Value, SIMT_LH_SCAR>;

defm : Continuous_Load_Addrspace_Pat<i64, 3, SIMT_DstReg_D.Value, SIMT_Continuous_LD>;
defm : Continuous_Load_Addrspace_Pat<i32, 2, SIMT_DstReg_W.Value, SIMT_Continuous_LW>;
defm : Continuous_Load_Addrspace_Pat<i16, 1, SIMT_DstReg_H.Value, SIMT_Continuous_LH>;
defm : Continuous_Load_Addrspace_Pat<i8,  0, SIMT_DstReg_B.Value, SIMT_Continuous_LB>;

defm : Continuous_Load_Addrspace_Pat<f64,  3, SIMT_DstReg_D.Value, SIMT_Continuous_LD>;
defm : Continuous_Load_Addrspace_Pat<f32,  2, SIMT_DstReg_W.Value, SIMT_Continuous_LW>;
defm : Continuous_Load_Addrspace_Pat<f16,  1, SIMT_DstReg_H.Value, SIMT_Continuous_LH>;
defm : Continuous_Load_Addrspace_Pat<bf16, 1, SIMT_DstReg_B.Value, SIMT_Continuous_LH>;

class SIMT_Reduce<bits<3> funcHi, bits<3> funcLo, string opcstr> :
      V5Inst_SIMT_F_RD<funcHi, funcLo, 0b000, 0b110, (outs SIMTDstRegReduce:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Dst" # "$DstType";
}

multiclass SIMT_Reduce<bits<3> FuncHi, bits<3> FuncLo, string opcstr> {
  let Format = InstFormat_REDUCE in
  def "" : SIMT_Reduce<FuncHi, FuncLo, opcstr>, P2I<NAME, 1>;
  let hasSideEffects = 0, mayLoad = 0, mayStore = 0, usesCustomInserter = 1, Format = InstFormat_REDUCE in
  def _Pseudo : Pseudo<(outs),
      (ins SIMTDstRegReduce:$Dst, SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType)>, P2I<NAME, 0>;
}

let Uses = [SIMT_P] in {
defm SIMT_RDADD  : SIMT_Reduce<0b000, 0b000, "v.rdadd">;
defm SIMT_RDAND  : SIMT_Reduce<0b001, 0b000, "v.rdand">;
defm SIMT_RDOR   : SIMT_Reduce<0b010, 0b000, "v.rdor">;
defm SIMT_RDXOR  : SIMT_Reduce<0b011, 0b000, "v.rdxor">;

defm SIMT_RDMAX  : SIMT_Reduce<0b000, 0b001, "v.rdmax">;
defm SIMT_RDMIN  : SIMT_Reduce<0b001, 0b001, "v.rdmin">;
}

class SIMT_Reduce_F<bits<3> funcHi, bits<3> funcLo, string opcstr> :
      V5Inst_SIMT_F_RD<funcHi, funcLo, 0b000, 0b110, (outs SIMTDstRegReduce:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Dst" # "$DstType";
}

multiclass SIMT_Reduce_F<bits<3> FuncHi, bits<3> FuncLo, string opcstr> {
  let Format = InstFormat_REDUCE in
  def "" : SIMT_Reduce_F<FuncHi, FuncLo, opcstr>, P2I<NAME, 1>;
  let hasSideEffects = 0, mayLoad = 0, mayStore = 0, usesCustomInserter = 1, Format = InstFormat_REDUCE in
  def _Pseudo : Pseudo<(outs),
      (ins SIMTDstRegReduce:$Dst, SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType)>, P2I<NAME, 0>;
}

let Uses = [SIMT_P] in {
defm SIMT_RDFADD : SIMT_Reduce_F<0b100, 0b000, "v.rdfadd">;
defm SIMT_RDFMAX : SIMT_Reduce_F<0b010, 0b001, "v.rdfmax">;
defm SIMT_RDFMIN : SIMT_Reduce_F<0b011, 0b001, "v.rdfmin">;
}

multiclass V5_FReduce_Pat<SDNode N, Instruction I> {
  def : Pat<(f64 (N f64:$Src)),  (I SIMT_DstReg_D.Value, f64:$Src,  SIMT_Float_SrcReg_FD.Value)>;
  def : Pat<(f32 (N f32:$Src)),  (I SIMT_DstReg_D.Value, f32:$Src,  SIMT_Float_SrcReg_FS.Value)>;
  def : Pat<(f16 (N f16:$Src)),  (I SIMT_DstReg_D.Value, f16:$Src,  SIMT_Float_SrcReg_FH.Value)>;
  def : Pat<(bf16 (N bf16:$Src)), (I SIMT_DstReg_D.Value, bf16:$Src, SIMT_Float_SrcReg_BF.Value)>;
}

multiclass V5_IReduce_Pat<SDNode N, Instruction I> {
  def : Pat<(i64 (N i64:$Src)), (I SIMT_DstReg_D.Value, i64:$Src, SIMT_Int_SrcReg_SD.Value)>;
  def : Pat<(i32 (N i32:$Src)), (I SIMT_DstReg_D.Value, i32:$Src, SIMT_Int_SrcReg_SW.Value)>;
  def : Pat<(i16 (N i16:$Src)), (I SIMT_DstReg_D.Value, i16:$Src, SIMT_Int_SrcReg_SH.Value)>;
  def : Pat<(i8 (N i8:$Src)),  (I SIMT_DstReg_D.Value, i8:$Src,  SIMT_Int_SrcReg_SB.Value)>;
}

defm : V5_FReduce_Pat<linxv5_rdadd, SIMT_RDFADD>;
defm : V5_FReduce_Pat<linxv5_rdmax, SIMT_RDFMAX>;
defm : V5_FReduce_Pat<linxv5_rdmin, SIMT_RDFMIN>;

defm : V5_IReduce_Pat<linxv5_rdadd, SIMT_RDADD>;
defm : V5_IReduce_Pat<linxv5_rdor,  SIMT_RDOR>;
defm : V5_IReduce_Pat<linxv5_rdmax, SIMT_RDMAX>;
defm : V5_IReduce_Pat<linxv5_rdmin, SIMT_RDMIN>;

class SIMT_ALUarithri<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F<Func, 0b000, Opcode, 0b010, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm12), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imm12, " # "$Dst" # "$DstType";
}

class SIMT_ALUarithri_Scar<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F_Scar<Func, 0b000, Opcode, 0b010, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm12), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imm12, " # "$Dst" # "$DstType";
}

class SIMT_ALUarithri_Scar_P<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F_Scar_P<Func, 0b000, Opcode, 0b010, (outs SIMTDstScalarRegP:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm12), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imm12, " # "$Dst" # "$DstType";
}

class SIMT_ALUshiftri<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_Shiftri<Func, Opcode, 0b010, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6:$Shamt), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Shamt, " # "$Dst" # "$DstType";
}

class SIMT_ALUshiftri_Scar<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_Shiftri_Scar<Func, Opcode, 0b010, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6:$Shamt), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Shamt, " # "$Dst" # "$DstType";
}

class SIMT_ALUarithri_CMP<RegisterOperand DstRegType, RegisterOperand SrcRegType, bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F<Func, 0b000, Opcode, 0b010, (outs DstRegType:$Dst),
                    (ins SIMTDstRegType:$DstType, SrcRegType:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm12), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imm12, " # "$Dst" # "$DstType";
}

class SIMT_ALUarithri_CMP_P<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F<Func, 0b000, Opcode, 0b010, (outs ),
                    (ins SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm12), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let Dst = 0b1011100;
  let DstType = 0b000;
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imm12, " # "->p";
}

multiclass SIMT_ALUarithri_CMP_VS<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> {
  def ""  : SIMT_ALUarithri_CMP<SIMTDstVecReg, SIMTSrcVecReg, Func, Opcode, ImmOp, "v." # opcstr>;
  let Defs = [SIMT_P] in
  def "_P"  : SIMT_ALUarithri_CMP_P<Func, Opcode, ImmOp, "v." # opcstr>;
  def "_SCAR" : SIMT_ALUarithri_CMP<SIMTDstCMPScalarReg, SIMTSrcScalarReg, Func, Opcode, ImmOp, "l." # opcstr> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
}

let Format = InstFormat_ADDI, Uses = [SIMT_P] in {
def SIMT_ADDI : SIMT_ALUarithri<0b000, 0b001, uimm12, "v.addi">;
def SIMT_SUBI : SIMT_ALUarithri<0b001, 0b001, uimm12, "v.subi">;

def SIMT_ADDI_SCAR : SIMT_ALUarithri_Scar<0b000, 0b001, uimm12, "l.addi">;
def SIMT_SUBI_SCAR : SIMT_ALUarithri_Scar<0b001, 0b001, uimm12, "l.subi">;

def SIMT_ADDI_SCAR_P : SIMT_ALUarithri_Scar_P<0b000, 0b001, uimm12, "l.addi">;
}

class PatGprImm<SDPatternOperator OpNode, V5Inst_SIMT Inst, ValueType Type, Operand ImmOp, int DstType, int RType>
    : Pat<(Type (OpNode Type:$SrcL, ImmOp:$imm)),
          (Inst DstType, Type:$SrcL, RType, ImmOp:$imm)>;
multiclass PatGprUimm12<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : PatGprImm<OpNode, Inst, i64, uimm12, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value>;
  def : PatGprImm<OpNode, Inst, i32, uimm12, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value>;
  def : PatGprImm<OpNode, Inst, i16, uimm12, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value>;
  def : PatGprImm<OpNode, Inst, i8, uimm12, SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value>;
}

let Uses = [SIMT_P] in {
def SIMT_ANDI : SIMT_ALUarithri<0b010, 0b001, simm12, "v.andi">;
def SIMT_ORI : SIMT_ALUarithri<0b011, 0b001, simm12, "v.ori">;
def SIMT_XORI : SIMT_ALUarithri<0b100, 0b001, simm12, "v.xori">;

def SIMT_ANDI_SCAR : SIMT_ALUarithri_Scar<0b010, 0b001, simm12, "l.andi">;
def SIMT_ORI_SCAR : SIMT_ALUarithri_Scar<0b011, 0b001, simm12, "l.ori">;
def SIMT_XORI_SCAR : SIMT_ALUarithri_Scar<0b100, 0b001, simm12, "l.xori">;


def SIMT_ORI_SCAR_P : SIMT_ALUarithri_Scar_P<0b011, 0b001, simm12, "l.ori">;
def SIMT_XORI_SCAR_P : SIMT_ALUarithri_Scar_P<0b100, 0b001, simm12, "l.xori">;

defm SIMT_CMP_EQI : SIMT_ALUarithri_CMP_VS<0b000, 0b101, simm12, "cmp.eqi">;
defm SIMT_CMP_NEI : SIMT_ALUarithri_CMP_VS<0b001, 0b101, simm12, "cmp.nei">;
defm SIMT_CMP_ANDI : SIMT_ALUarithri_CMP_VS<0b010, 0b101, simm12, "cmp.andi">;
defm SIMT_CMP_ORI : SIMT_ALUarithri_CMP_VS<0b011, 0b101, simm12, "cmp.ori">;
defm SIMT_CMP_LTI : SIMT_ALUarithri_CMP_VS<0b100, 0b101, simm12, "cmp.lti">;
defm SIMT_CMP_GEI : SIMT_ALUarithri_CMP_VS<0b101, 0b101, simm12, "cmp.gei">;
defm SIMT_CMP_LTUI : SIMT_ALUarithri_CMP_VS<0b110, 0b101, uimm12, "cmp.ltui">;
defm SIMT_CMP_GEUI : SIMT_ALUarithri_CMP_VS<0b111, 0b101, uimm12, "cmp.geui">;
}

multiclass PatGprSimm12<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : PatGprImm<OpNode, Inst, i64, simm12, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value>;
  def : PatGprImm<OpNode, Inst, i32, simm12, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value>;
  def : PatGprImm<OpNode, Inst, i16, simm12, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value>;
  def : PatGprImm<OpNode, Inst, i8, simm12, SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value>;
}

multiclass SIMT_CMPI_Pat_impl<SDPatternOperator OP, CondCode Cond, V5Inst_SIMT Inst, Operand ImmOp> {
  def : Pat<(i64 (OP i64:$SrcL, ImmOp:$imm, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), ImmOp:$imm)>;
  def : Pat<(i64 (OP i32:$SrcL, ImmOp:$imm, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), ImmOp:$imm)>;
  def : Pat<(i64 (OP i16:$SrcL, ImmOp:$imm, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), ImmOp:$imm)>;
  def : Pat<(i64 (OP i8:$SrcL, ImmOp:$imm, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), ImmOp:$imm)>;
}

multiclass SIMT_CMPI_Pat<CondCode Cond, V5Inst_SIMT Inst, Operand ImmOp> {
  defm : SIMT_CMPI_Pat_impl<DivergentTernaryOp<setcc>, Cond, Inst, ImmOp>;
  defm : SIMT_CMPI_Pat_impl<UniformTernaryOp<setcc>, Cond, !cast<V5Inst_SIMT>(Inst # "_SCAR"), ImmOp>;
}

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, usesCustomInserter = 1, Defs = [SIMT_P], Uses = [SIMT_P] in
def LinxV5PseudoMergePred : Pseudo<(outs ), (ins SIMTSrcVecReg:$SrcL), []>;

let mayLoad = 0, mayStore = 0, Uses = [SIMT_P] in
def LinxV5PseudoCopyFromP : Pseudo<(outs SIMTDstScalarReg:$Dst), (ins ), []>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0, Defs = [SIMT_P], Uses = [SIMT_P] in
def LinxV5PseudoCopy2P : Pseudo<(outs), (ins SIMTSrcScalarReg:$SrcL), []>;

let mayLoad = 0, mayStore = 0, Uses = [SIMT_P] in
def LinxV5ImplicitDef : Pseudo<(outs SIMTDstVecReg:$Dst), (ins SIMTDstRegType:$DstType), []>;
let mayLoad = 0, mayStore = 0, Uses = [SIMT_P] in
def LinxV5ImplicitSDef : Pseudo<(outs SIMTDstScalarReg:$Dst), (ins SIMTDstRegType:$DstType), []>;

let mayLoad = 0, mayStore = 1, isCodeGenOnly = 1, Uses = [SIMT_P] in
def PseudoVecSpill : Pseudo<(outs), (ins SIMTSrcReg:$SrcD, uimm5:$width, simm12:$fi, simm12:$mop), []>;

let mayLoad = 1, mayStore = 0, isCodeGenOnly = 1, Uses = [SIMT_P] in
def PseudoVecReload : Pseudo<(outs SIMTDstReg:$SrcD), (ins uimm5:$width, simm12:$fi, simm12:$mop), []>;

let Predicates = [isSIMT] in {
  def : Pat<(linxv5_merge_pred i64:$Src), (LinxV5PseudoMergePred i64:$Src)>;
  def : Pat<(i64 (linxv5_copy_p)), (i64 (LinxV5PseudoCopyFromP))>;
  def : Pat<(linxv5_copy2_p i64:$SrcL), (LinxV5PseudoCopy2P i64:$SrcL)>;
  def : Pat<(i64 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_D.Value))>;
  def : Pat<(i32 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_W.Value))>;
  def : Pat<(i16 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_H.Value))>;
  def : Pat<(i8  (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_B.Value))>;
  def : Pat<(f64 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_D.Value))>;
  def : Pat<(f32 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_W.Value))>;
  def : Pat<(f16 (Divergent<linxv5_implicit_def>)), (LinxV5ImplicitDef (i8 SIMT_DstReg_H.Value))>;

  def : Pat<(i64 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_D.Value))>;
  def : Pat<(i32 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_W.Value))>;
  def : Pat<(i16 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_H.Value))>;
  def : Pat<(i8  (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_B.Value))>;
  def : Pat<(f64 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_D.Value))>;
  def : Pat<(f32 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_W.Value))>;
  def : Pat<(f16 (Uniform<linxv5_implicit_def>)), (LinxV5ImplicitSDef (i8 SIMT_DstReg_H.Value))>;
}

let Predicates = [isSIMT] in {
  defm : PatGprUimm12<DivergentOp<add>, SIMT_ADDI>;
  defm : PatGprUimm12<DivergentOp<sub>, SIMT_SUBI>;
  defm : PatGprSimm12<DivergentOp<and>, SIMT_ANDI>;
  defm : PatGprSimm12<DivergentOp<or>, SIMT_ORI>;
  defm : PatGprSimm12<DivergentOp<xor>, SIMT_XORI>;

  defm : PatGprUimm12<UniformOp<add>, SIMT_ADDI_SCAR>;
  defm : PatGprUimm12<UniformOp<sub>, SIMT_SUBI_SCAR>;
  defm : PatGprSimm12<UniformOp<and>, SIMT_ANDI_SCAR>;
  defm : PatGprSimm12<UniformOp<or>, SIMT_ORI_SCAR>;
  defm : PatGprSimm12<UniformOp<xor>, SIMT_XORI_SCAR>;

  defm : SIMT_CMPI_Pat<SETEQ, SIMT_CMP_EQI, simm12>;
  defm : SIMT_CMPI_Pat<SETNE, SIMT_CMP_NEI, simm12>;
  defm : SIMT_CMPI_Pat<SETLT, SIMT_CMP_LTI, simm12>;
  defm : SIMT_CMPI_Pat<SETULT, SIMT_CMP_LTUI, uimm12>;
  defm : SIMT_CMPI_Pat<SETGE, SIMT_CMP_GEI, simm12>;
  defm : SIMT_CMPI_Pat<SETUGE, SIMT_CMP_GEUI, uimm12>;
}

let Uses = [SIMT_P] in {
def SIMT_SRLI : SIMT_ALUshiftri<0b101, 0b001, "v.srli">;
def SIMT_SRAI : SIMT_ALUshiftri<0b110, 0b001, "v.srai">;
def SIMT_SLLI : SIMT_ALUshiftri<0b111, 0b001, "v.slli">;

def SIMT_SRLI_SCAR : SIMT_ALUshiftri_Scar<0b101, 0b001, "l.srli">;
def SIMT_SRAI_SCAR : SIMT_ALUshiftri_Scar<0b110, 0b001, "l.srai">;
def SIMT_SLLI_SCAR : SIMT_ALUshiftri_Scar<0b111, 0b001, "l.slli">;
}

multiclass SIMT_ALUrrI_ReserveShmat_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode i64:$SrcL, uimm6:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value),
                  i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value),
                  uimm6:$SrcR)>;
  def : Pat<(i32 (OpNode i32:$SrcL, uimm6:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value),
                  i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value),
                  uimm6:$SrcR)>;
  def : Pat<(i16 (OpNode i16:$SrcL, uimm6:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value),
                  i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value),
                  uimm6:$SrcR)>;
  def : Pat<(i8 (OpNode i8:$SrcL, uimm6:$SrcR)),
            (Inst (i8 SIMT_DstReg_B.Value),
                  i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value),
                  uimm6:$SrcR)>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_ALUrrI_ReserveShmat_Pat<DivergentOp<srl>, SIMT_SRLI>;
  defm : SIMT_ALUrrI_ReserveShmat_Pat<DivergentOp<sra>, SIMT_SRAI>;
  defm : SIMT_ALUrrI_ReserveShmat_Pat<DivergentOp<shl>, SIMT_SLLI>;

  defm : SIMT_ALUrrI_ReserveShmat_Pat<UniformOp<srl>, SIMT_SRLI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_Pat<UniformOp<sra>, SIMT_SRAI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_Pat<UniformOp<shl>, SIMT_SLLI_SCAR>;
}

class SIMT_BIT<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_BIT<Func, Opcode, 0b011, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6:$Imms, uimm6_plus1:$Imml), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imms, $Imml, " # "$Dst" # "$DstType";
}

class SIMT_REV<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_BIT<Func, Opcode, 0b011, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6_revPlus1:$Imms, uimm6_rev:$Imml), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imms, $Imml, " # "$Dst" # "$DstType";
}

class SIMT_BIT_Scar<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_BIT_Scar<Func, Opcode, 0b011, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6:$Imms, uimm6_plus1:$Imml), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imms, $Imml, " # "$Dst" # "$DstType";
}

class SIMT_REV_Scar<bits<3> Func, bits<3> Opcode, string opcstr> :
      V5Inst_SIMT_F_BIT_Scar<Func, Opcode, 0b011, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, uimm6_revPlus1:$Imms, uimm6_rev:$Imml), opcstr>,
                     Sched<[WriteDafultA, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "$Imms, $Imml, " # "$Dst" # "$DstType";
}

let Uses = [SIMT_P] in {
def SIMT_BXS : SIMT_BIT<0b000, 0b110, "v.bxs">;
def SIMT_BXU : SIMT_BIT<0b001, 0b110, "v.bxu">;
def SIMT_BIC : SIMT_BIT<0b010, 0b110, "v.bic">;
def SIMT_BIS : SIMT_BIT<0b011, 0b110, "v.bis">;
def SIMT_CTZ : SIMT_BIT<0b100, 0b110, "v.ctz">;
def SIMT_CLZ : SIMT_BIT<0b101, 0b110, "v.clz">;
def SIMT_BCNT: SIMT_BIT<0b110, 0b110, "v.bcnt">;
def SIMT_REV : SIMT_REV<0b111, 0b110, "v.rev">;

def SIMT_BXS_SCAR : SIMT_BIT_Scar<0b000, 0b110, "l.bxs">;
def SIMT_BXU_SCAR : SIMT_BIT_Scar<0b001, 0b110, "l.bxu">;
def SIMT_BIC_SCAR : SIMT_BIT_Scar<0b010, 0b110, "l.bic">;
def SIMT_BIS_SCAR : SIMT_BIT_Scar<0b011, 0b110, "l.bis">;
def SIMT_CTZ_SCAR : SIMT_BIT_Scar<0b100, 0b110, "l.ctz">;
def SIMT_CLZ_SCAR : SIMT_BIT_Scar<0b101, 0b110, "l.clz">;
def SIMT_BCNT_SCAR: SIMT_BIT_Scar<0b110, 0b110, "l.bcnt">;
def SIMT_REV_SCAR : SIMT_REV_Scar<0b111, 0b110, "l.rev">;
}

let Predicates = [isSIMT] in {
def : Pat<(i64 (DivergentUnaryOp<sw_ext> i64:$Src)), (SIMT_BXS (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 32)>;
def : Pat<(i64 (DivergentUnaryOp<sh_ext> i64:$Src)), (SIMT_BXS (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 16)>;
def : Pat<(i32 (DivergentUnaryOp<sh_ext> i32:$Src)), (SIMT_BXS (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0, 16)>;
def : Pat<(i64 (DivergentUnaryOp<sb_ext> i64:$Src)), (SIMT_BXS (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0,  8)>;
def : Pat<(i32 (DivergentUnaryOp<sb_ext> i32:$Src)), (SIMT_BXS (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0,  8)>;
def : Pat<(i16 (DivergentUnaryOp<sb_ext> i16:$Src)), (SIMT_BXS (i8 SIMT_DstReg_H.Value), i16:$Src, (i8 SIMT_Int_SrcReg_SH.Value), 0,  8)>;

def : Pat<(i64 (DivergentUnaryOp<uw_ext> i64:$Src)), (SIMT_BXU (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 32)>;
def : Pat<(i64 (DivergentUnaryOp<uh_ext> i64:$Src)), (SIMT_BXU (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 16)>;
def : Pat<(i32 (DivergentUnaryOp<uh_ext> i32:$Src)), (SIMT_BXU (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0, 16)>;
def : Pat<(i64 (DivergentUnaryOp<ub_ext> i64:$Src)), (SIMT_BXU (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0,  8)>;
def : Pat<(i32 (DivergentUnaryOp<ub_ext> i32:$Src)), (SIMT_BXU (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0,  8)>;
def : Pat<(i16 (DivergentUnaryOp<ub_ext> i16:$Src)), (SIMT_BXU (i8 SIMT_DstReg_H.Value), i16:$Src, (i8 SIMT_Int_SrcReg_SH.Value), 0,  8)>;

def : Pat<(i64 (UniformUnaryOp<sw_ext> i64:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 32)>;
def : Pat<(i64 (UniformUnaryOp<sh_ext> i64:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 16)>;
def : Pat<(i32 (UniformUnaryOp<sh_ext> i32:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0, 16)>;
def : Pat<(i64 (UniformUnaryOp<sb_ext> i64:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0,  8)>;
def : Pat<(i32 (UniformUnaryOp<sb_ext> i32:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0,  8)>;
def : Pat<(i16 (UniformUnaryOp<sb_ext> i16:$Src)), (SIMT_BXS_SCAR (i8 SIMT_DstReg_H.Value), i16:$Src, (i8 SIMT_Int_SrcReg_SH.Value), 0,  8)>;

def : Pat<(i64 (UniformUnaryOp<uw_ext> i64:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 32)>;
def : Pat<(i64 (UniformUnaryOp<uh_ext> i64:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0, 16)>;
def : Pat<(i32 (UniformUnaryOp<uh_ext> i32:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0, 16)>;
def : Pat<(i64 (UniformUnaryOp<ub_ext> i64:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_D.Value), i64:$Src, (i8 SIMT_Int_SrcReg_SD.Value), 0,  8)>;
def : Pat<(i32 (UniformUnaryOp<ub_ext> i32:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_W.Value), i32:$Src, (i8 SIMT_Int_SrcReg_SW.Value), 0,  8)>;
def : Pat<(i16 (UniformUnaryOp<ub_ext> i16:$Src)), (SIMT_BXU_SCAR (i8 SIMT_DstReg_H.Value), i16:$Src, (i8 SIMT_Int_SrcReg_SH.Value), 0,  8)>;
}

class SIMT_Loadi_Lane<bits<3> Func, bits<1> Loc, bits<1> Brg, bits<1> Con, bits<3> Opcode, Operand ImmOp, string opcstr, string Lane> :
      V5Inst_SIMT_F_LDSD<Func, {Loc, Brg, Con}, Opcode, 0b100, (outs SIMTDstVecReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm24), opcstr>,
                     Sched<[WriteLD, ReadDafult]> {
  let AsmString = opcstr # "\t[$SrcL" # "$SrcLType"# Lane # ", $Imm24], " # "$Dst" # "$DstType";
  let mayLoad = 1;
}

multiclass SIMT_Loadi_Lane_LG<bits<3> Func, bits<1> Brg, bits<1> Con, bits<3> Opcode, Operand ImmOp, string opcstr, string Lane> {
  def "" : SIMT_Loadi_Lane<Func, 0b1, Brg, Con, Opcode, ImmOp, opcstr # ".local", Lane>;
  def _GLOBAL : SIMT_Loadi_Lane<Func, 0b0, Brg, Con, Opcode, ImmOp, opcstr, Lane>;
}

multiclass SIMT_Loadi_Lane_LG_Brg<bits<3> Func, bits<1> Con, bits<3> Opcode, Operand ImmOp, string opcstr, string Lane> {
  defm "" : SIMT_Loadi_Lane_LG<Func, 0b0, Con, Opcode, ImmOp, opcstr, Lane>;
  defm _BRG : SIMT_Loadi_Lane_LG<Func, 0b1, Con, Opcode, ImmOp, opcstr # ".brg", Lane>;
}

class SIMT_Loadi_Lane_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_F_Load_Scar<Func, FuncLo, Opcode, 0b100, (outs SIMTDstScalarReg:$Dst),
                    (ins SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, ImmOp:$Imm24), opcstr>,
                     Sched<[WriteLD, ReadDafult]> {
  let AsmString = opcstr # "\t[$SrcL" # "$SrcLType" # ", $Imm24], " # "$Dst" # "$DstType";
  let mayLoad = 1;
}

multiclass SIMT_Loadi_Lane_Scar_LG<bits<3> Func, bits<3> Opcode, Operand ImmOp, string opcstr> {
  def "" : SIMT_Loadi_Lane_Scar<Func, 0b100, Opcode, ImmOp, opcstr # ".local">;
  def _GLOBAL : SIMT_Loadi_Lane_Scar<Func, 0b000, Opcode, ImmOp, opcstr>;
}

let Uses = [SIMT_P] in {
defm SIMT_LHI : SIMT_Loadi_Lane_LG_Brg<0b001, 0b0, 0b001, simm_pshl<24, 1>, "v.lhi", "">;
defm SIMT_LWI : SIMT_Loadi_Lane_LG_Brg<0b010, 0b0, 0b001, simm_pshl<24, 2>, "v.lwi", "">;
defm SIMT_LDI : SIMT_Loadi_Lane_LG_Brg<0b011, 0b0, 0b001, simm_pshl<24, 3>, "v.ldi", "">;
defm SIMT_LBUI : SIMT_Loadi_Lane_LG_Brg<0b100, 0b0, 0b001, simm24i64, "v.lbui", "">;
defm SIMT_LHUI : SIMT_Loadi_Lane_LG_Brg<0b101, 0b0, 0b001, simm_pshl<24, 1>, "v.lhui", "">;
defm SIMT_LWUI : SIMT_Loadi_Lane_LG_Brg<0b110, 0b0, 0b001, simm_pshl<24, 2>, "v.lwui", "">;

defm SIMT_LHI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b001, 0b001, simm_pshl<24, 1>, "l.lhi">;
defm SIMT_LWI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b010, 0b001, simm_pshl<24, 2>, "l.lwi">;
defm SIMT_LDI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b011, 0b001, simm_pshl<24, 3>, "l.ldi">;
defm SIMT_LBUI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b100, 0b001, simm24i64, "l.lbui">;
defm SIMT_LHUI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b101, 0b001, simm_pshl<24, 1>, "l.lhui">;
defm SIMT_LWUI_SCAR : SIMT_Loadi_Lane_Scar_LG<0b110, 0b001, simm_pshl<24, 2>, "l.lwui">;

defm SIMT_Continuous_LHI : SIMT_Loadi_Lane_LG_Brg<0b001, 0b1, 0b001, simm_pshl<24, 1>, "v.lhi", ", lc0<<1">;
defm SIMT_Continuous_LWI : SIMT_Loadi_Lane_LG_Brg<0b010, 0b1, 0b001, simm_pshl<24, 2>, "v.lwi", ", lc0<<2">;
defm SIMT_Continuous_LDI : SIMT_Loadi_Lane_LG_Brg<0b011, 0b1, 0b001, simm_pshl<24, 3>, "v.ldi", ", lc0<<3">;
defm SIMT_Continuous_LBUI : SIMT_Loadi_Lane_LG_Brg<0b100, 0b1, 0b001, simm24i64, "v.lbui", ", lc0">;
defm SIMT_Continuous_LHUI : SIMT_Loadi_Lane_LG_Brg<0b101, 0b1, 0b001, simm_pshl<24, 1>, "v.lhui", ", lc0<<1">;
defm SIMT_Continuous_LWUI : SIMT_Loadi_Lane_LG_Brg<0b110, 0b1, 0b001, simm_pshl<24, 2>, "v.lwui", ", lc0<<2">;
}

let Format = InstFormat_LoadI_UnScaled, Uses = [SIMT_P] in {
  defm SIMT_LBI    : SIMT_Loadi_Lane_LG_Brg<0b000, 0b0, 0b001, simm24i64, "v.lbi", "">;
  defm SIMT_LHI_U  : SIMT_Loadi_Lane_LG_Brg<0b001, 0b0, 0b010, simm24i64, "v.lhi.u", "">;
  defm SIMT_LWI_U  : SIMT_Loadi_Lane_LG_Brg<0b010, 0b0, 0b010, simm24i64, "v.lwi.u", "">;
  defm SIMT_LDI_U  : SIMT_Loadi_Lane_LG_Brg<0b011, 0b0, 0b010, simm24i64, "v.ldi.u", "">;
  defm SIMT_LHUI_U : SIMT_Loadi_Lane_LG_Brg<0b101, 0b0, 0b010, simm24i64, "v.lhui.u", "">;
  defm SIMT_LWUI_U : SIMT_Loadi_Lane_LG_Brg<0b110, 0b0, 0b010, simm24i64, "v.lwui.u", "">;

  defm SIMT_LBI_SCAR    : SIMT_Loadi_Lane_Scar_LG<0b000, 0b001, simm24i64, "l.lbi">;
  defm SIMT_LHI_SCAR_U  : SIMT_Loadi_Lane_Scar_LG<0b001, 0b010, simm24i64, "l.lhi.u">;
  defm SIMT_LWI_SCAR_U  : SIMT_Loadi_Lane_Scar_LG<0b010, 0b010, simm24i64, "l.lwi.u">;
  defm SIMT_LDI_SCAR_U  : SIMT_Loadi_Lane_Scar_LG<0b011, 0b010, simm24i64, "l.ldi.u">;
  defm SIMT_LHUI_SCAR_U : SIMT_Loadi_Lane_Scar_LG<0b101, 0b010, simm24i64, "l.lhui.u">;
  defm SIMT_LWUI_SCAR_U : SIMT_Loadi_Lane_Scar_LG<0b110, 0b010, simm24i64, "l.lwui.u">;

  defm SIMT_Continuous_LBI    : SIMT_Loadi_Lane_LG_Brg<0b000, 0b1, 0b001, simm24i64, "v.lbi", ", lc0">;
  defm SIMT_Continuous_LHI_U  : SIMT_Loadi_Lane_LG_Brg<0b001, 0b1, 0b010, simm24i64, "v.lhi.u", ", lc0<<1">;
  defm SIMT_Continuous_LWI_U  : SIMT_Loadi_Lane_LG_Brg<0b010, 0b1, 0b010, simm24i64, "v.lwi.u", ", lc0<<2">;
  defm SIMT_Continuous_LDI_U  : SIMT_Loadi_Lane_LG_Brg<0b011, 0b1, 0b010, simm24i64, "v.ldi.u", ", lc0<<3">;
  defm SIMT_Continuous_LHUI_U : SIMT_Loadi_Lane_LG_Brg<0b101, 0b1, 0b010, simm24i64, "v.lhui.u", ", lc0<<1">;
  defm SIMT_Continuous_LWUI_U : SIMT_Loadi_Lane_LG_Brg<0b110, 0b1, 0b010, simm24i64, "v.lwui.u", ", lc0<<2">;
}

let Predicates = [isSIMT] in {
class LOADri_scaled_Pat<PatFrag Ld, ValueType vt, int shift, int DstType, V5Inst_SIMT I> :
  Pat<(vt (Ld (SIMTADDRri_scaled<shift> i64:$SrcL, simm_pshl<24, shift>:$Imm))),
      (I DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm_pshl<24, shift>:$Imm)>;
class LOADri_unscaled_Pat<PatFrag Ld, ValueType vt, int DstType, V5Inst_SIMT I> :
  Pat<(vt (Ld (SIMTADDRri_unscaled i64:$SrcL, simm24i64:$Imm))),
      (I DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm24i64:$Imm)>;

multiclass LOADri_unscaled_Addrspace_Pat<PatFrag Ld, ValueType vt, int DstType, V5Inst_SIMT I> {
  def : LOADri_unscaled_Pat<LoadAsLocal<Ld>, vt, DstType, I>;
  def : LOADri_unscaled_Pat<LoadAsGlobal<Ld>, vt, DstType, !cast<V5Inst_SIMT>(I # "_GLOBAL")>;
}

class Continuous_LOADri_Pat<PatFrag Ld, ValueType vt, int shift, int DstType, V5Inst_SIMT I> :
  Pat<(vt (Ld (ContinuousADDRri<shift> i64:$SrcL, simm_pshl<24, shift>:$Imm))),
      (I DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm_pshl<24, shift>:$Imm)>;

multiclass Continuous_LOADri_Addrspace_Pat<ValueType vt, int shift, int DstType, V5Inst_SIMT I> {
  def : Continuous_LOADri_Pat<LoadAsLocal<load>, vt,  shift, DstType, I>;
  def : Continuous_LOADri_Pat<LoadAsGlobal<load>, vt,  shift, DstType, !cast<V5Inst_SIMT>(I # "_GLOBAL")>;
}
}

multiclass LOADri_Pat<PatFrag Ld, ValueType vt, int shift, int DstType, V5Inst_SIMT I, string gstr = ""> {
  def : LOADri_scaled_Pat<Ld, vt, shift, DstType, !cast<V5Inst_SIMT>(I # gstr)>;
  def : LOADri_unscaled_Pat<Ld, vt, DstType, !cast<V5Inst_SIMT>(I # "_U" # gstr)>;
}

multiclass LOADri_Addrspace_Pat<PatFrag Ld, ValueType vt, int shift, int DstType, V5Inst_SIMT I> {
  defm : LOADri_Pat<LoadAsLocal<Ld>, vt, shift, DstType, I, "">;
  defm : LOADri_Pat<LoadAsGlobal<Ld>, vt, shift, DstType, I, "_GLOBAL">;
}

defm : LOADri_Addrspace_Pat<DivergentUnaryOp<sextloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<sextloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<sextloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHI>;

defm : LOADri_Addrspace_Pat<DivergentUnaryOp<extloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<extloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<extloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHI>;

defm : LOADri_Addrspace_Pat<DivergentUnaryOp<zextloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWUI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<zextloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHUI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<zextloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHUI>;

defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, i64, 3, SIMT_DstReg_D.Value, SIMT_LDI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, i32, 2, SIMT_DstReg_W.Value, SIMT_LWI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, i16, 1, SIMT_DstReg_H.Value, SIMT_LHI>;

defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, f64, 3, SIMT_DstReg_D.Value, SIMT_LDI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, f32, 2, SIMT_DstReg_W.Value, SIMT_LWI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, f16, 1, SIMT_DstReg_H.Value, SIMT_LHI>;
defm : LOADri_Addrspace_Pat<DivergentUnaryOp<load>, bf16, 1, SIMT_DstReg_H.Value, SIMT_LHI>;

defm : LOADri_Addrspace_Pat<UniformUnaryOp<sextloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<sextloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<sextloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHI_SCAR>;

defm : LOADri_Addrspace_Pat<UniformUnaryOp<extloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<extloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<extloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHI_SCAR>;

defm : LOADri_Addrspace_Pat<UniformUnaryOp<zextloadi32>, i64, 2, SIMT_DstReg_D.Value, SIMT_LWUI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<zextloadi16>, i64, 1, SIMT_DstReg_D.Value, SIMT_LHUI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<zextloadi16>, i32, 1, SIMT_DstReg_W.Value, SIMT_LHUI_SCAR>;

defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, i64, 3, SIMT_DstReg_D.Value, SIMT_LDI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, i32, 2, SIMT_DstReg_W.Value, SIMT_LWI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, i16, 1, SIMT_DstReg_H.Value, SIMT_LHI_SCAR>;

defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, f64, 3, SIMT_DstReg_D.Value, SIMT_LDI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, f32, 2, SIMT_DstReg_W.Value, SIMT_LWI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, f16, 1, SIMT_DstReg_H.Value, SIMT_LHI_SCAR>;
defm : LOADri_Addrspace_Pat<UniformUnaryOp<load>, bf16, 1, SIMT_DstReg_H.Value, SIMT_LHI_SCAR>;

defm : Continuous_LOADri_Addrspace_Pat<i64,  3, SIMT_DstReg_D.Value, SIMT_Continuous_LDI>;
defm : Continuous_LOADri_Addrspace_Pat<i32,  2, SIMT_DstReg_W.Value, SIMT_Continuous_LWI>;
defm : Continuous_LOADri_Addrspace_Pat<i16,  1, SIMT_DstReg_H.Value, SIMT_Continuous_LHI>;
defm : Continuous_LOADri_Addrspace_Pat<i8,   0, SIMT_DstReg_B.Value, SIMT_Continuous_LBI>;

defm : Continuous_LOADri_Addrspace_Pat<f64,  3, SIMT_DstReg_D.Value, SIMT_Continuous_LDI>;
defm : Continuous_LOADri_Addrspace_Pat<f32,  2, SIMT_DstReg_W.Value, SIMT_Continuous_LWI>;
defm : Continuous_LOADri_Addrspace_Pat<f16,  1, SIMT_DstReg_H.Value, SIMT_Continuous_LHI>;
defm : Continuous_LOADri_Addrspace_Pat<bf16, 1, SIMT_DstReg_H.Value, SIMT_Continuous_LHI>;

defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<sextloadi8>, i64, SIMT_DstReg_D.Value, SIMT_LBI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<sextloadi8>, i32, SIMT_DstReg_W.Value, SIMT_LBI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<sextloadi8>, i16, SIMT_DstReg_H.Value, SIMT_LBI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<zextloadi8>, i64, SIMT_DstReg_D.Value, SIMT_LBUI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<zextloadi8>, i32, SIMT_DstReg_W.Value, SIMT_LBUI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<zextloadi8>, i16, SIMT_DstReg_H.Value, SIMT_LBUI>;
defm : LOADri_unscaled_Addrspace_Pat<DivergentUnaryOp<load>,       i8,  SIMT_DstReg_B.Value, SIMT_LBI>;

defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<sextloadi8>, i64, SIMT_DstReg_D.Value, SIMT_LBI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<sextloadi8>, i32, SIMT_DstReg_W.Value, SIMT_LBI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<sextloadi8>, i16, SIMT_DstReg_H.Value, SIMT_LBI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<zextloadi8>, i64, SIMT_DstReg_D.Value, SIMT_LBUI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<zextloadi8>, i32, SIMT_DstReg_W.Value, SIMT_LBUI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<zextloadi8>, i16, SIMT_DstReg_H.Value, SIMT_LBUI_SCAR>;
defm : LOADri_unscaled_Addrspace_Pat<UniformUnaryOp<load>,       i8,  SIMT_DstReg_B.Value, SIMT_LBI_SCAR>;

class SIMT_Storei<bits<3> FuncHi, bits<1> Loc, bits<1> Brg, bits<1> Con, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_E<FuncHi, {Loc, Brg, Con}, 0b101, 0b100, (outs ),
                    (ins SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, ImmOp:$imm24), opcstr>,
                     Sched<[WriteST, ReadDafult, ReadDafult]> {
  bits<24> imm24;
  let imm12 = imm24{11-0};
  let rc_imm12 = imm24{23-12};
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "[$SrcR" # "$SrcRType, " # "$imm24]";
  let mayStore = 1;
}

multiclass SIMT_Storei_LG<bits<3> FuncHi, bits<1> Brg, bits<1> Con, Operand ImmOp, string opcstr> {
  def "" : SIMT_Storei<FuncHi, 0b1, Brg, Con, ImmOp, opcstr # ".local">;
  def _GLOBAL : SIMT_Storei<FuncHi, 0b0, Brg, Con, ImmOp, opcstr>;
}

multiclass SIMT_Storei_Brg<bits<3> FuncHi, bits<1> Con, Operand ImmOp, string opcstr> {
  defm "" : SIMT_Storei_LG<FuncHi, 0b0, Con, ImmOp, opcstr>;
  defm _BRG : SIMT_Storei_LG<FuncHi, 0b1, Con, ImmOp, opcstr # ".brg">;
}

class SIMT_Storei_Scar<bits<3> FuncHi, bits<3> FuncLo, Operand ImmOp, string opcstr> :
      V5Inst_SIMT_E_Scar<FuncHi, FuncLo, 0b101, 0b100, (outs ),
                    (ins SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, ImmOp:$imm24), opcstr>,
                     Sched<[WriteST, ReadDafult, ReadDafult]> {
  bits<24> imm24;
  let imm12 = imm24{11-0};
  let rc_imm12 = imm24{23-12};
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "[$SrcR" # "$SrcRType, " # "$imm24]";
  let mayStore = 1;
}

multiclass SIMT_Storei_Scar_LG<bits<3> FuncHi, Operand ImmOp, string opcstr> {
  def "" : SIMT_Storei_Scar<FuncHi, 0b100, ImmOp, opcstr # ".local">;
  def _GLOBAL : SIMT_Storei_Scar<FuncHi, 0b000, ImmOp, opcstr>;
}

let Uses = [SIMT_P] in {
defm SIMT_SHI : SIMT_Storei_Brg<0b001, 0b0, simm_pshl<24, 1>, "v.shi">;
defm SIMT_SWI : SIMT_Storei_Brg<0b010, 0b0, simm_pshl<24, 2>, "v.swi">;
defm SIMT_SDI : SIMT_Storei_Brg<0b011, 0b0, simm_pshl<24, 3>, "v.sdi">;

defm SIMT_SCAR_SHI : SIMT_Storei_Scar_LG<0b001, simm_pshl<24, 1>, "l.shi">;
defm SIMT_SCAR_SWI : SIMT_Storei_Scar_LG<0b010, simm_pshl<24, 2>, "l.swi">;
defm SIMT_SCAR_SDI : SIMT_Storei_Scar_LG<0b011, simm_pshl<24, 3>, "l.sdi">;
}

let Format = InstFormat_StoreI_UnScaled, Uses = [SIMT_P] in {
  defm SIMT_SBI   : SIMT_Storei_Brg<0b000, 0b0, simm24i64, "v.sbi">;
  defm SIMT_SHI_U : SIMT_Storei_Brg<0b101, 0b0, simm24i64, "v.shi.u">;
  defm SIMT_SWI_U : SIMT_Storei_Brg<0b110, 0b0, simm24i64, "v.swi.u">;
  defm SIMT_SDI_U : SIMT_Storei_Brg<0b111, 0b0, simm24i64, "v.sdi.u">;

  defm SIMT_SCAR_SBI  : SIMT_Storei_Scar_LG<0b000, simm24i64, "l.sbi">;
  defm SIMT_SCAR_SHI_U: SIMT_Storei_Scar_LG<0b101, simm24i64, "l.shi.u">;
  defm SIMT_SCAR_SWI_U: SIMT_Storei_Scar_LG<0b110, simm24i64, "l.swi.u">;
  defm SIMT_SCAR_SDI_U: SIMT_Storei_Scar_LG<0b111, simm24i64, "l.sdi.u">;
}

let Predicates = [isSIMT] in {
class STOREri_scaled_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I> :
    Pat<(St vt:$SrcD, (SIMTADDRri_scaled<shift> i64:$SrcL, simm_pshl<24, shift>:$Imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm_pshl<24, shift>:$Imm)>;
class STOREri_unscaled_Pat<PatFrag St, ValueType vt, int DstType, V5Inst_SIMT I> :
    Pat<(St vt:$SrcD, (SIMTADDRri_unscaled i64:$SrcL, simm24i64:$Imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm24i64:$Imm)>;
class Continuous_STOREri_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I> :
    Pat<(St vt:$SrcD, (ContinuousADDRri<shift> i64:$SrcL, simm_pshl<24, shift>:$Imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, simm_pshl<24, shift>:$Imm)>;
}

let Predicates = [isSIMT] in
def : Pat<(linxv5_addlo i64:$hi, mcsym:$lo), (SIMT_ADDI SIMT_DstReg_D.Value, SIMTSrcVecReg:$hi, SIMT_Int_SrcReg_SD.Value, mcsym:$lo)>;

def SIMT_BSTOP : V5Inst_SIMT_K<(outs ), (ins ), "L.BSTOP">, Sched<[WriteDafultA]>;

class SIMT_INTrr_Base<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A<Func, FuncLo, Opcode, Type, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

class SIMT_INTrr_Base_Scar<bits<3> Func, bits<3> FuncLo, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A_Scar<Func, FuncLo, Opcode, Type, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

class SIMT_MUL_Base<bits<3> Func, string opcstr>
    : SIMT_INTrr_Base<Func, 0b000, 0b100, 0b011, opcstr>;

let Uses = [SIMT_P] in {
def SIMT_MUL : SIMT_MUL_Base<0b000, "v.mul">;
def SIMT_MUL_SCAR : SIMT_INTrr_Base_Scar<0b000, 0b000, 0b100, 0b011, "l.mul">;
}

multiclass SIMT_MUL_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$Src)), i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$Src)), i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value))>;
  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$SrcL)), (i64 (LoopRegUnaryOp<zext> i16:$SrcR)))),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_UH.Value))>;
  def : Pat<(i64 (OpNode i64:$SrcL, i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value))>;
  def : Pat<(i32 (OpNode i32:$SrcL, i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value))>;
  def : Pat<(i16 (OpNode i16:$SrcL, i16:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), i16:$SrcR, (i8 SIMT_Int_SrcReg_SH.Value))>;
  def : Pat<(i8 (OpNode i8:$SrcL, i8:$SrcR)),
            (Inst (i8 SIMT_DstReg_B.Value), i8:$SrcL, (i8 SIMT_Int_SrcReg_SB.Value), i8:$SrcR, (i8 SIMT_Int_SrcReg_SB.Value))>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_MUL_Pat<DivergentOp<mul>, SIMT_MUL>;
  defm : SIMT_MUL_Pat<UniformOp<mul>, SIMT_MUL_SCAR>;
}

multiclass SIMT_MUL_CVT_Pat<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), VT:$SrcR)),
            (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), VT:$SrcR, (i8 RType))>;
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (IcvtOp SourceVT:$SrcR)))),
            (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$SrcR, (i8 SourceRType))>;
  def : Pat<(VT (OpNode (VT (LoopRegUnaryOp<zext> i16:$SrcL)), (VT (IcvtOp SourceVT:$SrcR)))),
            (Inst (i8 DType), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), SourceVT:$SrcR, (i8 SourceRType))>;
}

multiclass SIMT_MUL_CVT_NoLoopReg_Pat<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), VT:$SrcR)),
            (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), VT:$SrcR, (i8 RType))>;
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$SrcL)), (VT (IcvtOp SourceVT:$SrcR)))),
            (Inst (i8 DType), SourceVT:$SrcL, (i8 SourceRType), SourceVT:$SrcR, (i8 SourceRType))>;
}

multiclass SIMT_Base_MUL_CVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_MUL_CVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_NoLoopReg_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_MUL_CVT_ZeroExt_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_MUL_CVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_MUL_CVT_NoLoopReg_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_MUL_High_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_MUL_CVT_Pat<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_MUL_CVT_Pat<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_MUL_CVT_ZeroExt_Pat<N, I, RIORegUnaryOp<zext>>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_Base_MUL_High_Pat<DivergentOp<mul>, SIMT_MUL>;
  defm : SIMT_Base_MUL_High_Pat<UniformOp<mul>, SIMT_MUL_SCAR>;
}

class SIMT_MADD_Base<bits<3> Func, string opcstr>:
    V5Inst_SIMT_A<Func, 0b000, 0b100, 0b011, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
           SIMTSrcVecReg:$SrcD, SIMTIntSrcRegType:$SrcDType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcD$SrcDType, $Dst$DstType";
}

class SIMT_MADD_Base_Scar<bits<3> Func, string opcstr>:
    V5Inst_SIMT_A_Scar<Func, 0b000, 0b100, 0b011, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
           SIMTSrcScalarReg:$SrcD, SIMTIntSrcRegType:$SrcDType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcD$SrcDType, $Dst$DstType";
}

let Uses = [SIMT_P] in {
  def SIMT_MADD      : SIMT_MADD_Base<0b110, "v.madd">;
  def SIMT_MADD_SCAR : SIMT_MADD_Base_Scar<0b110, "l.madd">;
}
let Predicates = [isSIMT] in {
class MADD_Pat_Div<ValueType vt, int DstType, int SrcType> :
    Pat<(vt (DivergentOp<add> (mul vt:$SrcL, vt:$SrcR), vt:$SrcD)),
        (SIMT_MADD DstType,
                   vt:$SrcL, SrcType,
                   vt:$SrcR, SrcType,
                   vt:$SrcD, SrcType)>;

class MADD_Pat_Uni<ValueType vt, int DstType, int SrcType> :
    Pat<(vt (UniformOp<add> (mul vt:$SrcL, vt:$SrcR), vt:$SrcD)),
        (SIMT_MADD_SCAR DstType,
                        vt:$SrcL, SrcType,
                        vt:$SrcR, SrcType,
                        vt:$SrcD, SrcType)>;
}

def : MADD_Pat_Div<i64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value>;
def : MADD_Pat_Div<i32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value>;
def : MADD_Pat_Div<i16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value>;
def : MADD_Pat_Div<i8,  SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value>;

def : MADD_Pat_Uni<i64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value>;
def : MADD_Pat_Uni<i32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value>;
def : MADD_Pat_Uni<i16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value>;
def : MADD_Pat_Uni<i8,  SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value>;


class SIMT_STORErr_Base<bits<3> Func, bits<1> Loc, bits<1> Brg, bits<1> Con, string opcstr, Operand ShamtOp, string lane>:
    V5Inst_SIMT_A<Func, {Loc, Brg, Con}, 0b100, 0b100, (outs),
      (ins SIMTSrcVecReg:$SrcD, SIMTIntSrcRegType:$SrcDType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, ShamtOp:$ShamtImm), opcstr>,
           Sched<[WriteST, ReadDafult, ReadDafult, ReadDafult]> {
  bits<5> ShamtImm;
  let Dst = 0;
  let DstType = 0;
  let Inst{11-7} = ShamtImm;
  let AsmString = opcstr # "\t$SrcD$SrcDType, [${SrcL}$SrcLType"# lane #", $SrcR${SrcRType}" # "$ShamtImm]";
}

multiclass SIMT_STORErr_Base_LG<bits<3> Func, bits<1> Brg, bits<1> Con, string opcstr, Operand ShamtOp, string lane> {
  def "" : SIMT_STORErr_Base<Func, 0b1, Brg, Con, opcstr # ".local", ShamtOp, lane>;
  def _GLOBAL : SIMT_STORErr_Base<Func, 0b0, Brg, Con, opcstr, ShamtOp, lane>;
}

multiclass SIMT_STORErr_Base_LG_Brg<bits<3> Func, bits<1> Con, string opcstr, Operand ShamtOp, string lane> {
  defm "" : SIMT_STORErr_Base_LG<Func, 0b0, Con, opcstr, ShamtOp, lane>;
  defm _BRG : SIMT_STORErr_Base_LG<Func, 0b1, Con, opcstr # ".brg", ShamtOp, lane>;
}

class SIMT_STORErr_Base_Scar<bits<3> Func, bits<3> FuncLo, string opcstr, Operand ShamtOp>:
    V5Inst_SIMT_A_Store_Scar<Func, FuncLo, 0b100, 0b100, (outs),
      (ins SIMTSrcScalarReg:$SrcD, SIMTIntSrcRegType:$SrcDType,
           SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, ShamtOp:$ShamtImm), opcstr>,
           Sched<[WriteST, ReadDafult, ReadDafult, ReadDafult]> {
  bits<5> ShamtImm;
  let Dst = 0;
  let DstType = 0;
  let Inst{11-7} = ShamtImm;
  let AsmString = opcstr # "\t$SrcD$SrcDType, [${SrcL}$SrcLType" # ", $SrcR${SrcRType}"#"$ShamtImm]";
}

multiclass SIMT_STORErr_Base_Scar_LG<bits<3> Func, string opcstr, Operand Shamt> {
  def "" : SIMT_STORErr_Base_Scar<Func, 0b100, opcstr # ".local", Shamt>;
  def _GLOBAL : SIMT_STORErr_Base_Scar<Func, 0b000, opcstr, Shamt>;
}

let mayStore = 1, Uses = [SIMT_P] in {
  defm SIMT_SB : SIMT_STORErr_Base_LG_Brg<0b000, 0b0, "v.sb", SIMTShamtImm<0>, "">;
  defm SIMT_SH : SIMT_STORErr_Base_LG_Brg<0b001, 0b0, "v.sh", SIMTShamtImm<1>, "">;
  defm SIMT_SW : SIMT_STORErr_Base_LG_Brg<0b010, 0b0, "v.sw", SIMTShamtImm<2>, "">;
  defm SIMT_SD : SIMT_STORErr_Base_LG_Brg<0b011, 0b0, "v.sd", SIMTShamtImm<3>, "">;

  defm SIMT_SCAR_SB : SIMT_STORErr_Base_Scar_LG<0b000, "l.sb", SIMTShamtImm<0>>;
  defm SIMT_SCAR_SH : SIMT_STORErr_Base_Scar_LG<0b001, "l.sh", SIMTShamtImm<1>>;
  defm SIMT_SCAR_SW : SIMT_STORErr_Base_Scar_LG<0b010, "l.sw", SIMTShamtImm<2>>;
  defm SIMT_SCAR_SD : SIMT_STORErr_Base_Scar_LG<0b011, "l.sd", SIMTShamtImm<3>>;

  defm SIMT_SH_U : SIMT_STORErr_Base_LG_Brg<0b101, 0b0, "v.sh.u", SIMTShamtImm<0>, "">;
  defm SIMT_SW_U : SIMT_STORErr_Base_LG_Brg<0b110, 0b0, "v.sw.u", SIMTShamtImm<0>, "">;
  defm SIMT_SD_U : SIMT_STORErr_Base_LG_Brg<0b111, 0b0, "v.sd.u", SIMTShamtImm<0>, "">;

  defm SIMT_SCAR_SH_U : SIMT_STORErr_Base_Scar_LG<0b101, "l.sh.u", SIMTShamtImm<0>>;
  defm SIMT_SCAR_SW_U : SIMT_STORErr_Base_Scar_LG<0b110, "l.sw.u", SIMTShamtImm<0>>;
  defm SIMT_SCAR_SD_U : SIMT_STORErr_Base_Scar_LG<0b111, "l.sd.u", SIMTShamtImm<0>>;

  defm SIMT_Continuous_SB : SIMT_STORErr_Base_LG_Brg<0b000, 0b1, "v.sb", SIMTShamtImm<0>, ", lc0">;
  defm SIMT_Continuous_SH : SIMT_STORErr_Base_LG_Brg<0b001, 0b1, "v.sh", SIMTShamtImm<1>, ", lc0<<1">;
  defm SIMT_Continuous_SW : SIMT_STORErr_Base_LG_Brg<0b010, 0b1, "v.sw", SIMTShamtImm<2>, ", lc0<<2">;
  defm SIMT_Continuous_SD : SIMT_STORErr_Base_LG_Brg<0b011, 0b1, "v.sd", SIMTShamtImm<3>, ", lc0<<3">;

  defm SIMT_Continuous_SH_U : SIMT_STORErr_Base_LG_Brg<0b101, 0b1, "v.sh.u", SIMTShamtImm<0>, ", lc0<<1">;
  defm SIMT_Continuous_SW_U : SIMT_STORErr_Base_LG_Brg<0b110, 0b1, "v.sw.u", SIMTShamtImm<0>, ", lc0<<2">;
  defm SIMT_Continuous_SD_U : SIMT_STORErr_Base_LG_Brg<0b111, 0b1, "v.sd.u", SIMTShamtImm<0>, ", lc0<<3">;
}

class SIMT_Storei_Lane<bits<3> FuncHi, bits<1> Loc, bits<1> Brg, Operand ImmOp, string opcstr, string lane> :
      V5Inst_SIMT_E<FuncHi, {Loc, Brg, 0b1}, 0b101, 0b100, (outs ),
                    (ins SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, ImmOp:$imm24), opcstr>,
                     Sched<[WriteST, ReadDafult, ReadDafult]> {
  bits<24> imm24;
  let imm12 = imm24{11-0};
  let rc_imm12 = imm24{23-12};
  let AsmString = opcstr # "\t$SrcL" # "$SrcLType, " # "[$SrcR" # "$SrcRType, "# lane # ", $imm24]";
  let mayStore = 1;
}

multiclass SIMT_Storei_Lane_LG<bits<3> FuncHi, bits<1> Brg, Operand ImmOp, string opcstr, string lane> {
  def "" : SIMT_Storei_Lane<FuncHi, 1, Brg, ImmOp, opcstr # ".local", lane>;
  def _GLOBAL : SIMT_Storei_Lane<FuncHi, 0, Brg, ImmOp, opcstr, lane>;
}

multiclass SIMT_Storei_Lane_Brg<bits<3> FuncHi, Operand ImmOp, string opcstr, string lane> {
  defm "" : SIMT_Storei_Lane_LG<FuncHi, 0b0, ImmOp, opcstr, lane>;
  defm _BRG : SIMT_Storei_Lane_LG<FuncHi, 0b1, ImmOp, opcstr # ".brg", lane>;
}

let Uses = [SIMT_P] in {
defm SIMT_Continuous_SHI : SIMT_Storei_Lane_Brg<0b001, simm_pshl<24, 1>, "v.shi", "lc0<<1">;
defm SIMT_Continuous_SWI : SIMT_Storei_Lane_Brg<0b010, simm_pshl<24, 2>, "v.swi", "lc0<<2">;
defm SIMT_Continuous_SDI : SIMT_Storei_Lane_Brg<0b011, simm_pshl<24, 3>, "v.sdi", "lc0<<3">;
}

let Format = InstFormat_StoreI_UnScaled, Uses = [SIMT_P] in {
  defm SIMT_Continuous_SBI   : SIMT_Storei_Lane_Brg<0b000, simm24i64, "v.sbi", "lc0">;
  defm SIMT_Continuous_SHI_U : SIMT_Storei_Lane_Brg<0b101, simm24i64, "v.shi.u", "lc0<<1">;
  defm SIMT_Continuous_SWI_U : SIMT_Storei_Lane_Brg<0b110, simm24i64, "v.swi.u", "lc0<<2">;
  defm SIMT_Continuous_SDI_U : SIMT_Storei_Lane_Brg<0b111, simm24i64, "v.sdi.u", "lc0<<3">;
}

class StoreAsLocal<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR), (N node:$SrcL, node:$SrcR), [{
  auto *StoreN = cast<StoreSDNode>(N);
  return StoreN->getPointerInfo().getAddrSpace() == 6;
}]>;

class StoreAsGlobal<SDPatternOperator N>
  : PatFrag<(ops node:$SrcL, node:$SrcR), (N node:$SrcL, node:$SrcR), [{
  auto *StoreN = cast<StoreSDNode>(N);
  return StoreN->getPointerInfo().getAddrSpace() != 6;
}]>;

let Predicates = [isSIMT] in {
class STORErr_scaled_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I> :
    Pat<(St vt:$SrcD, (SIMTStoreADDRrr_scaled<shift> i64:$SrcL, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<shift>:$imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<shift>:$imm)>;
class STORErr_unscaled_Pat<PatFrag St, ValueType vt, int DstType, V5Inst_SIMT I> :
    Pat<(St vt:$SrcD, (SIMTStoreADDRrr_unscaled i64:$SrcL, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<0>:$imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<0>:$imm)>;

class Continuous_STORErr_Pat<PatFrag St, ValueType vt, int scale, int DstType, Instruction I> :
    Pat<(St vt:$SrcD, (ContinuousStoreADDRrr<scale> i64:$SrcL, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<scale>:$imm)),
        (I vt:$SrcD, DstType, i64:$SrcL, SIMT_Int_SrcReg_SD.Value, i64:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTShamtImm<scale>:$imm)>;
}

multiclass STORE_unscaled_Pat<PatFrag St, ValueType vt, int DstType, V5Inst_SIMT I, string gstr = ""> {
  def : STORErr_unscaled_Pat<St, vt, DstType, !cast<V5Inst_SIMT>(I # gstr)>;
  def : STOREri_unscaled_Pat<St, vt, DstType, !cast<V5Inst_SIMT>(I # "I" # gstr)>;
}

multiclass STORE_unscaled_Addrspace_Pat<PatFrag St, ValueType vt, int DstType, V5Inst_SIMT I> {
  defm : STORE_unscaled_Pat<StoreAsLocal<St>, vt, DstType, I, "">;
  defm : STORE_unscaled_Pat<StoreAsGlobal<St>, vt, DstType, I, "_GLOBAL">;
}

multiclass STORE_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I, string gstr = ""> {
  def : STORErr_scaled_Pat<St, vt, shift, DstType, !cast<V5Inst_SIMT>(I # gstr)>;
  def : STOREri_scaled_Pat<St, vt, shift, DstType, !cast<V5Inst_SIMT>(I # "I" # gstr)>;
  def : STORErr_unscaled_Pat<St, vt, DstType, !cast<V5Inst_SIMT>(I # "_U" # gstr)>;
  def : STOREri_unscaled_Pat<St, vt, DstType, !cast<V5Inst_SIMT>(I # "I_U" # gstr)>;
}

multiclass STORE_Addrspace_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I> {
  defm : STORE_Pat<StoreAsLocal<St>, vt, shift, DstType, I, "">;
  defm : STORE_Pat<StoreAsGlobal<St>, vt, shift, DstType, I, "_GLOBAL">;
}

multiclass Continuous_STORE_Pat<PatFrag St, ValueType vt, int shift, int DstType, V5Inst_SIMT I, string gstr = ""> {
  def : Continuous_STORErr_Pat<St, vt, shift, DstType, !cast<V5Inst_SIMT>(I # gstr)>;
  def : Continuous_STOREri_Pat<St, vt, shift, DstType, !cast<V5Inst_SIMT>(I # "I" # gstr)>;
}

multiclass Continuous_STORE_Addrspace_Pat<ValueType vt, int shift, int DstType, V5Inst_SIMT I> {
  defm : Continuous_STORE_Pat<StoreAsLocal<store>, vt, shift, DstType, I, "">;
  defm : Continuous_STORE_Pat<StoreAsGlobal<store>, vt, shift, DstType, I, "_GLOBAL">;
}

defm : STORE_Addrspace_Pat<DivergentOp<truncstorei32>, i64, 2, SIMT_Int_SrcReg_SD.Value, SIMT_SW>;
defm : STORE_Addrspace_Pat<DivergentOp<truncstorei16>, i64, 1, SIMT_Int_SrcReg_SD.Value, SIMT_SH>;
defm : STORE_Addrspace_Pat<DivergentOp<truncstorei16>, i32, 1, SIMT_Int_SrcReg_SW.Value, SIMT_SH>;

defm : STORE_Addrspace_Pat<DivergentOp<store>, i64, 3, SIMT_Int_SrcReg_SD.Value, SIMT_SD>;
defm : STORE_Addrspace_Pat<DivergentOp<store>, i32, 2, SIMT_Int_SrcReg_SW.Value, SIMT_SW>;
defm : STORE_Addrspace_Pat<DivergentOp<store>, i16, 1, SIMT_Int_SrcReg_SH.Value, SIMT_SH>;

defm : STORE_Addrspace_Pat<DivergentOp<store>, f64,  3, SIMT_Int_SrcReg_SD.Value, SIMT_SD>;
defm : STORE_Addrspace_Pat<DivergentOp<store>, f32,  2, SIMT_Int_SrcReg_SW.Value, SIMT_SW>;
defm : STORE_Addrspace_Pat<DivergentOp<store>, f16,  1, SIMT_Int_SrcReg_SH.Value, SIMT_SH>;
defm : STORE_Addrspace_Pat<DivergentOp<store>, bf16, 1, SIMT_Int_SrcReg_SH.Value, SIMT_SH>;

defm : STORE_Addrspace_Pat<UniformOp<truncstorei32>, i64, 2, SIMT_Int_SrcReg_SD.Value, SIMT_SCAR_SW>;
defm : STORE_Addrspace_Pat<UniformOp<truncstorei16>, i64, 1, SIMT_Int_SrcReg_SD.Value, SIMT_SCAR_SH>;
defm : STORE_Addrspace_Pat<UniformOp<truncstorei16>, i32, 1, SIMT_Int_SrcReg_SW.Value, SIMT_SCAR_SH>;

defm : STORE_Addrspace_Pat<UniformOp<store>, i64, 3, SIMT_Int_SrcReg_SD.Value, SIMT_SCAR_SD>;
defm : STORE_Addrspace_Pat<UniformOp<store>, i32, 2, SIMT_Int_SrcReg_SW.Value, SIMT_SCAR_SW>;
defm : STORE_Addrspace_Pat<UniformOp<store>, i16, 1, SIMT_Int_SrcReg_SH.Value, SIMT_SCAR_SH>;

defm : STORE_Addrspace_Pat<UniformOp<store>, f64,  3, SIMT_Int_SrcReg_SD.Value, SIMT_SCAR_SD>;
defm : STORE_Addrspace_Pat<UniformOp<store>, f32,  2, SIMT_Int_SrcReg_SW.Value, SIMT_SCAR_SW>;
defm : STORE_Addrspace_Pat<UniformOp<store>, f16,  1, SIMT_Int_SrcReg_SH.Value, SIMT_SCAR_SH>;
defm : STORE_Addrspace_Pat<UniformOp<store>, bf16, 1, SIMT_Int_SrcReg_SH.Value, SIMT_SCAR_SH>;

defm : Continuous_STORE_Addrspace_Pat<i64,  3, SIMT_Int_SrcReg_SD.Value, SIMT_Continuous_SD>;
defm : Continuous_STORE_Addrspace_Pat<i32,  2, SIMT_Int_SrcReg_SW.Value, SIMT_Continuous_SW>;
defm : Continuous_STORE_Addrspace_Pat<i16,  1, SIMT_Int_SrcReg_SH.Value, SIMT_Continuous_SH>;
defm : Continuous_STORE_Addrspace_Pat<i8,   0, SIMT_Int_SrcReg_SB.Value, SIMT_Continuous_SB>;

defm : Continuous_STORE_Addrspace_Pat<f64,  3, SIMT_Int_SrcReg_SD.Value, SIMT_Continuous_SD>;
defm : Continuous_STORE_Addrspace_Pat<f32,  2, SIMT_Int_SrcReg_SW.Value, SIMT_Continuous_SW>;
defm : Continuous_STORE_Addrspace_Pat<f16,  1, SIMT_Int_SrcReg_SH.Value, SIMT_Continuous_SH>;
defm : Continuous_STORE_Addrspace_Pat<bf16, 1, SIMT_Int_SrcReg_SH.Value, SIMT_Continuous_SH>;

defm : STORE_unscaled_Addrspace_Pat<DivergentOp<truncstorei8>,  i64, SIMT_Int_SrcReg_SD.Value, SIMT_SB>;
defm : STORE_unscaled_Addrspace_Pat<DivergentOp<truncstorei8>,  i32, SIMT_Int_SrcReg_SW.Value, SIMT_SB>;
defm : STORE_unscaled_Addrspace_Pat<DivergentOp<truncstorei8>,  i16, SIMT_Int_SrcReg_SH.Value, SIMT_SB>;
defm : STORE_unscaled_Addrspace_Pat<DivergentOp<store>,         i8,  SIMT_Int_SrcReg_SB.Value, SIMT_SB>;

defm : STORE_unscaled_Addrspace_Pat<UniformOp<truncstorei8>,  i64, SIMT_Int_SrcReg_SD.Value, SIMT_SCAR_SB>;
defm : STORE_unscaled_Addrspace_Pat<UniformOp<truncstorei8>,  i32, SIMT_Int_SrcReg_SW.Value, SIMT_SCAR_SB>;
defm : STORE_unscaled_Addrspace_Pat<UniformOp<truncstorei8>,  i16, SIMT_Int_SrcReg_SH.Value, SIMT_SCAR_SB>;
defm : STORE_unscaled_Addrspace_Pat<UniformOp<store>,         i8,  SIMT_Int_SrcReg_SB.Value, SIMT_SCAR_SB>;

let Format = InstFormatFP in
class SIMT_FLOATrr_Base<bits<3> Func, bits<3> FucLo, bits<3> Opcode, bits<3> Type, string opcstr>:
    V5Inst_SIMT_A<Func, FucLo, Opcode, Type, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

class SIMT_FLOATrr_Base_Scar<bits<3> Func, bits<3> FucLo, bits<3> Opcode, bits<3> Type, string opcstr>:
    V5Inst_SIMT_A_Scar<Func, FucLo, Opcode, Type, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

let Format = InstFormatFP in
class SIMT_FLOATrr_RS_Base<bits<3> Func, bits<3> FucLo, bits<3> Opcode, bits<3> Type, string opcstr>:
    V5Inst_SIMT_A<Func, FucLo, Opcode, Type, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;
  let SrcD = 0;
  let SrcDType = 0;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode, $sat";
}

class SIMT_FLOATrr_RS_Base_Scar<bits<3> Func, bits<3> FucLo, bits<3> Opcode, bits<3> Type, string opcstr>:
    V5Inst_SIMT_A_Scar<Func, FucLo, Opcode, Type, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;
  let SrcD = 0;
  let SrcDType = 0;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode, $sat";
}
class SIMT_FARITHrr_Base<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_RS_Base<Func, 0b000, 0b100, 0b101, opcstr>;

class SIMT_FARITHrr_Base_Scar<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_RS_Base_Scar<Func, 0b000, 0b100, 0b101, opcstr>;

let Uses = [SIMT_P] in {
def SIMT_FADD : SIMT_FARITHrr_Base<0b000, "v.fadd">;
def SIMT_FSUB : SIMT_FARITHrr_Base<0b001, "v.fsub">;
def SIMT_FMUL : SIMT_FARITHrr_Base<0b010, "v.fmul">;
def SIMT_FDIV : SIMT_FARITHrr_Base<0b011, "v.fdiv">;

def SIMT_FADD_SCAR : SIMT_FARITHrr_Base_Scar<0b000, "l.fadd">;
def SIMT_FSUB_SCAR : SIMT_FARITHrr_Base_Scar<0b001, "l.fsub">;
def SIMT_FMUL_SCAR : SIMT_FARITHrr_Base_Scar<0b010, "l.fmul">;
def SIMT_FDIV_SCAR : SIMT_FARITHrr_Base_Scar<0b011, "l.fdiv">;
}

def : InstAlias<"v.fadd\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"v.fsub\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"v.fmul\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FMUL SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"v.fdiv\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FDIV SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"l.fadd\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"l.fsub\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"l.fmul\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FMUL_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;
def : InstAlias<"l.fdiv\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (SIMT_FDIV_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, 0, 0), 0>;

def : InstAlias<"v.fadd\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fsub\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fmul\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FMUL SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fdiv\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FDIV SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fadd\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fsub\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fmul\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FMUL_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fdiv\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (SIMT_FDIV_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, RMode:$RMode, 0), 0>;

multiclass SIMT_F_ALU_Base_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(f64 (OpNode f64:$SrcL, f64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), f64:$SrcL, (i8 SIMT_Float_SrcReg_FD.Value), f64:$SrcR, (i8 SIMT_Float_SrcReg_FD.Value), (i64 0), (i64 0))>;
  def : Pat<(f32 (OpNode f32:$SrcL, f32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), f32:$SrcL, (i8 SIMT_Float_SrcReg_FS.Value), f32:$SrcR, (i8 SIMT_Float_SrcReg_FS.Value), (i64 0), (i64 0))>;
  def : Pat<(f16 (OpNode f16:$SrcL, f16:$SrcR)),
            (Inst (i8 SIMT_DstReg_H.Value), f16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value), f16:$SrcR, (i8 SIMT_Float_SrcReg_FH.Value), (i64 0), (i64 0))>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_F_ALU_Base_Pat<DivergentOp<fadd>, SIMT_FADD>;
  defm : SIMT_F_ALU_Base_Pat<DivergentOp<fsub>, SIMT_FSUB>;
  defm : SIMT_F_ALU_Base_Pat<DivergentOp<fmul>, SIMT_FMUL>;
  defm : SIMT_F_ALU_Base_Pat<DivergentOp<fdiv>, SIMT_FDIV>;

  defm : SIMT_F_ALU_Base_Pat<UniformOp<fadd>, SIMT_FADD_SCAR>;
  defm : SIMT_F_ALU_Base_Pat<UniformOp<fsub>, SIMT_FSUB_SCAR>;
  defm : SIMT_F_ALU_Base_Pat<UniformOp<fmul>, SIMT_FMUL_SCAR>;
  defm : SIMT_F_ALU_Base_Pat<UniformOp<fdiv>, SIMT_FDIV_SCAR>;
}

let Predicates = [isSIMT] in
class FNEG_Pat<ValueType VT, int DstType, int SrcType> :
    Pat<(VT (fneg VT:$Src)), (SIMT_FSUB DstType, (VT R0), SrcType, VT:$Src, SrcType, (i64 0), (i64 0))>;

def : FNEG_Pat<f64,  SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FD.Value>;
def : FNEG_Pat<f32,  SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FS.Value>;
// def : FNEG_Pat<bf16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FH.Value>;
def : FNEG_Pat<f16,  SIMT_DstReg_H.Value, SIMT_Float_SrcReg_BF.Value>;

let Format = InstFormatFP in
class SIMT_FARITHrrr_Base<bits<3> Func, string opcstr>:
    V5Inst_SIMT_A<Func, 0b000, 0b100, 0b101, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
           SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcA;
  bits<3> SrcAType;
  bits<3> RMode;
  bits<1> sat;
  let SrcD = SrcA;
  let SrcDType = SrcAType;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode, $sat";
}

let Format = InstFormatFP in
class SIMT_FARITHrrr_Base_Scar<bits<3> Func, string opcstr>:
    V5Inst_SIMT_A_Scar<Func, 0b000, 0b100, 0b101, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
           SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcA;
  bits<3> SrcAType;
  bits<3> RMode;
  bits<1> sat;
  let SrcD = SrcA;
  let SrcDType = SrcAType;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode, $sat";
}

let Uses = [SIMT_P] in {
def SIMT_FMADD  : SIMT_FARITHrrr_Base<0b100, "v.fmadd">;
def SIMT_FMSUB  : SIMT_FARITHrrr_Base<0b101, "v.fmsub">;
def SIMT_FNMADD : SIMT_FARITHrrr_Base<0b110, "v.fnmadd">;
def SIMT_FNMSUB : SIMT_FARITHrrr_Base<0b111, "v.fnmsub">;

def SIMT_FMADD_SCAR  : SIMT_FARITHrrr_Base_Scar<0b100, "l.fmadd">;
def SIMT_FMSUB_SCAR  : SIMT_FARITHrrr_Base_Scar<0b101, "l.fmsub">;
def SIMT_FNMADD_SCAR : SIMT_FARITHrrr_Base_Scar<0b110, "l.fnmadd">;
def SIMT_FNMSUB_SCAR : SIMT_FARITHrrr_Base_Scar<0b111, "l.fnmsub">;
}

def : InstAlias<"v.fmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FMADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"v.fmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FMSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"v.fnmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FNMADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"v.fnmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FNMSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"l.fmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FMADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"l.fmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FMSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"l.fnmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FNMADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;
def : InstAlias<"l.fnmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType", (SIMT_FNMSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, 0, 0), 0>;

def : InstAlias<"v.fmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FMADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FMSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fnmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FNMADD SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fnmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FNMSUB SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FMADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FMSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fnmadd\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FNMADD_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fnmsub\t$SrcL$SrcLType, $SrcR$SrcRType, $SrcA$SrcAType, $Dst$DstType, $RMode", (SIMT_FNMSUB_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, SIMTSrcScalarReg:$SrcR, SIMTFloatSrcRegType:$SrcRType, SIMTSrcScalarReg:$SrcA, SIMTFloatSrcRegType:$SrcAType, RMode:$RMode, 0), 0>;

class FMACC_Pat<SDPatternOperator N, ValueType VT, int DstType, int SrcType, V5Inst_SIMT I> :
    Pat<(VT (N VT:$SrcL, VT:$SrcR, VT:$SrcA)),
        (I DstType, VT:$SrcL, SrcType, VT:$SrcR, SrcType, VT:$SrcA, SrcType, 0, 0)>;

multiclass FMACC_Pat<V5Inst_SIMT I, SDPatternOperator N> {
  def : FMACC_Pat<N, f64,  SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FD.Value, I>;
  def : FMACC_Pat<N, f32,  SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FS.Value, I>;
  def : FMACC_Pat<N, f16,  SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FH.Value, I>;
  // def : FMACC_Pat<N, bf16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_BF.Value, I>;
}

let Predicates = [isSIMT] in {
defm : FMACC_Pat<SIMT_FMADD,  DivergentTernaryOp<any_fma>>;
// a * b + (- c)
defm : FMACC_Pat<SIMT_FMSUB,  TriOpFrag<(DivergentTernaryOp<any_fma> node:$LHS, node:$MHS, (fneg node:$RHS))>>;
// -(a * b + c)
defm : FMACC_Pat<SIMT_FNMADD, TriOpFrag<(fneg (DivergentTernaryOp<any_fma> node:$LHS, node:$MHS, node:$RHS))>>;
// a * (-b) + c
defm : FMACC_Pat<SIMT_FNMSUB, TriOpFrag<(DivergentTernaryOp<any_fma> node:$LHS, (fneg node:$MHS), node:$RHS)>>;
// a * (-b) + (-c)
defm : FMACC_Pat<SIMT_FNMADD, TriOpFrag<(DivergentTernaryOp<any_fma> node:$LHS, (fneg node:$MHS), (fneg node:$RHS))>>;

defm : FMACC_Pat<SIMT_FMADD_SCAR,  UniformTernaryOp<any_fma>>;
// a * b + (- c)
defm : FMACC_Pat<SIMT_FMSUB_SCAR,  TriOpFrag<(UniformTernaryOp<any_fma> node:$LHS, node:$MHS, (fneg node:$RHS))>>;
// -(a * b + c)
defm : FMACC_Pat<SIMT_FNMADD_SCAR, TriOpFrag<(fneg (UniformTernaryOp<any_fma> node:$LHS, node:$MHS, node:$RHS))>>;
// a * (-b) + c
defm : FMACC_Pat<SIMT_FNMSUB_SCAR, TriOpFrag<(UniformTernaryOp<any_fma> node:$LHS, (fneg node:$MHS), node:$RHS)>>;
// a * (-b) + (-c)
defm : FMACC_Pat<SIMT_FNMADD_SCAR, TriOpFrag<(UniformTernaryOp<any_fma> node:$LHS, (fneg node:$MHS), (fneg node:$RHS))>>;
}

class SIMT_FCMPrr_Base<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_Base<Func, 0b000, 0b101, 0b101, opcstr>;

class SIMT_FCMPrr_Base_Scar<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_Base_Scar<Func, 0b000, 0b101, 0b101, opcstr>;

let Uses = [SIMT_P] in {
def SIMT_FEQ  : SIMT_FCMPrr_Base<0b000, "v.feq">;
def SIMT_FNE  : SIMT_FCMPrr_Base<0b001, "v.fne">;
def SIMT_FLT  : SIMT_FCMPrr_Base<0b010, "v.flt">;
def SIMT_FGE  : SIMT_FCMPrr_Base<0b011, "v.fge">;
def SIMT_FEQS : SIMT_FCMPrr_Base<0b100, "v.feqs">;
def SIMT_FNES : SIMT_FCMPrr_Base<0b101, "v.fnes">;
def SIMT_FLTS : SIMT_FCMPrr_Base<0b110, "v.flts">;
def SIMT_FGES : SIMT_FCMPrr_Base<0b111, "v.fges">;

def SIMT_FEQ_SCAR  : SIMT_FCMPrr_Base_Scar<0b000, "l.feq">;
def SIMT_FNE_SCAR  : SIMT_FCMPrr_Base_Scar<0b001, "l.fne">;
def SIMT_FLT_SCAR  : SIMT_FCMPrr_Base_Scar<0b010, "l.flt">;
def SIMT_FGE_SCAR  : SIMT_FCMPrr_Base_Scar<0b011, "l.fge">;
def SIMT_FEQS_SCAR : SIMT_FCMPrr_Base_Scar<0b100, "l.feqs">;
def SIMT_FNES_SCAR : SIMT_FCMPrr_Base_Scar<0b101, "l.fnes">;
def SIMT_FLTS_SCAR : SIMT_FCMPrr_Base_Scar<0b110, "l.flts">;
def SIMT_FGES_SCAR : SIMT_FCMPrr_Base_Scar<0b111, "l.fges">;
}

multiclass SIMT_F_CMP_Pat<CondCode Cond, V5Inst_SIMT Inst> {
  def : Pat<(i64 (setcc f64:$SrcL, f64:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f64:$SrcL, (i8 SIMT_Float_SrcReg_FD.Value), f64:$SrcR, (i8 SIMT_Float_SrcReg_FD.Value))>;
  def : Pat<(i64 (setcc f32:$SrcL, f32:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f32:$SrcL, (i8 SIMT_Float_SrcReg_FS.Value), f32:$SrcR, (i8 SIMT_Float_SrcReg_FS.Value))>;
  def : Pat<(i64 (setcc f16:$SrcL, f16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value), f16:$SrcR, (i8 SIMT_Float_SrcReg_FH.Value))>;
  def : Pat<(i64 (setcc bf16:$SrcL, bf16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), bf16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value), bf16:$SrcR, (i8 SIMT_Float_SrcReg_FH.Value))>;
}

multiclass SIMT_F_CMP_Pat_Swap<CondCode Cond, V5Inst_SIMT Inst> {
  def : Pat<(i64 (setcc f64:$SrcL, f64:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f64:$SrcR, (i8 SIMT_Float_SrcReg_FD.Value), f64:$SrcL, (i8 SIMT_Float_SrcReg_FD.Value))>;
  def : Pat<(i64 (setcc f32:$SrcL, f32:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f32:$SrcR, (i8 SIMT_Float_SrcReg_FS.Value), f32:$SrcL, (i8 SIMT_Float_SrcReg_FS.Value))>;
  def : Pat<(i64 (setcc f16:$SrcL, f16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), f16:$SrcR, (i8 SIMT_Float_SrcReg_FH.Value), f16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value))>;
  def : Pat<(i64 (setcc bf16:$SrcL, bf16:$SrcR, Cond)),
            (Inst (i8 SIMT_DstReg_D.Value), bf16:$SrcR, (i8 SIMT_Float_SrcReg_FH.Value), bf16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value))>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_F_CMP_Pat<SETOEQ, SIMT_FEQ>;
  defm : SIMT_F_CMP_Pat<SETOLT, SIMT_FLT>;
  defm : SIMT_F_CMP_Pat<SETOGE, SIMT_FGE>;

  defm : SIMT_F_CMP_Pat_Swap<SETOGT, SIMT_FLT>;
  defm : SIMT_F_CMP_Pat_Swap<SETOLE, SIMT_FGE>;
}

let Format = InstFormatFP in
class SIMT_FLOATr_Base<bits<3> Func, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A<Func, 0b000, Opcode, Type, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0b1011111;
  let SrcRType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $Dst$DstType";
}

class SIMT_FLOATr_Base_Scar<bits<3> Func, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A_Scar<Func, 0b000, Opcode, Type, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let AsmString = opcstr # "\t$SrcL$SrcLType, $Dst$DstType";
}

let Uses = [SIMT_P] in {
def SIMT_FCLASS : SIMT_FLOATr_Base<0b111, 0b111, 0b101, "v.fclass">;

def SIMT_FCLASS_SCAR : SIMT_FLOATr_Base_Scar<0b111, 0b111, 0b101, "l.fclass">;
}

let Format = InstFormatFP in
class SIMT_FLOATr_RS_Base<bits<3> Func, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A<Func, 0b000, Opcode, Type, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;

  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;

  let AsmString = opcstr # "\t$SrcL$SrcLType, $Dst$DstType, $RMode, $sat";
}

class SIMT_FLOATr_RS_Base_Scar<bits<3> Func, bits<3> Opcode, bits<3> Type, string opcstr> :
    V5Inst_SIMT_A_Scar<Func, 0b000, Opcode, Type, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
           RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;

  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;

  let AsmString = opcstr # "\t$SrcL$SrcLType, $Dst$DstType, $RMode, $sat";
}

let Uses = [SIMT_P] in {
def SIMT_FABS   : SIMT_FLOATr_RS_Base<0b000, 0b111, 0b101, "v.fabs">;
def SIMT_FSQRT  : SIMT_FLOATr_RS_Base<0b001, 0b111, 0b101, "v.fsqrt">;
def SIMT_FRECIP : SIMT_FLOATr_RS_Base<0b010, 0b111, 0b101, "v.frecip">;
def SIMT_FEXP   : SIMT_FLOATr_RS_Base<0b011, 0b111, 0b101, "v.fexp">;

def SIMT_FABS_SCAR   : SIMT_FLOATr_RS_Base_Scar<0b000, 0b111, 0b101, "l.fabs">;
def SIMT_FSQRT_SCAR  : SIMT_FLOATr_RS_Base_Scar<0b001, 0b111, 0b101, "l.fsqrt">;
def SIMT_FRECIP_SCAR : SIMT_FLOATr_RS_Base_Scar<0b010, 0b111, 0b101, "l.frecip">;
def SIMT_FEXP_SCAR   : SIMT_FLOATr_RS_Base_Scar<0b011, 0b111, 0b101, "l.fexp">;
}

def : InstAlias<"v.fabs\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FABS SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"v.fsqrt\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FSQRT SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"v.frecip\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FRECIP SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"v.fexp\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FEXP SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"l.fabs\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FABS_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"l.fsqrt\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FSQRT_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"l.frecip\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FRECIP_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"l.fexp\t$SrcL$SrcLType, $Dst$DstType", (SIMT_FEXP_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, 0, 0), 0>;
def : InstAlias<"v.fabs\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FABS SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fsqrt\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FSQRT SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.frecip\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FRECIP SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"v.fexp\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FEXP SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fabs\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FABS_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fsqrt\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FSQRT_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.frecip\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FRECIP_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;
def : InstAlias<"l.fexp\t$SrcL$SrcLType, $Dst$DstType, $RMode", (SIMT_FEXP_SCAR SIMTDstScalarReg:$Dst, SIMTDstRegType:$DstType, SIMTSrcScalarReg:$SrcL, SIMTFloatSrcRegType:$SrcLType, RMode:$RMode, 0), 0>;

multiclass SIMT_fALGO_Pats<SDPatternOperator N, Instruction I> {
  def : Pat<(f64 (N f64:$SrcL)),
              (I (i8 SIMT_DstReg_D.Value), f64:$SrcL, (i8 SIMT_Float_SrcReg_FD.Value), 0, 0)>;
  def : Pat<(f32 (N f32:$SrcL)),
              (I (i8 SIMT_DstReg_W.Value), f32:$SrcL, (i8 SIMT_Float_SrcReg_FS.Value), 0, 0)>;
  def : Pat<(f16 (N f16:$SrcL)),
              (I (i8 SIMT_DstReg_H.Value), f16:$SrcL, (i8 SIMT_Float_SrcReg_FH.Value), 0, 0)>;
}

let Predicates = [isSIMT] in {
defm : SIMT_fALGO_Pats<DivergentUnaryOp<fabs>,   SIMT_FABS>;
defm : SIMT_fALGO_Pats<DivergentUnaryOp<fsqrt>,  SIMT_FSQRT>;
defm : SIMT_fALGO_Pats<DivergentUnaryOp<fexp>,   SIMT_FEXP>;

defm : SIMT_fALGO_Pats<UniformUnaryOp<fabs>,   SIMT_FABS_SCAR>;
defm : SIMT_fALGO_Pats<UniformUnaryOp<fsqrt>,  SIMT_FSQRT_SCAR>;
defm : SIMT_fALGO_Pats<UniformUnaryOp<fexp>,   SIMT_FEXP_SCAR>;
/* recip is a div optimization */
}

multiclass SIMT_Float_Arithmetic_Pat<SDNode N, Instruction I> {
  def : Pat<(f64 (N f64:$Src)), (I SIMT_DstReg_D.Value, f64:$Src, SIMT_Float_SrcReg_FD.Value, 0, 0)>;
  def : Pat<(f32 (N f32:$Src)), (I SIMT_DstReg_W.Value, f32:$Src, SIMT_Float_SrcReg_FS.Value, 0, 0)>;
  def : Pat<(f16 (N f16:$Src)), (I SIMT_DstReg_H.Value, f16:$Src, SIMT_Float_SrcReg_FH.Value, 0, 0)>;
  def : Pat<(bf16 (N bf16:$Src)), (I SIMT_DstReg_H.Value, bf16:$Src, SIMT_Float_SrcReg_FH.Value, 0, 0)>;
}

defm: SIMT_Float_Arithmetic_Pat<linxv5_fabs, SIMT_FABS>;
defm: SIMT_Float_Arithmetic_Pat<linxv5_fsqrt, SIMT_FSQRT>;
defm: SIMT_Float_Arithmetic_Pat<linxv5_fexp, SIMT_FEXP>;

class SIMT_MAXMIN_Base<bits<3> Func, string opcstr>
    : SIMT_INTrr_Base<Func, 0b001, 0b101, 0b101, opcstr>;

class SIMT_MAXMIN_Base_Scar<bits<3> Func, string opcstr>
    : SIMT_INTrr_Base_Scar<Func, 0b001, 0b101, 0b101, opcstr>;

class SIMT_FMAXMIN_Base<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_Base<Func, 0b001, 0b101, 0b101, opcstr>;

class SIMT_FMAXMIN_Base_Scar<bits<3> Func, string opcstr>
    : SIMT_FLOATrr_Base_Scar<Func, 0b001, 0b101, 0b101, opcstr>;

let Uses = [SIMT_P] in {
def SIMT_MAX  : SIMT_MAXMIN_Base <0b000, "v.max">;
def SIMT_FMAX : SIMT_FMAXMIN_Base<0b010, "v.fmax">;
def SIMT_MIN  : SIMT_MAXMIN_Base <0b001, "v.min">;
def SIMT_FMIN : SIMT_FMAXMIN_Base<0b011, "v.fmin">;
def SIMT_MAX_SCAR  : SIMT_MAXMIN_Base_Scar <0b000, "l.max">;
def SIMT_FMAX_SCAR : SIMT_FMAXMIN_Base_Scar<0b010, "l.fmax">;
def SIMT_MIN_SCAR  : SIMT_MAXMIN_Base_Scar <0b001, "l.min">;
def SIMT_FMIN_SCAR : SIMT_FMAXMIN_Base_Scar<0b011, "l.fmin">;
}

def SIMT_MOV: V5Inst_SIMT_D<0b000, 0b011, 0b110, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType), "v.mov">,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcR = 0;
  let SrcRType = 0;
  let AsmString = "v.mov\t$SrcL$SrcLType, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_CSEL: V5Inst_SIMT_A_C<0b000, 0b000, 0b111, 0b011, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcP, SIMTIntSrcRegType:$SrcPType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOp),
      "v.csel">,
      Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcP;
  bits<3> SrcPType;
  let SrcD = SrcP;
  let SrcDType = SrcPType;
  let AsmString = "v.csel\t$SrcP$SrcPType, $SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOp, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_CSEL_SCAR: V5Inst_SIMT_A_C<0b000, 0b000, 0b111, 0b011, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcP, SIMTIntSrcRegType:$SrcPType,
           SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOp),
      "l.csel">,
      Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcP;
  bits<3> SrcPType;
  let SrcD = SrcP;
  let SrcDType = SrcPType;
  let Dst{6-5} = 0b01;
  let Inst{8-7} = 0b01;
  let AsmString = "l.csel\t$SrcP$SrcPType, $SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOp, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_PSEL: V5Inst_SIMT_A_C<0b001, 0b000, 0b111, 0b011, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcP, SIMTIntSrcRegType:$SrcPType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType, SIMTRegOpAU:$SIMTRegOp),
      "v.psel">,
      Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcP;
  bits<3> SrcPType;
  let SrcD = SrcP;
  let SrcDType = SrcPType;
  let AsmString = "v.psel\t$SrcP$SrcPType, $SrcL$SrcLType, $SrcR$SrcRType$SIMTRegOp, $Dst$DstType";
}

multiclass SIMT_PSelect_Pat<ValueType Type, int DstType, int RType, V5Inst_SIMT Inst> {
  def : Pat<(Type (linxv5_merge_cf Type:$SrcL, Type:$SrcR)),
            (Inst DstType, (i64 SIMT_P), (i8 SIMT_Int_SrcReg_SD.Value), Type:$SrcL, RType, Type:$SrcR, RType, 0)>;
}
let Predicates = [isSIMT] in {
  defm : SIMT_PSelect_Pat<i64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<i32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<i16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<i8,   SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<f64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<f32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<f16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_PSEL>;
  defm : SIMT_PSelect_Pat<bf16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_PSEL>;
}

multiclass SIMT_Select_Pat<ValueType Type, int DstType, int RType, V5Inst_SIMT Inst> {
  def : Pat<(Type (select i64:$SrcP, Type:$SrcL, Type:$SrcR)),
            (Inst DstType, i64:$SrcP, (i8 SIMT_Int_SrcReg_SD.Value), Type:$SrcL, RType, Type:$SrcR, RType, 0)>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_Select_Pat<i64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<i32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<i16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<i8,   SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<f64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<f32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<f16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_CSEL>;
  defm : SIMT_Select_Pat<bf16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_CSEL>;
}

class SIMT_SHFL_Base<bits<3> Func, string opcstr> : V5Inst_SIMT_A<Func, 0b000, 0b001, 0b110, (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
             SIMTSrcVecReg:$SrcD, SIMTIntSrcRegType:$SrcDType),
        opcstr, "$SrcL$SrcLType, $SrcR$SrcRType, $SrcD$SrcDType, $Dst$DstType">,
        Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]>;

class SIMT_SHFL_BFLY_Base<bits<3> Func, string opcstr> : V5Inst_SIMT_A<Func, 0b000, 0b001, 0b110, (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcD, SIMTIntSrcRegType:$SrcDType),
        opcstr, "$SrcL$SrcLType, $SrcD$SrcDType, $Dst$DstType">,
        Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  let Inst{24-20} = 0;
  let Inst{56-52} = 0;
}

class SIMT_SHFLI_Base<bits<3> Func, Operand ImmOp, string opcstr> : V5Inst_SIMT_B<Func, 0b000, 0b010, 0b110, (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
             ImmOp:$Imm14),
        opcstr, "$SrcL$SrcLType, $SrcR$SrcRType, $Imm14, $Dst$DstType">,
        Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<14> Imm14;
  let Inst{63-57} = Imm14{6-0};
  let Inst{31-25} = Imm14{13-7};
}

class SIMT_SHFLI_BFLY_Base<bits<3> Func, Operand ImmOp, string opcstr> : V5Inst_SIMT_B<Func, 0b000, 0b010, 0b110, (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             ImmOp:$Imm14),
        opcstr, "$SrcL$SrcLType, $Imm14, $Dst$DstType">,
        Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<14> Imm14;
  let Inst{63-57} = Imm14{6-0};
  let Inst{31-25} = Imm14{13-7};
  let Inst{24-20} = 0;
  let Inst{56-52} = 0;
}

let hasSideEffects = 1, Uses = [SIMT_P] in {
def SIMT_SHFL_UP   : SIMT_SHFL_Base<0b000, "v.shfv.up">;
def SIMT_SHFL_DOWN : SIMT_SHFL_Base<0b001, "v.shfv.down">;
def SIMT_SHFL_BFLY : SIMT_SHFL_BFLY_Base<0b010, "v.shfv.bfly">;
def SIMT_SHFL_IDX  : SIMT_SHFL_Base<0b011, "v.shfv.idx">;
def SIMT_SHFLI_UP   : SIMT_SHFLI_Base<0b000, uimm14i32, "v.shfli.up">;
def SIMT_SHFLI_DOWN : SIMT_SHFLI_Base<0b001, uimm14i32, "v.shfli.down">;
def SIMT_SHFLI_BFLY : SIMT_SHFLI_BFLY_Base<0b010, simm14i32, "v.shfli.bfly">;
def SIMT_SHFLI_IDX  : SIMT_SHFLI_Base<0b011, simm14i32, "v.shfli.idx">;
}

multiclass SIMT_SHFL_Pat_Base<ValueType Type, int DstType, SDPatternOperator N, int R1Type, int R3Type, Operand ImmOp, string Inst> {
  def : Pat<(Type (N Type:$SrcL, i64:$SrcR, i32:$SrcD)),
            (!cast<Instruction>("SIMT_SHFL_" # Inst)
             DstType, Type:$SrcL, R1Type, i64:$SrcR, SIMT_Int_SrcReg_SD.Value, i32:$SrcD, R3Type)>;
  def : Pat<(Type (N Type:$SrcL, i64:$SrcR, ImmOp:$Imm14)),
            (!cast<Instruction>("SIMT_SHFLI_" # Inst)
             DstType, Type:$SrcL, R1Type, i64:$SrcR, SIMT_Int_SrcReg_SD.Value, ImmOp:$Imm14)>;
}

multiclass SIMT_SHFL_BFLY_Pat_Base<ValueType Type, int DstType, SDPatternOperator N, int R1Type, int R3Type, Operand ImmOp, string Inst> {
  def : Pat<(Type (N Type:$SrcL, i32:$SrcD)),
            (!cast<Instruction>("SIMT_SHFL_" # Inst)
             DstType, Type:$SrcL, R1Type, i32:$SrcD, R3Type)>;
  def : Pat<(Type (N Type:$SrcL, ImmOp:$Imm14)),
            (!cast<Instruction>("SIMT_SHFLI_" # Inst)
             DstType, Type:$SrcL, R1Type, ImmOp:$Imm14)>;
}

multiclass SIMT_SHFL_Pat<SDPatternOperator N, int R3Type, Operand ImmOp, string Inst> {
  defm : SIMT_SHFL_Pat_Base<i64,  SIMT_DstReg_D.Value, N, SIMT_Int_SrcReg_SD.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<i32,  SIMT_DstReg_W.Value, N, SIMT_Int_SrcReg_SW.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<i16,  SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<i8,   SIMT_DstReg_B.Value, N, SIMT_Int_SrcReg_SB.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<f64,  SIMT_DstReg_D.Value, N, SIMT_Int_SrcReg_SD.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<f32,  SIMT_DstReg_W.Value, N, SIMT_Int_SrcReg_SW.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<f16,  SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_Pat_Base<bf16, SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
}

multiclass SIMT_SHFL_BFLY_Pat<SDPatternOperator N, int R3Type, Operand ImmOp, string Inst> {
  defm : SIMT_SHFL_BFLY_Pat_Base<i64,  SIMT_DstReg_D.Value, N, SIMT_Int_SrcReg_SD.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<i32,  SIMT_DstReg_W.Value, N, SIMT_Int_SrcReg_SW.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<i16,  SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<i8,   SIMT_DstReg_B.Value, N, SIMT_Int_SrcReg_SB.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<f64,  SIMT_DstReg_D.Value, N, SIMT_Int_SrcReg_SD.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<f32,  SIMT_DstReg_W.Value, N, SIMT_Int_SrcReg_SW.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<f16,  SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
  defm : SIMT_SHFL_BFLY_Pat_Base<bf16, SIMT_DstReg_H.Value, N, SIMT_Int_SrcReg_SH.Value, R3Type, ImmOp, Inst>;
}

let Predicates = [isSIMT] in {
defm : SIMT_SHFL_Pat<linxv5_shfl_up,   SIMT_Int_SrcReg_UW.Value, uimm14i32, "UP">;
defm : SIMT_SHFL_Pat<linxv5_shfl_down, SIMT_Int_SrcReg_UW.Value, uimm14i32, "DOWN">;
defm : SIMT_SHFL_BFLY_Pat<linxv5_shfl_xor,  SIMT_Int_SrcReg_SW.Value, simm14i32, "BFLY">;
defm : SIMT_SHFL_Pat<linxv5_shfl_idx,  SIMT_Int_SrcReg_SW.Value, simm14i32, "IDX">;
}

multiclass SIMT_Float_TwoSrcFloat_Pat<SDPatternOperator N, Instruction I> {
  def : Pat<(f64 (N f64:$Src0, f64:$Src1)), (I SIMT_DstReg_D.Value, f64:$Src0, SIMT_Float_SrcReg_FD.Value, f64:$Src1, SIMT_Float_SrcReg_FD.Value)>;
  def : Pat<(f32 (N f32:$Src0, f32:$Src1)), (I SIMT_DstReg_W.Value, f32:$Src0, SIMT_Float_SrcReg_FS.Value, f32:$Src1, SIMT_Float_SrcReg_FS.Value)>;
  def : Pat<(f16 (N f16:$Src0, f16:$Src1)), (I SIMT_DstReg_H.Value, f16:$Src0, SIMT_Float_SrcReg_FH.Value, f16:$Src1, SIMT_Float_SrcReg_FH.Value)>;
  def : Pat<(bf16 (N bf16:$Src0, bf16:$Src1)), (I SIMT_DstReg_H.Value, bf16:$Src0, SIMT_Float_SrcReg_FH.Value, bf16:$Src1, SIMT_Float_SrcReg_FH.Value)>;
}

multiclass SIMT_Int_TwoSrcFloat_Pat<SDPatternOperator N, Instruction I> {
  def : Pat<(i64 (N i64:$Src0, i64:$Src1)), (I SIMT_DstReg_D.Value, i64:$Src0, SIMT_Int_SrcReg_SD.Value, i64:$Src1, SIMT_Int_SrcReg_SD.Value)>;
  def : Pat<(i32 (N i32:$Src0, i32:$Src1)), (I SIMT_DstReg_W.Value, i32:$Src0, SIMT_Int_SrcReg_SW.Value, i32:$Src1, SIMT_Int_SrcReg_SW.Value)>;
  def : Pat<(i16 (N i16:$Src0, i16:$Src1)), (I SIMT_DstReg_H.Value, i16:$Src0, SIMT_Int_SrcReg_SH.Value, i16:$Src1, SIMT_Int_SrcReg_SH.Value)>;
  def : Pat<(i8 (N i8:$Src0, i8:$Src1)), (I SIMT_DstReg_B.Value, i8:$Src0, SIMT_Int_SrcReg_SB.Value, i8:$Src1, SIMT_Int_SrcReg_SB.Value)>;
}

defm: SIMT_Float_TwoSrcFloat_Pat<DivergentOp<linxv5_max>, SIMT_FMAX>;
defm: SIMT_Int_TwoSrcFloat_Pat<DivergentOp<linxv5_max>, SIMT_MAX>;
defm: SIMT_Float_TwoSrcFloat_Pat<DivergentOp<linxv5_min>, SIMT_FMIN>;
defm: SIMT_Int_TwoSrcFloat_Pat<DivergentOp<linxv5_min>, SIMT_MIN>;

defm: SIMT_Float_TwoSrcFloat_Pat<UniformOp<linxv5_max>, SIMT_FMAX_SCAR>;
defm: SIMT_Int_TwoSrcFloat_Pat<UniformOp<linxv5_max>, SIMT_MAX_SCAR>;
defm: SIMT_Float_TwoSrcFloat_Pat<UniformOp<linxv5_min>, SIMT_FMIN_SCAR>;
defm: SIMT_Int_TwoSrcFloat_Pat<UniformOp<linxv5_min>, SIMT_MIN_SCAR>;

let Uses = [SIMT_P] in
class SIMT_CVT_Base<RegisterOperand DstRegType, RegisterOperand SrcRegType, bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileDstType, bits<5> TileSrcType, string opcstr, string suffix>
    : V5Inst_SIMT_A<Func, 0b000, 0b110, 0b101, (outs DstRegType:$Dst),
      (ins DstTypeOperand:$DstType,
           SrcRegType:$SrcL, SrcTypeOperand:$SrcLType, RMode:$RMode, Sat:$sat), opcstr>,
           Sched<[WriteDafultA, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let Inst{63-59} = TileDstType;
  let Inst{58-57} = RMode{1-0};
  let Inst{31-27} = TileSrcType;
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let AsmString = opcstr # suffix # "\t$SrcL$SrcLType, $Dst$DstType, $RMode, $sat";
}

multiclass SIMT_CVT_DEFS<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileDstType, bits<5> TileSrcType, string opcstr, string suffix> {
  def "" : SIMT_CVT_Base<SIMTDstVecReg, SIMTSrcVecReg, Func, DstTypeOperand, SrcTypeOperand, TileDstType, TileSrcType, "v." # opcstr, suffix>;
  def "_SCAR" : SIMT_CVT_Base<SIMTDstScalarReg, SIMTSrcScalarReg, Func, DstTypeOperand, SrcTypeOperand, TileDstType, TileSrcType, "l." # opcstr, suffix> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
  def : InstAlias<"v.cvt" # suffix # "\t$SrcL$SrcLType, $Dst$DstType", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType, SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, 0, 0), 0>;
  def : InstAlias<"v.cvt" # suffix # "\t$SrcL$SrcLType, $Dst$DstType, $RMode, $sat", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType, SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, RMode:$RMode, Sat:$sat), 0>;
  def : InstAlias<"v." # opcstr # suffix #"\t$SrcL$SrcLType, $Dst$DstType", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType, SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, 0, 0), 0>;
  def : InstAlias< "l." # opcstr # suffix # "\t$SrcL$SrcLType, $Dst$DstType", (!cast<Instruction>(NAME # "_SCAR") SIMTDstScalarReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcScalarReg:$SrcL, SrcTypeOperand:$SrcLType, 0, 0), 0>;
  def : InstAlias<"v." # opcstr # suffix #"\t$SrcL$SrcLType, $Dst$DstType, $RMode", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, RMode:$RMode, 0), 0>;
  def : InstAlias< "l." # opcstr # suffix # "\t$SrcL$SrcLType, $Dst$DstType, $RMode", (!cast<Instruction>(NAME # "_SCAR") SIMTDstScalarReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcScalarReg:$SrcL, SrcTypeOperand:$SrcLType, RMode:$RMode, 0), 0>;
}

multiclass SIMT_CVT_Float<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileSrcType, string opcstr, string suffix> {
  defm "2FP64" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00000, TileSrcType, opcstr, suffix # "2fp64">;
  defm "2FP32" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00001, TileSrcType, opcstr, suffix # "2fp32">;
  defm "2TF32" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00010, TileSrcType, opcstr, suffix # "2tf32">;
  defm "2HF32" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00011, TileSrcType, opcstr, suffix # "2hf32">;
  defm "2FP16" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00100, TileSrcType, opcstr, suffix # "2fp16">;
  defm "2BF16" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00101, TileSrcType, opcstr, suffix # "2bf16">;
  defm "2HiF8" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00110, TileSrcType, opcstr, suffix # "2hif8">;
  defm "2E4M3" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00111, TileSrcType, opcstr, suffix # "2e4m3">;
  defm "2E5M2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01000, TileSrcType, opcstr, suffix # "2e5m2">;
  defm "2E3M2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01001, TileSrcType, opcstr, suffix # "2e3m2">;
  defm "2E2M3" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01010, TileSrcType, opcstr, suffix # "2e2m3">;
  defm "2E8M0" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01110, TileSrcType, opcstr, suffix # "2e8m0">;
  defm "2E6M2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01111, TileSrcType, opcstr, suffix # "2e6m2">;
}

multiclass SIMT_CVT_Int<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileSrcType, string opcstr, string suffix> {
  defm "2U64" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00000, TileSrcType, opcstr, suffix # "2u64">;
  defm "2U32" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00001, TileSrcType, opcstr, suffix # "2u32">;
  defm "2U16" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00010, TileSrcType, opcstr, suffix # "2u16">;
  defm "2U8"  : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00011, TileSrcType, opcstr, suffix # "2u8">;
  defm "2S64" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01000, TileSrcType, opcstr, suffix # "2s64">;
  defm "2S32" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01001, TileSrcType, opcstr, suffix # "2s32">;
  defm "2S16" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01010, TileSrcType, opcstr, suffix # "2s16">;
  defm "2S8"  : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01011, TileSrcType, opcstr, suffix # "2s8">;
}

multiclass SIMT_F_2_F<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_FP64" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".fp64">;
  defm "_FP32" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".fp32">;
  defm "_TF32" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".tf32">;
  defm "_HF32" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".hf32">;
  defm "_FP16" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".fp16">;
  defm "_BF16" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".bf16">;
  defm "_HiF8" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".hif8">;
  defm "_E4M3" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00111, opcstr, ".e4m3">;
  defm "_E5M2" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".e5m2">;
  defm "_E3M2" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".e3m2">;
  defm "_E2M3" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".e2m3">;
  defm "_E8M0" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".e8m0">;
  defm "_E6M2" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01111, opcstr, ".e6m2">;
}

multiclass SIMT_F_2_I<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_FP64" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".fp64">;
  defm "_FP32" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".fp32">;
  defm "_TF32" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".tf32">;
  defm "_HF32" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".hf32">;
  defm "_FP16" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".fp16">;
  defm "_BF16" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".bf16">;
  defm "_HiF8" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".hif8">;
  defm "_E4M3" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00111, opcstr, ".e4m3">;
  defm "_E5M2" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".e5m2">;
  defm "_E3M2" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".e3m2">;
  defm "_E2M3" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".e2m3">;
  defm "_E8M0" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".e8m0">;
  defm "_E6M2" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01111, opcstr, ".e6m2">;
}

multiclass SIMT_I_2_F<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U64" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".u64">;
  defm "_U32" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".u32">;
  defm "_U16" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".u16">;
  defm "_U8"  : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".u8">;
  defm "_S64" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".s64">;
  defm "_S32" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".s32">;
  defm "_S16" : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".s16">;
  defm "_S8"  : SIMT_CVT_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".s8">;
}

multiclass SIMT_I_2_I<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U64" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".u64">;
  defm "_U32" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".u32">;
  defm "_U16" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".u16">;
  defm "_U8"  : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".u8">;
  defm "_S64" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".s64">;
  defm "_S32" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".s32">;
  defm "_S16" : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".s16">;
  defm "_S8"  : SIMT_CVT_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".s8">;
}

defm SIMT_FCVT  : SIMT_F_2_F<0b000, SIMTDstRegType, SIMTFloatSrcRegType, "fcvt">;
defm SIMT_FCVTI : SIMT_F_2_I<0b001, SIMTDstRegType, SIMTFloatSrcRegType, "fcvti">;
defm SIMT_ICVT  : SIMT_I_2_I<0b110, SIMTDstRegType, SIMTIntSrcRegType, "icvt">;
defm SIMT_ICVTF : SIMT_I_2_F<0b111, SIMTDstRegType, SIMTIntSrcRegType, "icvtf">;

multiclass CVT_Pat<ValueType DstTy, SDPatternOperator N, ValueType SrcTy, Instruction I, int DType, int RType> {
  def : Pat<(DstTy (DivergentUnaryOp<N> SrcTy:$Src)), (DstTy (I (i8 DType), SrcTy:$Src, (i8 RType), 0, 0))>;
  def : Pat<(DstTy (UniformUnaryOp<N> SrcTy:$Src)), (DstTy (!cast<Instruction>(I # "_SCAR") (i8 DType), SrcTy:$Src, (i8 RType), 0, 0))>;
}

let Predicates = [isSIMT] in {
defm : CVT_Pat<f64, uint_to_fp, i64, SIMT_ICVTF_U642FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value>;
defm : CVT_Pat<f32, uint_to_fp, i64, SIMT_ICVTF_U642FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UD.Value>;
defm : CVT_Pat<f16, uint_to_fp, i64, SIMT_ICVTF_U642FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UD.Value>;
defm : CVT_Pat<f64, uint_to_fp, i32, SIMT_ICVTF_U322FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UW.Value>;
defm : CVT_Pat<f32, uint_to_fp, i32, SIMT_ICVTF_U322FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value>;
defm : CVT_Pat<f16, uint_to_fp, i32, SIMT_ICVTF_U322FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UW.Value>;
defm : CVT_Pat<f64, uint_to_fp, i16, SIMT_ICVTF_U162FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value>;
defm : CVT_Pat<f32, uint_to_fp, i16, SIMT_ICVTF_U162FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UH.Value>;
defm : CVT_Pat<f16, uint_to_fp, i16, SIMT_ICVTF_U162FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value>;
defm : CVT_Pat<f64, uint_to_fp,  i8, SIMT_ICVTF_U82FP64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UB.Value>;
defm : CVT_Pat<f32, uint_to_fp,  i8, SIMT_ICVTF_U82FP32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UB.Value>;
defm : CVT_Pat<f16, uint_to_fp,  i8, SIMT_ICVTF_U82FP16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UB.Value>;

defm : CVT_Pat<f64, sint_to_fp, i64, SIMT_ICVTF_S642FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value>;
defm : CVT_Pat<f32, sint_to_fp, i64, SIMT_ICVTF_S642FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SD.Value>;
defm : CVT_Pat<f16, sint_to_fp, i64, SIMT_ICVTF_S642FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SD.Value>;
defm : CVT_Pat<f64, sint_to_fp, i32, SIMT_ICVTF_S322FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SW.Value>;
defm : CVT_Pat<f32, sint_to_fp, i32, SIMT_ICVTF_S322FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value>;
defm : CVT_Pat<f16, sint_to_fp, i32, SIMT_ICVTF_S322FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SW.Value>;
defm : CVT_Pat<f64, sint_to_fp, i16, SIMT_ICVTF_S162FP64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<f32, sint_to_fp, i16, SIMT_ICVTF_S162FP32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<f16, sint_to_fp, i16, SIMT_ICVTF_S162FP16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<f64, sint_to_fp,  i8, SIMT_ICVTF_S82FP64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<f32, sint_to_fp,  i8, SIMT_ICVTF_S82FP32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<f16, sint_to_fp,  i8, SIMT_ICVTF_S82FP16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SB.Value>;

defm : CVT_Pat<i64, fp_to_uint, f64, SIMT_FCVTI_FP642U64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i64, fp_to_uint, f32, SIMT_FCVTI_FP322U64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i64, fp_to_uint, f16, SIMT_FCVTI_FP162U64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i32, fp_to_uint, f64, SIMT_FCVTI_FP642U32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i32, fp_to_uint, f32, SIMT_FCVTI_FP322U32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i32, fp_to_uint, f16, SIMT_FCVTI_FP162U32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i16, fp_to_uint, f64, SIMT_FCVTI_FP642U16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i16, fp_to_uint, f32, SIMT_FCVTI_FP322U16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i16, fp_to_uint, f16, SIMT_FCVTI_FP162U16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i8,  fp_to_uint, f64, SIMT_FCVTI_FP642U8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i8,  fp_to_uint, f32, SIMT_FCVTI_FP322U8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i8,  fp_to_uint, f16, SIMT_FCVTI_FP162U8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FH.Value>;

defm : CVT_Pat<i64, fp_to_sint, f64, SIMT_FCVTI_FP642S64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i64, fp_to_sint, f32, SIMT_FCVTI_FP322S64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i64, fp_to_sint, f16, SIMT_FCVTI_FP162S64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i32, fp_to_sint, f64, SIMT_FCVTI_FP642S32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i32, fp_to_sint, f32, SIMT_FCVTI_FP322S32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i32, fp_to_sint, f16, SIMT_FCVTI_FP162S32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i16, fp_to_sint, f64, SIMT_FCVTI_FP642S16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i16, fp_to_sint, f32, SIMT_FCVTI_FP322S16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i16, fp_to_sint, f16, SIMT_FCVTI_FP162S16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<i8,  fp_to_sint, f64, SIMT_FCVTI_FP642S8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<i8,  fp_to_sint, f32, SIMT_FCVTI_FP322S8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<i8,  fp_to_sint, f16, SIMT_FCVTI_FP162S8,  SIMT_DstReg_B.Value, SIMT_Float_SrcReg_FH.Value>;

defm : CVT_Pat<f64, any_fpextend, f32, SIMT_FCVT_FP322FP64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<f32, any_fpround,  f64, SIMT_FCVT_FP642FP32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FD.Value>;
defm : CVT_Pat<f32, any_fpextend, f16, SIMT_FCVT_FP162FP32, SIMT_DstReg_W.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<f16, any_fpround,  f32, SIMT_FCVT_FP322FP16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FS.Value>;
defm : CVT_Pat<f64, any_fpextend, f16, SIMT_FCVT_FP162FP64, SIMT_DstReg_D.Value, SIMT_Float_SrcReg_FH.Value>;
defm : CVT_Pat<f16, any_fpround,  f64, SIMT_FCVT_FP642FP16, SIMT_DstReg_H.Value, SIMT_Float_SrcReg_FD.Value>;

defm : CVT_Pat<i64, zext, i32, SIMT_ICVT_U322U64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UW.Value>;
defm : CVT_Pat<i64, zext, i16, SIMT_ICVT_U162U64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value>;
defm : CVT_Pat<i32, zext, i16, SIMT_ICVT_U162U32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UH.Value>;
defm : CVT_Pat<i64, zext, i8,  SIMT_ICVT_U82U64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UB.Value>;
defm : CVT_Pat<i32, zext, i8,  SIMT_ICVT_U82U32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UB.Value>;
defm : CVT_Pat<i16, zext, i8,  SIMT_ICVT_U82U16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UB.Value>;

defm : CVT_Pat<i64, sext, i32, SIMT_ICVT_S322S64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SW.Value>;
defm : CVT_Pat<i64, sext, i16, SIMT_ICVT_S162S64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<i32, sext, i16, SIMT_ICVT_S162S32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<i64, sext, i8,  SIMT_ICVT_S82S64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<i32, sext, i8,  SIMT_ICVT_S82S32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<i16, sext, i8,  SIMT_ICVT_S82S16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SB.Value>;

defm : CVT_Pat<i64, anyext, i32, SIMT_ICVT_S322S64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SW.Value>;
defm : CVT_Pat<i64, anyext, i16, SIMT_ICVT_S162S64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<i32, anyext, i16, SIMT_ICVT_S162S32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SH.Value>;
defm : CVT_Pat<i64, anyext, i8,  SIMT_ICVT_S82S64,  SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<i32, anyext, i8,  SIMT_ICVT_S82S32,  SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SB.Value>;
defm : CVT_Pat<i16, anyext, i8,  SIMT_ICVT_S82S16,  SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SB.Value>;

def : Pat<(i32 (DivergentUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S32 (i8 SIMT_DstReg_W.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i16 (DivergentUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S16 (i8 SIMT_DstReg_H.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i16 (DivergentUnaryOp<trunc> i32:$SrcL)), (SIMT_ICVT_S322S16 (i8 SIMT_DstReg_H.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
def : Pat<(i8 (DivergentUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S8 (i8 SIMT_DstReg_B.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i8 (DivergentUnaryOp<trunc> i32:$SrcL)), (SIMT_ICVT_S322S8 (i8 SIMT_DstReg_B.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
def : Pat<(i8 (DivergentUnaryOp<trunc> i16:$SrcL)), (SIMT_ICVT_S162S8 (i8 SIMT_DstReg_B.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), 0, 0)>;

def : Pat<(i32 (UniformUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S32_SCAR (i8 SIMT_DstReg_W.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i16 (UniformUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S16_SCAR (i8 SIMT_DstReg_H.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i16 (UniformUnaryOp<trunc> i32:$SrcL)), (SIMT_ICVT_S322S16_SCAR (i8 SIMT_DstReg_H.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
def : Pat<(i8 (UniformUnaryOp<trunc> i64:$SrcL)), (SIMT_ICVT_S642S8_SCAR (i8 SIMT_DstReg_B.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
def : Pat<(i8 (UniformUnaryOp<trunc> i32:$SrcL)), (SIMT_ICVT_S322S8_SCAR (i8 SIMT_DstReg_B.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
def : Pat<(i8 (UniformUnaryOp<trunc> i16:$SrcL)), (SIMT_ICVT_S162S8_SCAR (i8 SIMT_DstReg_B.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_SH.Value), 0, 0)>;

def : Pat<(i64 (bitconvert f64:$SrcL)), (i64 (COPY f64:$SrcL))>;
def : Pat<(f64 (bitconvert i64:$SrcL)), (f64 (COPY i64:$SrcL))>;
def : Pat<(i32 (bitconvert f32:$SrcL)), (i32 (COPY f32:$SrcL))>;
def : Pat<(f32 (bitconvert i32:$SrcL)), (f32 (COPY i32:$SrcL))>;
def : Pat<(i16 (bitconvert f16:$SrcL)), (i16 (COPY f16:$SrcL))>;
def : Pat<(f16 (bitconvert i16:$SrcL)), (f16 (COPY i16:$SrcL))>;
def : Pat<(i16 (bitconvert bf16:$SrcL)), (i16 (COPY bf16:$SrcL))>;
def : Pat<(bf16 (bitconvert i16:$SrcL)), (bf16 (COPY i16:$SrcL))>;
}

//===----------------------------------------------------------------------===//
// Packed CVT (new x2/x4 instruction family)
//===----------------------------------------------------------------------===//

let Uses = [SIMT_P] in
class SIMT_CVT_PACKED_Base< RegisterOperand DstRegType, RegisterOperand SrcRegType, bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileDstType, bits<5> TileSrcType, string opcstr, string suffix>
    : V5Inst_SIMT_A<Func, 0b000, 0b110, 0b101, (outs DstRegType:$Dst),
      (ins DstTypeOperand:$DstType, SrcRegType:$SrcL, SrcTypeOperand:$SrcLType, SrcRegType:$SrcR, SrcTypeOperand:$SrcRType, RMode:$RMode, Sat:$sat), opcstr>,
      Sched<[WriteDafultA, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;

  let Inst{63-59} = TileDstType;
  let Inst{58-57} = RMode{1-0};
  let Inst{31-27} = TileSrcType;
  let Inst{25} = RMode{2};
  let Inst{12} = sat;

  let AsmString = opcstr # suffix # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode, $sat";
}

multiclass SIMT_CVT_PACKED_DEFS<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileDstType, bits<5> TileSrcType, string opcstr, string suffix> {
  def "" : SIMT_CVT_PACKED_Base<SIMTDstVecReg, SIMTSrcVecReg, Func, DstTypeOperand, SrcTypeOperand, TileDstType, TileSrcType, "v." # opcstr, suffix>;
  def "_SCAR" : SIMT_CVT_PACKED_Base<SIMTDstScalarReg, SIMTSrcScalarReg, Func, DstTypeOperand, SrcTypeOperand, TileDstType, TileSrcType, "l." # opcstr, suffix> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
  def : InstAlias< "v.cvt" # suffix # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType, SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType,
                  SIMTSrcVecReg:$SrcR, SrcTypeOperand:$SrcRType, 0, 0), 0>;
  def : InstAlias< "v.cvt" # suffix # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode, $sat", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType, SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType,
                  SIMTSrcVecReg:$SrcR, SrcTypeOperand:$SrcRType, RMode:$RMode, Sat:$sat), 0>;
  def : InstAlias<"v." # opcstr # suffix #"\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, SIMTSrcVecReg:$SrcR, SrcTypeOperand:$SrcRType, 0, 0), 0>;
  def : InstAlias< "l." # opcstr # suffix # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType", (!cast<Instruction>(NAME # "_SCAR") SIMTDstScalarReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcScalarReg:$SrcL, SrcTypeOperand:$SrcLType, SIMTSrcScalarReg:$SrcR, SrcTypeOperand:$SrcRType, 0, 0), 0>;
  def : InstAlias<"v." # opcstr # suffix #"\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (!cast<Instruction>(NAME) SIMTDstVecReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcVecReg:$SrcL, SrcTypeOperand:$SrcLType, SIMTSrcVecReg:$SrcR, SrcTypeOperand:$SrcRType, RMode:$RMode, 0), 0>;
  def : InstAlias< "l." # opcstr # suffix # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType, $RMode", (!cast<Instruction>(NAME # "_SCAR") SIMTDstScalarReg:$Dst, DstTypeOperand:$DstType,
                  SIMTSrcScalarReg:$SrcL, SrcTypeOperand:$SrcLType, SIMTSrcScalarReg:$SrcR, SrcTypeOperand:$SrcRType, RMode:$RMode, 0), 0>;
}

multiclass SIMT_CVT_PACKED_Float<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileSrcType, string opcstr, string suffix> {
  defm "2E2M1x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01011, TileSrcType, opcstr, suffix # "2e2m1x2">;
  defm "2E1M2x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01100, TileSrcType, opcstr, suffix # "2e1m2x2">;
  defm "2HiF4x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01101, TileSrcType, opcstr, suffix # "2hif4x2">;
  defm "2FP16x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10000, TileSrcType, opcstr, suffix # "2fp16x2">;
  defm "2BF16x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10001, TileSrcType, opcstr, suffix # "2bf16x2">;
  defm "2E4M3x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10010, TileSrcType, opcstr, suffix # "2e4m3x4">;
  defm "2E5M2x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10011, TileSrcType, opcstr, suffix # "2e5m2x4">;
  defm "2E4M3x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10100, TileSrcType, opcstr, suffix # "2e4m3x2">;
  defm "2E5M2x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10101, TileSrcType, opcstr, suffix # "2e5m2x2">;
  defm "2E6M2x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10110, TileSrcType, opcstr, suffix # "2e6m2x2">;
}

multiclass SIMT_CVT_PACKED_Int< bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, bits<5> TileSrcType, string opcstr, string suffix> {
  defm "2U4x2"  : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00100, TileSrcType, opcstr, suffix # "2u4x2">;
  defm "2U16x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00101, TileSrcType, opcstr, suffix # "2u16x2">;
  defm "2U8x4"  : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00110, TileSrcType, opcstr, suffix # "2u8x4">;
  defm "2S4x2"  : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01100, TileSrcType, opcstr, suffix # "2s4x2">;
  defm "2S16x2" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01101, TileSrcType, opcstr, suffix # "2s16x2">;
  defm "2S8x4"  : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01110, TileSrcType, opcstr, suffix # "2s8x4">;
}

multiclass SIMT_F_2_F_PACKED<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_FP64" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".fp64">;
  defm "_FP32" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".fp32">;
  defm "_TF32" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".tf32">;
  defm "_HF32" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".hf32">;
  defm "_FP16" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".fp16">;
  defm "_BF16" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".bf16">;
  defm "_HiF8" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".hif8">;
  defm "_E4M3" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00111, opcstr, ".e4m3">;
  defm "_E5M2" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".e5m2">;
  defm "_E3M2" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".e3m2">;
  defm "_E2M3" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".e2m3">;
  defm "_E8M0" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".e8m0">;
  defm "_E6M2" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01111, opcstr, ".e6m2">;
}

multiclass SIMT_F_2_I_PACKED<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_FP64" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".fp64">;
  defm "_FP32" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".fp32">;
  defm "_TF32" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".tf32">;
  defm "_HF32" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".hf32">;
  defm "_FP16" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".fp16">;
  defm "_BF16" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".bf16">;
  defm "_HiF8" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".hif8">;
  defm "_E4M3" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00111, opcstr, ".e4m3">;
  defm "_E5M2" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".e5m2">;
  defm "_E3M2" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".e3m2">;
  defm "_E2M3" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".e2m3">;
  defm "_E8M0" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".e8m0">;
  defm "_E6M2" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01111, opcstr, ".e6m2">;
}

multiclass SIMT_I_2_F_PACKED<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U64"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".u64">;
  defm "_U32"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".u32">;
  defm "_U16"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".u16">;
  defm "_U8"    : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".u8">;
  defm "_U4x2"  : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".u4x2">;
  defm "_U16x2" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".u16x2">;
  defm "_U8x4"  : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".u8x4">;
  defm "_S64"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".s64">;
  defm "_S32"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".s32">;
  defm "_S16"   : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".s16">;
  defm "_S8"    : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".s8">;
  defm "_S4x2"  : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".s4x2">;
  defm "_S16x2" : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".s16x2">;
  defm "_S8x4"  : SIMT_CVT_PACKED_Float<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".s8x4">;
}

multiclass SIMT_I_2_I_PACKED<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U64"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00000, opcstr, ".u64">;
  defm "_U32"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00001, opcstr, ".u32">;
  defm "_U16"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00010, opcstr, ".u16">;
  defm "_U8"    : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00011, opcstr, ".u8">;
  defm "_U4x2"  : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".u4x2">;
  defm "_U16x2" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".u16x2">;
  defm "_U8x4"  : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".u8x4">;
  defm "_S64"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01000, opcstr, ".s64">;
  defm "_S32"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01001, opcstr, ".s32">;
  defm "_S16"   : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01010, opcstr, ".s16">;
  defm "_S8"    : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".s8">;
  defm "_S4x2"  : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".s4x2">;
  defm "_S16x2" : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".s16x2">;
  defm "_S8x4"  : SIMT_CVT_PACKED_Int<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".s8x4">;
}

defm SIMT_FCVT_PACKED  : SIMT_F_2_F_PACKED<0b000, SIMTDstRegType, SIMTFloatSrcRegType, "fcvt">;
defm SIMT_FCVTI_PACKED : SIMT_F_2_I_PACKED<0b001, SIMTDstRegType, SIMTFloatSrcRegType, "fcvti">;
defm SIMT_ICVT_PACKED  : SIMT_I_2_I_PACKED<0b110, SIMTDstRegType, SIMTIntSrcRegType, "icvt">;
defm SIMT_ICVTF_PACKED : SIMT_I_2_F_PACKED<0b111, SIMTDstRegType, SIMTIntSrcRegType, "icvtf">;

//===----------------------------------------------------------------------===//
// Packed-to-Packed CVT
// one packed src -> one packed dst
// Reuse SIMT_CVT_DEFS because encoding/operands are same as one-src CVT.
//===----------------------------------------------------------------------===//

multiclass SIMT_CVT_P2P_FloatDst_X2<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2E2M1x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01011, TileSrcType, opcstr, suffix # "2e2m1x2">;
  defm "2E1M2x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01100, TileSrcType, opcstr, suffix # "2e1m2x2">;
  defm "2HiF4x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01101, TileSrcType, opcstr, suffix # "2hif4x2">;
  defm "2FP16x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10000, TileSrcType, opcstr, suffix # "2fp16x2">;
  defm "2BF16x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10001, TileSrcType, opcstr, suffix # "2bf16x2">;
  defm "2E4M3x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10100, TileSrcType, opcstr, suffix # "2e4m3x2">;
  defm "2E5M2x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10101, TileSrcType, opcstr, suffix # "2e5m2x2">;
  defm "2E6M2x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10110, TileSrcType, opcstr, suffix # "2e6m2x2">;
}

multiclass SIMT_CVT_P2P_IntDst_X2<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2U4x2"  : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00100, TileSrcType, opcstr, suffix # "2u4x2">;
  defm "2U16x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00101, TileSrcType, opcstr, suffix # "2u16x2">;
  defm "2S4x2"  : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01100, TileSrcType, opcstr, suffix # "2s4x2">;
  defm "2S16x2" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01101, TileSrcType, opcstr, suffix # "2s16x2">;
}

multiclass SIMT_CVT_P2P_FloatDst_X4<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2E4M3x4" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10010, TileSrcType, opcstr, suffix # "2e4m3x4">;
  defm "2E5M2x4" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10011, TileSrcType, opcstr, suffix # "2e5m2x4">;
}

multiclass SIMT_CVT_P2P_IntDst_X4<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2U8x4" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00110, TileSrcType, opcstr, suffix # "2u8x4">;
  defm "2S8x4" : SIMT_CVT_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01110, TileSrcType, opcstr, suffix # "2s8x4">;
}

// packed float x2 -> packed float x2
multiclass SIMT_P2P_F_2_F_X2<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_E2M1x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".e2m1x2">;
  defm "_E1M2x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".e1m2x2">;
  defm "_HiF4x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".hif4x2">;
  defm "_FP16x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10000, opcstr, ".fp16x2">;
  defm "_BF16x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10001, opcstr, ".bf16x2">;
  defm "_E4M3x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10100, opcstr, ".e4m3x2">;
  defm "_E5M2x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10101, opcstr, ".e5m2x2">;
  defm "_E6M2x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10110, opcstr, ".e6m2x2">;
}

// packed float x2 -> packed int x2
multiclass SIMT_P2P_F_2_I_X2<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_E2M1x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".e2m1x2">;
  defm "_E1M2x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".e1m2x2">;
  defm "_HiF4x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".hif4x2">;
  defm "_FP16x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10000, opcstr, ".fp16x2">;
  defm "_BF16x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10001, opcstr, ".bf16x2">;
  defm "_E4M3x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10100, opcstr, ".e4m3x2">;
  defm "_E5M2x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10101, opcstr, ".e5m2x2">;
  defm "_E6M2x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b10110, opcstr, ".e6m2x2">;
}

// packed int x2 -> packed float x2
multiclass SIMT_P2P_I_2_F_X2<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U4x2"  : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".u4x2">;
  defm "_U16x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".u16x2">;
  defm "_S4x2"  : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".s4x2">;
  defm "_S16x2" : SIMT_CVT_P2P_FloatDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".s16x2">;
}

// packed int x2 -> packed int x2
multiclass SIMT_P2P_I_2_I_X2<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U4x2"  : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b00100, opcstr, ".u4x2">;
  defm "_U16x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b00101, opcstr, ".u16x2">;
  defm "_S4x2"  : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".s4x2">;
  defm "_S16x2" : SIMT_CVT_P2P_IntDst_X2<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".s16x2">;
}

// packed float x4 -> packed float x4
multiclass SIMT_P2P_F_2_F_X4<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_E4M3x4" : SIMT_CVT_P2P_FloatDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b10010, opcstr, ".e4m3x4">;
  defm "_E5M2x4" : SIMT_CVT_P2P_FloatDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b10011, opcstr, ".e5m2x4">;
}

// packed float x4 -> packed int x4
multiclass SIMT_P2P_F_2_I_X4<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_E4M3x4" : SIMT_CVT_P2P_IntDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b10010, opcstr, ".e4m3x4">;
  defm "_E5M2x4" : SIMT_CVT_P2P_IntDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b10011, opcstr, ".e5m2x4">;
}

// packed int x4 -> packed float x4
multiclass SIMT_P2P_I_2_F_X4<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U8x4" : SIMT_CVT_P2P_FloatDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".u8x4">;
  defm "_S8x4" : SIMT_CVT_P2P_FloatDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".s8x4">;
}

// packed int x4 -> packed int x4
multiclass SIMT_P2P_I_2_I_X4<bits<3> Func, Operand DstTypeOperand, Operand SrcTypeOperand, string opcstr> {
  defm "_U8x4" : SIMT_CVT_P2P_IntDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b00110, opcstr, ".u8x4">;
  defm "_S8x4" : SIMT_CVT_P2P_IntDst_X4<Func, DstTypeOperand, SrcTypeOperand, 0b01110, opcstr, ".s8x4">;
}

defm SIMT_FCVT_P2P_X2  : SIMT_P2P_F_2_F_X2<0b000, SIMTDstRegType, SIMTFloatSrcRegType, "fcvt">;
defm SIMT_FCVTI_P2P_X2 : SIMT_P2P_F_2_I_X2<0b001, SIMTDstRegType, SIMTFloatSrcRegType, "fcvti">;
defm SIMT_ICVT_P2P_X2  : SIMT_P2P_I_2_I_X2<0b110, SIMTDstRegType, SIMTIntSrcRegType, "icvt">;
defm SIMT_ICVTF_P2P_X2 : SIMT_P2P_I_2_F_X2<0b111, SIMTDstRegType, SIMTIntSrcRegType, "icvtf">;

defm SIMT_FCVT_P2P_X4  : SIMT_P2P_F_2_F_X4<0b000, SIMTDstRegType, SIMTFloatSrcRegType, "fcvt">;
defm SIMT_FCVTI_P2P_X4 : SIMT_P2P_F_2_I_X4<0b001, SIMTDstRegType, SIMTFloatSrcRegType, "fcvti">;
defm SIMT_ICVT_P2P_X4  : SIMT_P2P_I_2_I_X4<0b110, SIMTDstRegType, SIMTIntSrcRegType, "icvt">;
defm SIMT_ICVTF_P2P_X4 : SIMT_P2P_I_2_F_X4<0b111, SIMTDstRegType, SIMTIntSrcRegType, "icvtf">;

//===----------------------------------------------------------------------===//
// Two packed-float x2 src -> one packed x4 dst
// Example: fp16x2, fp16x2 -> e4m3x4
// This only adds packed float x2 sources.
// Packed int x2 sources are already covered by existing SIMT_I_2_*_PACKED.
//===----------------------------------------------------------------------===//

multiclass SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2E4M3x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10010, TileSrcType, opcstr, suffix # "2e4m3x4">;
  defm "2E5M2x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b10011, TileSrcType, opcstr, suffix # "2e5m2x4">;
}

multiclass SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    bits<5> TileSrcType,
    string opcstr,
    string suffix> {
  defm "2U8x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b00110, TileSrcType, opcstr, suffix # "2u8x4">;
  defm "2S8x4" : SIMT_CVT_PACKED_DEFS<Func, DstTypeOperand, SrcTypeOperand, 0b01110, TileSrcType, opcstr, suffix # "2s8x4">;
}

multiclass SIMT_F_2_F_PACKED_X2_TO_X4<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    string opcstr> {
  defm "_E2M1x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".e2m1x2">;
  defm "_E1M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".e1m2x2">;
  defm "_HiF4x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".hif4x2">;
  defm "_FP16x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10000, opcstr, ".fp16x2">;
  defm "_BF16x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10001, opcstr, ".bf16x2">;
  defm "_E4M3x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10100, opcstr, ".e4m3x2">;
  defm "_E5M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10101, opcstr, ".e5m2x2">;
  defm "_E6M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_FloatX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10110, opcstr, ".e6m2x2">;
}

multiclass SIMT_F_2_I_PACKED_X2_TO_X4<
    bits<3> Func,
    Operand DstTypeOperand,
    Operand SrcTypeOperand,
    string opcstr> {
  defm "_E2M1x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01011, opcstr, ".e2m1x2">;
  defm "_E1M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01100, opcstr, ".e1m2x2">;
  defm "_HiF4x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b01101, opcstr, ".hif4x2">;
  defm "_FP16x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10000, opcstr, ".fp16x2">;
  defm "_BF16x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10001, opcstr, ".bf16x2">;
  defm "_E4M3x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10100, opcstr, ".e4m3x2">;
  defm "_E5M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10101, opcstr, ".e5m2x2">;
  defm "_E6M2x2" : SIMT_CVT_PACKED_FloatX2Src_2_IntX4Dst<Func, DstTypeOperand, SrcTypeOperand, 0b10110, opcstr, ".e6m2x2">;
}

defm SIMT_FCVT_PACKED_X2_TO_X4
    : SIMT_F_2_F_PACKED_X2_TO_X4<0b000, SIMTDstRegType, SIMTFloatSrcRegType, "fcvt">;

defm SIMT_FCVTI_PACKED_X2_TO_X4
    : SIMT_F_2_I_PACKED_X2_TO_X4<0b001, SIMTDstRegType, SIMTFloatSrcRegType, "fcvti">;

//============================================================================================//
multiclass SIMT_ALUrrI_ReserveShmat_ICVT_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$Src)), uimm6:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value),
                  i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value),
                  uimm6:$SrcR)>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<DivergentOp<srl>, SIMT_SRLI>;
  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<DivergentOp<sra>, SIMT_SRAI>;
  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<DivergentOp<shl>, SIMT_SLLI>;

  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<UniformOp<srl>, SIMT_SRLI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<UniformOp<sra>, SIMT_SRAI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_ICVT_Pat<UniformOp<shl>, SIMT_SLLI_SCAR>;
}

multiclass SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$Src)), uimm6:$SrcR)),
            (Inst (i8 DType),
                  SourceVT:$Src, (i8 SourceRType),
                  uimm6:$SrcR)>;
}

multiclass SIMT_Base_RR_Shift_Extend_RIO_CVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_Shift_ZeroExtend_RIO_CVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_RR_Shift_Extend_RIO_CVT_Pat<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_RR_Shift_Extend_RIO_CVT_Pat<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_RR_Shift_ZeroExtend_RIO_CVT_Pat<N, I, RIORegUnaryOp<zext>>;
}

let Predicates = [isSIMT] in {
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<DivergentOp<srl>, SIMT_SRLI>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<DivergentOp<sra>, SIMT_SRAI>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<DivergentOp<shl>, SIMT_SLLI>;

  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<UniformOp<srl>, SIMT_SRLI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<UniformOp<sra>, SIMT_SRAI_SCAR>;
  defm : SIMT_ALUrrI_ReserveShmat_RIO_ICVT_High_Pat<UniformOp<shl>, SIMT_SLLI_SCAR>;
}

class PatGprImm_ICVT<SDPatternOperator OpNode, V5Inst_SIMT Inst, ValueType Type, Operand ImmOp, int DstType, int RType>
    : Pat<(Type (OpNode (Type (LoopRegUnaryOp<zext> i16:$Src)), ImmOp:$imm)),
          (Inst DstType, i16:$Src, RType, ImmOp:$imm)>;

multiclass PatGprSimm12_ICVT<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : PatGprImm_ICVT<OpNode, Inst, i64, simm12, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value>;
  def : PatGprImm_ICVT<OpNode, Inst, i32, simm12, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UH.Value>;
}

multiclass PatGprUimm12_ICVT<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : PatGprImm_ICVT<OpNode, Inst, i64, uimm12, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value>;
}

let Predicates = [isSIMT] in {
  defm : PatGprUimm12_ICVT<DivergentOp<add>, SIMT_ADDI>;
  defm : PatGprUimm12_ICVT<DivergentOp<sub>, SIMT_SUBI>;
  defm : PatGprSimm12_ICVT<DivergentOp<and>, SIMT_ANDI>;
  defm : PatGprSimm12_ICVT<DivergentOp<or>, SIMT_ORI>;
  defm : PatGprSimm12_ICVT<DivergentOp<xor>, SIMT_XORI>;
}

class PatGprImm_RIO_ICVT<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int SourceRType, Instruction Inst, PatFrag IcvtOp, Operand ImmOp>
    : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$Src)), ImmOp:$imm)),
          (Inst DType, SourceVT:$Src, SourceRType, ImmOp:$imm)>;

multiclass PatGprImm12_RIO_ICVT<SDPatternOperator N, Instruction I, PatFrag IcvtOp, Operand ImmOp> {
  def : PatGprImm_RIO_ICVT<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp, ImmOp>;
}

multiclass PatGprImm12_RIO_ZeroExtend_ICVT<SDPatternOperator N, Instruction I, PatFrag IcvtOp, Operand ImmOp> {
  def : PatGprImm_RIO_ICVT<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp, ImmOp>;
  def : PatGprImm_RIO_ICVT<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp, ImmOp>;
}

multiclass PatGprImm_RIO_ICVT_High_Pat<SDPatternOperator N, Instruction I, Operand ImmOp> {
  defm : PatGprImm12_RIO_ICVT<N, I, RIORegUnaryOp<sext>, ImmOp>;
  defm : PatGprImm12_RIO_ICVT<N, I, RIORegUnaryOp<anyext>, ImmOp>;
  defm : PatGprImm12_RIO_ZeroExtend_ICVT<N, I, RIORegUnaryOp<zext>, ImmOp>;
}

let Predicates = [isSIMT] in {
  defm : PatGprImm_RIO_ICVT_High_Pat<DivergentOp<add>, SIMT_ADDI, uimm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<DivergentOp<sub>, SIMT_SUBI, uimm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<DivergentOp<and>, SIMT_ANDI, simm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<DivergentOp<or>, SIMT_ORI, simm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<DivergentOp<xor>, SIMT_XORI, simm12>;

  defm : PatGprImm_RIO_ICVT_High_Pat<UniformOp<add>, SIMT_ADDI_SCAR, uimm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<UniformOp<sub>, SIMT_SUBI, uimm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<UniformOp<and>, SIMT_ANDI, simm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<UniformOp<or>, SIMT_ORI, simm12>;
  defm : PatGprImm_RIO_ICVT_High_Pat<UniformOp<xor>, SIMT_XORI, simm12>;
}

multiclass SIMT_Base_RR_ICVT_Pat<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$Src)), i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0)>;
  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (LoopRegUnaryOp<zext> i16:$Src)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0)>;

  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$Src)), i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0)>;
  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (LoopRegUnaryOp<zext> i16:$Src)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0)>;

  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$SrcL)), (i32 (LoopRegUnaryOp<zext> i16:$SrR)))),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$SrR, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0)>;
}

multiclass SIMT_Base_RR_ICVT_Pat_S<SDPatternOperator OpNode, V5Inst_SIMT Inst> {
  def : Pat<(i64 (OpNode (i64 (LoopRegUnaryOp<zext> i16:$Src)), i64:$SrcR)),
            (Inst (i8 SIMT_DstReg_D.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i64:$SrcR, (i8 SIMT_Int_SrcReg_SD.Value), 0, 0, 0)>;
  def : Pat<(i64 (OpNode i64:$SrcL, (i64 (LoopRegUnaryOp<zext> i16:$Src)))),
            (Inst (i8 SIMT_DstReg_D.Value), i64:$SrcL, (i8 SIMT_Int_SrcReg_SD.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0, 0)>;

  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$Src)), i32:$SrcR)),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), i32:$SrcR, (i8 SIMT_Int_SrcReg_SW.Value), 0, 0, 0)>;
  def : Pat<(i32 (OpNode i32:$SrcL, (i32 (LoopRegUnaryOp<zext> i16:$Src)))),
            (Inst (i8 SIMT_DstReg_W.Value), i32:$SrcL, (i8 SIMT_Int_SrcReg_SW.Value), i16:$Src, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0, 0)>;

  def : Pat<(i32 (OpNode (i32 (LoopRegUnaryOp<zext> i16:$SrcL)), (i32 (LoopRegUnaryOp<zext> i16:$SrR)))),
            (Inst (i8 SIMT_DstReg_W.Value), i16:$SrcL, (i8 SIMT_Int_SrcReg_UH.Value), i16:$SrR, (i8 SIMT_Int_SrcReg_UH.Value), 0, 0, 0)>;
}

let Predicates = [isSIMT] in {
defm : SIMT_Base_RR_ICVT_Pat_S<DivergentOp<add>, SIMT_ADD>;
defm : SIMT_Base_RR_ICVT_Pat_S<DivergentOp<sub>, SIMT_SUB>;
defm : SIMT_Base_RR_ICVT_Pat<DivergentOp<and>, SIMT_AND>;
defm : SIMT_Base_RR_ICVT_Pat<DivergentOp<or>, SIMT_OR>;
defm : SIMT_Base_RR_ICVT_Pat<DivergentOp<xor>, SIMT_XOR>;

}

multiclass SIMT_Base_RR_RIO_ICVT_Pat<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$Src)), VT:$SrcR)),
            (Inst (i8 DType), SourceVT:$Src, (i8 SourceRType), VT:$SrcR, (i8 RType), 0, 0)>;
  def : Pat<(VT (OpNode VT:$SrcL, (VT (IcvtOp SourceVT:$Src)))),
            (Inst (i8 DType), VT:$SrcL, (i8 RType), SourceVT:$Src, (i8 SourceRType), 0, 0)>;
}

multiclass SIMT_Base_RR_RIO_Extend_ICVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_RIO_ZeroExtend_ICVT_Pat<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_RIO_ICVT_High_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_RR_RIO_Extend_ICVT_Pat<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_RR_RIO_Extend_ICVT_Pat<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_RR_RIO_ZeroExtend_ICVT_Pat<N, I, RIORegUnaryOp<zext>>;
}

let Predicates = [isSIMT] in {
defm : SIMT_Base_RR_RIO_ICVT_High_Pat<DivergentOp<and>, SIMT_AND>;
defm : SIMT_Base_RR_RIO_ICVT_High_Pat<DivergentOp<or>, SIMT_OR>;
defm : SIMT_Base_RR_RIO_ICVT_High_Pat<DivergentOp<xor>, SIMT_XOR>;

defm : SIMT_Base_RR_ICVT_Pat<DivergentOp<linxv5_merge_cf>, SIMT_OR>;

defm : SIMT_Base_RR_RIO_ICVT_High_Pat<UniformOp<and>, SIMT_AND_SCAR>;
defm : SIMT_Base_RR_RIO_ICVT_High_Pat<UniformOp<or>, SIMT_OR_SCAR>;
defm : SIMT_Base_RR_RIO_ICVT_High_Pat<UniformOp<xor>, SIMT_XOR_SCAR>;

defm : SIMT_Base_RR_ICVT_Pat<UniformOp<linxv5_merge_cf>, SIMT_OR_SCAR>;
}

multiclass SIMT_Base_RR_RIO_ICVT_Pat_RS<SDPatternOperator OpNode, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction Inst, PatFrag IcvtOp> {
  def : Pat<(VT (OpNode (VT (IcvtOp SourceVT:$Src)), VT:$SrcR)), (Inst (i8 DType), SourceVT:$Src, (i8 SourceRType), VT:$SrcR, (i8 RType), 0, 0, 0)>;
  def : Pat<(VT (OpNode VT:$SrcL, (VT (IcvtOp SourceVT:$Src)))), (Inst (i8 DType), VT:$SrcL, (i8 RType), SourceVT:$Src, (i8 SourceRType), 0, 0, 0)>;
}

multiclass SIMT_Base_RR_RIO_Extend_ICVT_Pat_RS<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_RIO_ZeroExtend_ICVT_Pat_RS<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_Base_RR_RIO_ICVT_Pat_RS<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_Base_RR_RIO_ICVT_High_Pat_RS<SDPatternOperator N, Instruction I> {
  defm : SIMT_Base_RR_RIO_Extend_ICVT_Pat_RS<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_Base_RR_RIO_Extend_ICVT_Pat_RS<N, I, RIORegUnaryOp<anyext>>;
  defm : SIMT_Base_RR_RIO_ZeroExtend_ICVT_Pat_RS<N, I, RIORegUnaryOp<zext>>;
}
let Predicates = [isSIMT] in {
  defm : SIMT_Base_RR_RIO_ICVT_High_Pat_RS<DivergentOp<add>, SIMT_ADD>;
  defm : SIMT_Base_RR_RIO_ICVT_High_Pat_RS<DivergentOp<sub>, SIMT_SUB>;

  defm : SIMT_Base_RR_RIO_ICVT_High_Pat_RS<UniformOp<add>, SIMT_ADD_SCAR>;
  defm : SIMT_Base_RR_RIO_ICVT_High_Pat_RS<UniformOp<sub>, SIMT_SUB_SCAR>;
}

let Uses = [SIMT_P] in
def SIMT_QPUSH : V5Inst_SIMT_A<0b001, 0b000, 0b111, 0b110, (outs SIMTDstVecReg:$Dst),
    (ins SIMTDstRegType:$DstType,
         SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
         SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType),
    "qpush">,
    Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = "v.qpush\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_QPOP : V5Inst_SIMT_A<0b010, 0b000, 0b111, 0b110, (outs SIMTDstVecReg:$Dst),
    (ins SIMTDstRegType:$DstType,
         SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType),
    "qpop">,
    Sched<[WriteDafultA, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let AsmString = "v.qpop\t$SrcL$SrcLType, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_QPUSH_SCAR : V5Inst_SIMT_A_Scar<0b001, 0b000, 0b111, 0b110, (outs SIMTDstScalarReg:$Dst),
    (ins SIMTDstRegType:$DstType,
         SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
         SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType),
    "qpush">,
    Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let AsmString = "l.qpush\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

let Uses = [SIMT_P] in
def SIMT_QPOP_SCAR : V5Inst_SIMT_A_Scar<0b010, 0b000, 0b111, 0b110, (outs SIMTDstScalarReg:$Dst),
    (ins SIMTDstRegType:$DstType,
         SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType),
    "qpop">,
    Sched<[WriteDafultA, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let AsmString = "l.qpop\t$SrcL$SrcLType, $Dst$DstType";
}

class SIMT_DIVrr_Base<bits<3> Func, string opcstr>
    : V5Inst_SIMT_D<Func, 0b101, 0b011, (outs SIMTDstVecReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

class SIMT_DIVrr_Base_Scar<bits<3> Func, string opcstr>
    : V5Inst_SIMT_D_Scar<Func, 0b101, 0b011, (outs SIMTDstScalarReg:$Dst),
      (ins SIMTDstRegType:$DstType,
           SIMTSrcScalarReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SIMTSrcScalarReg:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr>,
           Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let AsmString = opcstr # "\t$SrcL$SrcLType, $SrcR$SrcRType, $Dst$DstType";
}

let Uses = [SIMT_P] in {
def SIMT_DIV  : SIMT_DIVrr_Base<0b000, "v.div">;
def SIMT_REM  : SIMT_DIVrr_Base<0b100, "v.rem">;

def SIMT_DIV_SCAR  : SIMT_DIVrr_Base_Scar<0b000, "l.div">;
def SIMT_REM_SCAR  : SIMT_DIVrr_Base_Scar<0b100, "l.rem">;
}

multiclass SIMT_DIVREM_Pat_Base<SDPatternOperator N, ValueType VT, int DType, int RType, Instruction I> {
  def : Pat<(VT (N VT:$SrcL, VT:$SrcR)), (I (i8 DType), VT:$SrcL, (i8 RType), VT:$SrcR, (i8 RType))>;
}

multiclass SIMT_DIVREM_ICVT_Pat_Base<SDPatternOperator N, ValueType VT, ValueType SourceVT, int DType, int RType, int SourceRType, Instruction I, PatFrag IcvtOp> {
  def : Pat<(VT (N VT:$SrcL, (VT (IcvtOp SourceVT:$Src)))), (I (i8 DType), VT:$SrcL, (i8 RType), SourceVT:$Src, (i8 SourceRType))>;
}

multiclass SIMT_DIVREM_ICVT_SExtend_Pat_Base<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SW.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SH.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, SIMT_Int_SrcReg_SB.Value, I, IcvtOp>;
}

multiclass SIMT_DIVREM_ICVT_UExtend_Pat_Base<SDPatternOperator N, Instruction I, PatFrag IcvtOp> {
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i32, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UW.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i16, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i64, i8, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i32, i16, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UH.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i32, i8, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
  defm : SIMT_DIVREM_ICVT_Pat_Base<N, i16, i8, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, SIMT_Int_SrcReg_UB.Value, I, IcvtOp>;
}

multiclass SIMT_SDIVREM_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_DIVREM_Pat_Base<N, i64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_SD.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_SW.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_SH.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i8,  SIMT_DstReg_B.Value, SIMT_Int_SrcReg_SB.Value, I>;
  defm : SIMT_DIVREM_ICVT_SExtend_Pat_Base<N, I, RIORegUnaryOp<sext>>;
  defm : SIMT_DIVREM_ICVT_SExtend_Pat_Base<N, I, RIORegUnaryOp<anyext>>;
}

multiclass SIMT_UDIVREM_Pat<SDPatternOperator N, Instruction I> {
  defm : SIMT_DIVREM_Pat_Base<N, i64, SIMT_DstReg_D.Value, SIMT_Int_SrcReg_UD.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i32, SIMT_DstReg_W.Value, SIMT_Int_SrcReg_UW.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i16, SIMT_DstReg_H.Value, SIMT_Int_SrcReg_UH.Value, I>;
  defm : SIMT_DIVREM_Pat_Base<N, i8,  SIMT_DstReg_B.Value, SIMT_Int_SrcReg_UB.Value, I>;
  defm : SIMT_DIVREM_ICVT_UExtend_Pat_Base<N, I, RIORegUnaryOp<zext>>;
}

let Predicates = [isSIMT] in {
defm : SIMT_SDIVREM_Pat<DivergentOp<sdiv>, SIMT_DIV>;
defm : SIMT_SDIVREM_Pat<DivergentOp<srem>, SIMT_REM>;
defm : SIMT_UDIVREM_Pat<DivergentOp<udiv>, SIMT_DIV>;
defm : SIMT_UDIVREM_Pat<DivergentOp<urem>, SIMT_REM>;

defm : SIMT_SDIVREM_Pat<UniformOp<sdiv>, SIMT_DIV_SCAR>;
defm : SIMT_SDIVREM_Pat<UniformOp<srem>, SIMT_REM_SCAR>;
defm : SIMT_UDIVREM_Pat<UniformOp<udiv>, SIMT_DIV_SCAR>;
defm : SIMT_UDIVREM_Pat<UniformOp<urem>, SIMT_REM_SCAR>;
}

// ===----------------------- Atomic Instructions --------------------------------

class SIMT_ATOMIC_LOAD_BASE_VS<RegisterOperand DstRegType, RegisterOperand SrcRegType, bits<3> funcHi, bits<3> funcLo, bits<4> Order, string opcstr>
    : V5Inst_SIMT_B<funcHi, funcLo, 0b000, 0b101, (outs DstRegType:$Dst),
      (ins SIMTDstRegType:$DstType,
           SrcRegType:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SrcRegType:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr> {
  let Inst{63-59} = 0;
  let Inst{58-57} = Order{3-2};
  let Inst{26-25} = Order{1-0};
  let AsmString = opcstr # "\t[$SrcL$SrcLType], $SrcR$SrcRType, $Dst$DstType";
}

multiclass SIMT_ATOMIC_LOAD_BASE<bits<3> FuncHi, bits<3> FuncLo, bits<4> Order, string opcstr> {
  def _VEC : SIMT_ATOMIC_LOAD_BASE_VS<SIMTDstVecReg, SIMTSrcVecReg, FuncHi, FuncLo, Order, "v." # opcstr>;
  def _SCAR: SIMT_ATOMIC_LOAD_BASE_VS<SIMTDstScalarReg, SIMTSrcScalarReg, FuncHi, FuncLo, Order, "l." # opcstr> {
    let Dst{6-5} = 0b01;
    let Inst{8-7} = 0b01;
  }
}

multiclass SIMT_ATOMIC_LOAD_ORDER<bits<3> FuncHi, bits<3> FuncLo, string opcstr> {
  defm ""      : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b0000, opcstr>;
  defm _AQ     : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b1000, opcstr # ".aq">;
  defm _RL     : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b0100, opcstr # ".rl">;
  defm _FAR     : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b0001, opcstr # ".f">;
  defm _AQRL   : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b1100, opcstr # ".aqrl">;
  defm _AQF   : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b1001, opcstr # ".aqf">;
  defm _RLF   : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b0101, opcstr # ".rlf">;
  defm _AQRLF : SIMT_ATOMIC_LOAD_BASE<FuncHi, FuncLo, 0b1101, opcstr # ".aqrlf">;
}

multiclass SIMT_ATOMIC_LOAD_METHOD<bits<3> FuncLo, string opcstr> {
  defm _ADD : SIMT_ATOMIC_LOAD_ORDER<0b000, FuncLo, opcstr # ".add">;
  defm _AND : SIMT_ATOMIC_LOAD_ORDER<0b001, FuncLo, opcstr # ".and">;
  defm _OR  : SIMT_ATOMIC_LOAD_ORDER<0b010, FuncLo, opcstr # ".or">;
  defm _XOR : SIMT_ATOMIC_LOAD_ORDER<0b011, FuncLo, opcstr # ".xor">;
  defm _MAX : SIMT_ATOMIC_LOAD_ORDER<0b100, FuncLo, opcstr # ".max">;
  defm _MIN : SIMT_ATOMIC_LOAD_ORDER<0b101, FuncLo, opcstr # ".min">;
}

let Uses = [SIMT_P] in {
defm SIMT_LW : SIMT_ATOMIC_LOAD_METHOD<0b000, "lw">;
defm SIMT_LD : SIMT_ATOMIC_LOAD_METHOD<0b010, "ld">;
}

class SIMT_ATOMIC_STORE_BASE_VS<RegisterOperand SrcRegType, bits<3> funcHi, bits<3> funcLo, bits<4> Order, string opcstr>
    : V5Inst_SIMT_B<funcHi, funcLo, 0b000, 0b101, (outs),
      (ins SrcRegType:$SrcL, SIMTIntSrcRegType:$SrcLType,
           SrcRegType:$SrcR, SIMTIntSrcRegType:$SrcRType), opcstr> {
  let Dst = 0;
  let DstType = 0;

  let Inst{63-59} = 0;
  let Inst{58-57} = Order{3-2};
  let Inst{26-25} = Order{1-0};
  let AsmString = opcstr # "\t[$SrcL$SrcLType], $SrcR$SrcRType";
}

multiclass SIMT_ATOMIC_STORE_BASE<bits<3> FuncHi, bits<3> FuncLo, bits<4> Order, string opcstr> {
  def _VEC : SIMT_ATOMIC_STORE_BASE_VS<SIMTSrcVecReg, FuncHi, FuncLo, Order, "v." # opcstr>;
  def _SCAR: SIMT_ATOMIC_STORE_BASE_VS<SIMTSrcScalarReg, FuncHi, FuncLo, Order, "l." # opcstr> {
    let Inst{8-7} = 0b01;
  }
}

multiclass SIMT_ATOMIC_STORE_ORDER<bits<3> FuncHi, bits<3> FuncLo, string opcstr> {
  defm ""      : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0000, opcstr>;
  defm _RL     : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0100, opcstr # ".rl">;
  defm _RD     : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0010, opcstr # ".rd">;
  defm _F     : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0001, opcstr # ".f">;
  defm _RLRD   : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0110, opcstr # ".rlrd">;
  defm _RLF   : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0101, opcstr # ".rlf">;
  defm _RDF   : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0011, opcstr # ".rdf">;
  defm _RLRDF   : SIMT_ATOMIC_STORE_BASE<FuncHi, FuncLo, 0b0111, opcstr # ".rlrdf">;
}

multiclass SIMT_ATOMIC_STORE_METHOD<bits<3> FuncLo, string opcstr> {
  defm _ADD : SIMT_ATOMIC_STORE_ORDER<0b000, FuncLo, opcstr # ".add">;
  defm _AND : SIMT_ATOMIC_STORE_ORDER<0b001, FuncLo, opcstr # ".and">;
  defm _OR  : SIMT_ATOMIC_STORE_ORDER<0b010, FuncLo, opcstr # ".or">;
  defm _XOR : SIMT_ATOMIC_STORE_ORDER<0b011, FuncLo, opcstr # ".xor">;
  defm _MAX : SIMT_ATOMIC_STORE_ORDER<0b100, FuncLo, opcstr # ".max">;
  defm _MIN : SIMT_ATOMIC_STORE_ORDER<0b101, FuncLo, opcstr # ".min">;
}

let Uses = [SIMT_P] in {
defm SIMT_SW : SIMT_ATOMIC_STORE_METHOD<0b001, "sw">;
defm SIMT_SD : SIMT_ATOMIC_STORE_METHOD<0b011, "sd">;
}

// ===----------------------- Pseudo Instrunctions -------------------------------

let hasSideEffects = 0, mayLoad = 0, mayStore = 0, Uses = [SIMT_P] in {
def SIMT_PseudoADDTPC_HI : Pseudo<(outs SIMTDstVecReg:$DstR), (ins linxbaresymbol:$label, linxbaresymbol:$addrhi), []> {
  let isNotDuplicable = 1;
}
}

// ===----------------------- Scalar Instructions in SIMT -------------------------

class PatRR_uni<SDPatternOperator N, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, i64:$SrcR), (I i64:$SrcL, i64:$SrcR)>;
class PatRX_uni<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, (X i64:$SrcR)), (I i64:$SrcL, i64:$SrcR)>;
let AddedComplexity = 6 in
class PatRI_uni<SDPatternOperator N, SDPatternOperator ImmOp, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, ImmOp:$Imm), (I i64:$SrcL, ImmOp:$Imm)>;
class PatRR0_uni<SDPatternOperator N, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, i64:$SrcR), (I i64:$SrcL, i64:$SrcR, (i64 0))>;
class PatRX0_uni<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, (X i64:$SrcR)), (I i64:$SrcL, i64:$SrcR, (i64 0))>;
class PatRRS_uni<SDPatternOperator N, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, (worth_shlext i64:$SrcR, ShamtImm:$Shamt)), (I i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt)>;
class PatRXS_uni<SDPatternOperator N, SDPatternOperator X, Instruction I> :
    Pat<(UniformOp<N> i64:$SrcL, (worth_shlext (X i64:$SrcR), ShamtImm:$Shamt)), (I i64:$SrcL, i64:$SrcR, ShamtImm:$Shamt)>;

multiclass ALUPatNONE_uni<SDPatternOperator N, Instruction I> {
  def : PatRR0_uni<N, I>;
  def : PatRRS_uni<N, I>;
}

multiclass ALUPat_uni<SDPatternOperator N, SDPatternOperator X, Instruction I> {
  def : PatRX0_uni<N, X, I>;
  def : PatRXS_uni<N, X, I>;
}

multiclass ExtPat_uni<SDPatternOperator N, Instruction I> {
  def : Pat<(N i64:$Src), (I (i64 R0), i64:$Src, (i64 0))>;
}

multiclass AddSubPat_uni<SDPatternOperator N, Instruction Irr, Instruction Iri, Instruction Irni> {
  defm : ALUPatNONE_uni<N, Irr>;
  defm : ALUPat_uni<N, uw_ext, !cast<Instruction>(Irr # "_UW")>;
  defm : ALUPat_uni<N, worth_sw_ext, !cast<Instruction>(Irr # "_SW")>;
  defm : ALUPat_uni<N, neg_ext, !cast<Instruction>(Irr # "_NEG")>;
  def : PatRI_uni<N, uimm12, Iri>;
  def : PatRI_uni<N, neg_uimm12, Irni>;
}

multiclass BitPat_uni<SDPatternOperator N, Instruction Irr, Instruction Iri> {
  defm : ALUPatNONE_uni<N, Irr>;
  defm : ALUPat_uni<N, uw_ext, !cast<Instruction>(Irr # "_UW")>;
  defm : ALUPat_uni<N, worth_sw_ext, !cast<Instruction>(Irr # "_SW")>;
  defm : ALUPat_uni<N, not_ext, !cast<Instruction>(Irr # "_NOT")>;
  def : PatRI_uni<N, simm12, Iri>;
}

multiclass CMPPat_uni<SDPatternOperator N, SDPatternOperator ImmOp, Instruction Irr, Instruction Iri> {
  def : PatRR_uni<N, Irr>;
  def : PatRX_uni<N, uw_ext, !cast<Instruction>(Irr # _UW)>;
  def : PatRX_uni<N, worth_sw_ext, !cast<Instruction>(Irr # _SW)>;
  def : PatRI_uni<N, ImmOp, Iri>;
}

class linxv5_setcc_swap_uni<SDPatternOperator N> :
    PatFrag<(ops node:$lhs, node:$rhs), (UniformOp<N> node:$rhs, node:$lhs)>;

let Predicates = [isSIMT] in {
def : Pat<(br bb:$Imm12), (J bb:$Imm12)>;
def : Pat<(brcond i64:$cond, bb:$Imm12), (BNE i64:$cond, (i64 R0), bb:$Imm12)>;
}

//===----------------------------------------------------------------------===//
// SIMT vector multi-element instructions
// Inst{14-13} = vlen
// vlen = 0b01 => x2
// vlen = 0b10 => x4
// Only v.* instructions are added here. No l.* / _SCAR instructions.
// x2/x4 is appended only after typed source operands.
//===----------------------------------------------------------------------===//

class SIMT_VLenSuffix<bits<2> vlen> {
  string VLenSuffix = !if(!eq(vlen, 0b01), "x2",
                      !if(!eq(vlen, 0b10), "x4", ""));
}

//===----------------------------------------------------------------------===//
// Integer ALU: v.add/v.sub
//===----------------------------------------------------------------------===//

class SIMT_AUrr_T_N_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_B_T<Func, 0b000, 0b000, 0b010,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
             SIMTRegOpAU:$SIMTRegOpcode,
             SIMTShamtImm<0>:$Shamt,
             Sat:$sat),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<1> sat;

  let Inst{12} = sat;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];
  let isReMaterializable = 1;

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix
             # "$SIMTRegOpcode$Shamt, "
             # "$Dst$DstType, $sat";
}

multiclass SIMT_AUrr_T_N_VLEN_Alias<bits<3> Func, bits<2> vlen,
                                    string suffix, string opcstr> {
  def "" : SIMT_AUrr_T_N_VLEN<Func, vlen, opcstr>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", "
           # "${SrcR}${SrcRType}" # suffix # "$SIMTRegOpcode$Shamt, "
           # "$Dst$DstType",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
      SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
      SIMTRegOpAU:$SIMTRegOpcode,
      SIMTShamtImm<0>:$Shamt,
      0),
    0>;
}

//===----------------------------------------------------------------------===//
// Integer logical: v.and/v.or/v.xor
//===----------------------------------------------------------------------===//

class SIMT_LUrr_T_N_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_B_T<Func, 0b000, 0b000, 0b010,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
             SIMTRegOpLU:$SIMTRegOpcode,
             SIMTShamtImm<0>:$Shamt),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix
             # "$SIMTRegOpcode$Shamt, "
             # "$Dst$DstType";
}

//===----------------------------------------------------------------------===//
// Integer shifts and compare rr
//===----------------------------------------------------------------------===//

class SIMT_ALUrr_ReserveShmat_VLEN<bits<3> Func, bits<3> Opcode,
                                   bits<2> vlen, string opcstr>
    : SIMT_ALUrr_ReserveShmat<SIMTDstVecReg, SIMTSrcVecReg,
                               Func, Opcode, opcstr>,
      SIMT_VLenSuffix<vlen> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

class SIMT_ALUrr_ReserveShmat_CMP_P_VLEN<bits<3> Func, bits<3> Opcode,
                                         bits<2> vlen, string opcstr>
    : SIMT_ALUrr_ReserveShmat_CMP_P<Func, Opcode, opcstr>,
      SIMT_VLenSuffix<vlen> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", ->p";
}

multiclass SIMT_ALUrr_CMP_ReserveShmat_VLEN<bits<3> Func, bits<3> Opcode,
                                            bits<2> vlen, string opcstr> {
  def "" : SIMT_ALUrr_ReserveShmat_VLEN<Func, Opcode, vlen, opcstr>;

  let Defs = [SIMT_P] in
  def "_P" : SIMT_ALUrr_ReserveShmat_CMP_P_VLEN<Func, Opcode, vlen, opcstr>;
}

//===----------------------------------------------------------------------===//
// Integer immediate
//===----------------------------------------------------------------------===//

class SIMT_ALUarithri_VLEN<bits<3> Func, bits<3> Opcode, Operand ImmOp,
                           bits<2> vlen, string opcstr>
    : V5Inst_SIMT_F<Func, 0b000, Opcode, 0b010,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             ImmOp:$Imm12),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];
  let Format = InstFormat_ADDI;

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Imm12, "
             # "$Dst$DstType";
}

class SIMT_ALUshiftri_VLEN<bits<3> Func, bits<3> Opcode,
                           bits<2> vlen, string opcstr>
    : V5Inst_SIMT_F_Shiftri<Func, Opcode, 0b010,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             uimm6:$Shamt),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Shamt, "
             # "$Dst$DstType";
}

//===----------------------------------------------------------------------===//
// Integer compare immediate
//===----------------------------------------------------------------------===//

class SIMT_ALUarithri_CMP_VLEN<bits<3> Func, bits<3> Opcode, Operand ImmOp,
                               bits<2> vlen, string opcstr>
    : SIMT_ALUarithri_CMP<SIMTDstVecReg, SIMTSrcVecReg,
                          Func, Opcode, ImmOp, opcstr>,
      SIMT_VLenSuffix<vlen> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];
  let Format = InstFormat_ADDI;

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Imm12, "
             # "$Dst$DstType";
}

class SIMT_ALUarithri_CMP_P_VLEN<bits<3> Func, bits<3> Opcode, Operand ImmOp,
                                 bits<2> vlen, string opcstr>
    : SIMT_ALUarithri_CMP_P<Func, Opcode, ImmOp, opcstr>,
      SIMT_VLenSuffix<vlen> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];
  let Format = InstFormat_ADDI;

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Imm12, ->p";
}

multiclass SIMT_ALUarithri_CMP_VLEN_VP<bits<3> Func, bits<3> Opcode,
                                       Operand ImmOp, bits<2> vlen,
                                       string opcstr> {
  def "" : SIMT_ALUarithri_CMP_VLEN<Func, Opcode, ImmOp, vlen, opcstr>;

  let Defs = [SIMT_P] in
  def "_P" : SIMT_ALUarithri_CMP_P_VLEN<Func, Opcode, ImmOp, vlen, opcstr>;
}

//===----------------------------------------------------------------------===//
// Integer multiply/add/div/rem
//===----------------------------------------------------------------------===//

class SIMT_INTrr_Base_VLEN<bits<3> Func, bits<3> FuncLo, bits<3> Opcode,
                           bits<3> Type, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, FuncLo, Opcode, Type,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

class SIMT_MUL_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : SIMT_INTrr_Base_VLEN<Func, 0b000, 0b100, 0b011, vlen, opcstr>;

class SIMT_MADD_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, 0b000, 0b100, 0b011,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType,
             SIMTSrcVecReg:$SrcD, SIMTIntSrcRegType:$SrcDType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "${SrcD}${SrcDType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

class SIMT_DIVrr_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_D<Func, 0b101, 0b011,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

//===----------------------------------------------------------------------===//
// Integer max/min
//===----------------------------------------------------------------------===//

class SIMT_MAXMIN_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, 0b001, 0b101, 0b101,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTIntSrcRegType:$SrcRType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

//===----------------------------------------------------------------------===//
// Floating rr with RMode/Sat
//===----------------------------------------------------------------------===//

let Format = InstFormatFP in
class SIMT_FLOATrr_RS_Base_VLEN<bits<3> Func, bits<3> FucLo,
                                bits<3> Opcode, bits<3> Type,
                                bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, FucLo, Opcode, Type,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
             RMode:$RMode, Sat:$sat),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType, $RMode, $sat";
}

class SIMT_FARITHrr_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : SIMT_FLOATrr_RS_Base_VLEN<Func, 0b000, 0b100, 0b101, vlen, opcstr>;

multiclass SIMT_FARITHrr_VLEN<bits<3> Func, bits<2> vlen,
                              string suffix, string opcstr> {
  def "" : SIMT_FARITHrr_Base_VLEN<Func, vlen, opcstr>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", "
           # "${SrcR}${SrcRType}" # suffix # ", "
           # "$Dst$DstType",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
      0, 0),
    0>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", "
           # "${SrcR}${SrcRType}" # suffix # ", "
           # "$Dst$DstType, $RMode",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
      RMode:$RMode, 0),
    0>;
}

//===----------------------------------------------------------------------===//
// Floating rrr with RMode/Sat
//===----------------------------------------------------------------------===//

let Format = InstFormatFP in
class SIMT_FARITHrrr_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, 0b000, 0b100, 0b101,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
             SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType,
             RMode:$RMode, Sat:$sat),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult, ReadDafult]> {
  bits<7> SrcA;
  bits<3> SrcAType;
  bits<3> RMode;
  bits<1> sat;

  let SrcD = SrcA;
  let SrcDType = SrcAType;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "${SrcA}${SrcAType}" # VLenSuffix # ", "
             # "$Dst$DstType, $RMode, $sat";
}

multiclass SIMT_FARITHrrr_VLEN<bits<3> Func, bits<2> vlen,
                               string suffix, string opcstr> {
  def "" : SIMT_FARITHrrr_Base_VLEN<Func, vlen, opcstr>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", "
           # "${SrcR}${SrcRType}" # suffix # ", "
           # "${SrcA}${SrcAType}" # suffix # ", "
           # "$Dst$DstType",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
      SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType,
      0, 0),
    0>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", "
           # "${SrcR}${SrcRType}" # suffix # ", "
           # "${SrcA}${SrcAType}" # suffix # ", "
           # "$Dst$DstType, $RMode",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType,
      SIMTSrcVecReg:$SrcA, SIMTFloatSrcRegType:$SrcAType,
      RMode:$RMode, 0),
    0>;
}

//===----------------------------------------------------------------------===//
// Floating rr without RMode/Sat
//===----------------------------------------------------------------------===//

let Format = InstFormatFP in
class SIMT_FLOATrr_Base_VLEN<bits<3> Func, bits<3> FucLo,
                             bits<3> Opcode, bits<3> Type,
                             bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, FucLo, Opcode, Type,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
             SIMTSrcVecReg:$SrcR, SIMTFloatSrcRegType:$SrcRType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "${SrcR}${SrcRType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

class SIMT_FCMPrr_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : SIMT_FLOATrr_Base_VLEN<Func, 0b000, 0b101, 0b101, vlen, opcstr>;

class SIMT_FMAXMIN_Base_VLEN<bits<3> Func, bits<2> vlen, string opcstr>
    : SIMT_FLOATrr_Base_VLEN<Func, 0b001, 0b101, 0b101, vlen, opcstr>;

//===----------------------------------------------------------------------===//
// Floating unary with RMode/Sat
//===----------------------------------------------------------------------===//

let Format = InstFormatFP in
class SIMT_FLOATr_RS_Base_VLEN<bits<3> Func, bits<3> Opcode,
                               bits<3> Type, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, 0b000, Opcode, Type,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
             RMode:$RMode, Sat:$sat),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  bits<3> RMode;
  bits<1> sat;

  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let Inst{58-57} = RMode{1-0};
  let Inst{25} = RMode{2};
  let Inst{12} = sat;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Dst$DstType, $RMode, $sat";
}

multiclass SIMT_FLOATr_RS_VLEN<bits<3> Func, bits<3> Opcode, bits<3> Type,
                               bits<2> vlen, string suffix, string opcstr> {
  def "" : SIMT_FLOATr_RS_Base_VLEN<Func, Opcode, Type, vlen, opcstr>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", $Dst$DstType",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      0, 0),
    0>;

  def : InstAlias<
    opcstr # "\t${SrcL}${SrcLType}" # suffix # ", $Dst$DstType, $RMode",
    (!cast<Instruction>(NAME)
      SIMTDstVecReg:$Dst, SIMTDstRegType:$DstType,
      SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType,
      RMode:$RMode, 0),
    0>;
}

//===----------------------------------------------------------------------===//
// Floating unary without RMode/Sat
//===----------------------------------------------------------------------===//

let Format = InstFormatFP in
class SIMT_FLOATr_Base_VLEN<bits<3> Func, bits<3> Opcode,
                            bits<3> Type, bits<2> vlen, string opcstr>
    : V5Inst_SIMT_A<Func, 0b000, Opcode, Type,
        (outs SIMTDstVecReg:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult, ReadDafult]> {
  let SrcD = 0;
  let SrcDType = 0;
  let SrcR = 0;
  let SrcRType = 0;
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

//===----------------------------------------------------------------------===//
// Reduce integer
//===----------------------------------------------------------------------===//

class SIMT_Reduce_VLEN<bits<3> funcHi, bits<3> funcLo,
                       bits<2> vlen, string opcstr>
    : V5Inst_SIMT_F_RD<funcHi, funcLo, 0b000, 0b110,
        (outs SIMTDstRegReduce:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

multiclass SIMT_Reduce_VLEN_MC<bits<3> FuncHi, bits<3> FuncLo,
                               bits<2> vlen, string opcstr> {
  let Format = InstFormat_REDUCE in
  def "" : SIMT_Reduce_VLEN<FuncHi, FuncLo, vlen, opcstr>, P2I<NAME, 1>;

  let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
      usesCustomInserter = 1, Format = InstFormat_REDUCE,
      Uses = [SIMT_P] in
  def _Pseudo : Pseudo<(outs),
      (ins SIMTDstRegReduce:$Dst, SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTIntSrcRegType:$SrcLType)>,
      P2I<NAME, 0>;
}

//===----------------------------------------------------------------------===//
// Reduce floating
//===----------------------------------------------------------------------===//

class SIMT_Reduce_F_VLEN<bits<3> funcHi, bits<3> funcLo,
                         bits<2> vlen, string opcstr>
    : V5Inst_SIMT_F_RD<funcHi, funcLo, 0b000, 0b110,
        (outs SIMTDstRegReduce:$Dst),
        (ins SIMTDstRegType:$DstType,
             SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType),
        opcstr>,
      SIMT_VLenSuffix<vlen>,
      Sched<[WriteDafultA, ReadDafult]> {
  let Inst{14-13} = vlen;
  let Uses = [SIMT_P];

  let AsmString =
      opcstr # "\t${SrcL}${SrcLType}" # VLenSuffix # ", "
             # "$Dst$DstType";
}

multiclass SIMT_Reduce_F_VLEN_MC<bits<3> FuncHi, bits<3> FuncLo,
                                 bits<2> vlen, string opcstr> {
  let Format = InstFormat_REDUCE in
  def "" : SIMT_Reduce_F_VLEN<FuncHi, FuncLo, vlen, opcstr>, P2I<NAME, 1>;

  let hasSideEffects = 0, mayLoad = 0, mayStore = 0,
      usesCustomInserter = 1, Format = InstFormat_REDUCE,
      Uses = [SIMT_P] in
  def _Pseudo : Pseudo<(outs),
      (ins SIMTDstRegReduce:$Dst, SIMTDstRegType:$DstType,
           SIMTSrcVecReg:$SrcL, SIMTFloatSrcRegType:$SrcLType)>,
      P2I<NAME, 0>;
}

//===----------------------------------------------------------------------===//
// x2 instruction definitions
//===----------------------------------------------------------------------===//

defm SIMT_ADD_X2 : SIMT_AUrr_T_N_VLEN_Alias<0b000, 0b01, "x2", "v.add">;
defm SIMT_SUB_X2 : SIMT_AUrr_T_N_VLEN_Alias<0b001, 0b01, "x2", "v.sub">;

def SIMT_AND_X2 : SIMT_LUrr_T_N_VLEN<0b010, 0b01, "v.and">;
def SIMT_OR_X2  : SIMT_LUrr_T_N_VLEN<0b011, 0b01, "v.or">;
def SIMT_XOR_X2 : SIMT_LUrr_T_N_VLEN<0b100, 0b01, "v.xor">;

def SIMT_SRL_X2 : SIMT_ALUrr_ReserveShmat_VLEN<0b101, 0b000, 0b01, "v.srl">;
def SIMT_SRA_X2 : SIMT_ALUrr_ReserveShmat_VLEN<0b110, 0b000, 0b01, "v.sra">;
def SIMT_SLL_X2 : SIMT_ALUrr_ReserveShmat_VLEN<0b111, 0b000, 0b01, "v.sll">;

def SIMT_ADDI_X2 : SIMT_ALUarithri_VLEN<0b000, 0b001, uimm12, 0b01, "v.addi">;
def SIMT_SUBI_X2 : SIMT_ALUarithri_VLEN<0b001, 0b001, uimm12, 0b01, "v.subi">;
def SIMT_ANDI_X2 : SIMT_ALUarithri_VLEN<0b010, 0b001, simm12, 0b01, "v.andi">;
def SIMT_ORI_X2  : SIMT_ALUarithri_VLEN<0b011, 0b001, simm12, 0b01, "v.ori">;
def SIMT_XORI_X2 : SIMT_ALUarithri_VLEN<0b100, 0b001, simm12, 0b01, "v.xori">;

defm SIMT_CMP_EQ_X2  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b000, 0b100, 0b01, "v.cmp.eq">;
defm SIMT_CMP_NE_X2  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b001, 0b100, 0b01, "v.cmp.ne">;
defm SIMT_CMP_AND_X2 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b010, 0b100, 0b01, "v.cmp.and">;
defm SIMT_CMP_OR_X2  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b011, 0b100, 0b01, "v.cmp.or">;
defm SIMT_CMP_LT_X2  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b100, 0b100, 0b01, "v.cmp.lt">;
defm SIMT_CMP_GE_X2  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b101, 0b100, 0b01, "v.cmp.ge">;
defm SIMT_CMP_LTU_X2 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b110, 0b100, 0b01, "v.cmp.ltu">;
defm SIMT_CMP_GEU_X2 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b111, 0b100, 0b01, "v.cmp.geu">;

defm SIMT_CMP_EQI_X2  : SIMT_ALUarithri_CMP_VLEN_VP<0b000, 0b101, simm12, 0b01, "v.cmp.eqi">;
defm SIMT_CMP_NEI_X2  : SIMT_ALUarithri_CMP_VLEN_VP<0b001, 0b101, simm12, 0b01, "v.cmp.nei">;
defm SIMT_CMP_ANDI_X2 : SIMT_ALUarithri_CMP_VLEN_VP<0b010, 0b101, simm12, 0b01, "v.cmp.andi">;
defm SIMT_CMP_ORI_X2  : SIMT_ALUarithri_CMP_VLEN_VP<0b011, 0b101, simm12, 0b01, "v.cmp.ori">;
defm SIMT_CMP_LTI_X2  : SIMT_ALUarithri_CMP_VLEN_VP<0b100, 0b101, simm12, 0b01, "v.cmp.lti">;
defm SIMT_CMP_GEI_X2  : SIMT_ALUarithri_CMP_VLEN_VP<0b101, 0b101, simm12, 0b01, "v.cmp.gei">;
defm SIMT_CMP_LTUI_X2 : SIMT_ALUarithri_CMP_VLEN_VP<0b110, 0b101, uimm12, 0b01, "v.cmp.ltui">;
defm SIMT_CMP_GEUI_X2 : SIMT_ALUarithri_CMP_VLEN_VP<0b111, 0b101, uimm12, 0b01, "v.cmp.geui">;

def SIMT_MUL_X2  : SIMT_MUL_Base_VLEN<0b000, 0b01, "v.mul">;
def SIMT_MADD_X2 : SIMT_MADD_Base_VLEN<0b110, 0b01, "v.madd">;
def SIMT_DIV_X2  : SIMT_DIVrr_Base_VLEN<0b000, 0b01, "v.div">;
def SIMT_REM_X2  : SIMT_DIVrr_Base_VLEN<0b100, 0b01, "v.rem">;

def SIMT_MAX_X2 : SIMT_MAXMIN_Base_VLEN<0b000, 0b01, "v.max">;
def SIMT_MIN_X2 : SIMT_MAXMIN_Base_VLEN<0b001, 0b01, "v.min">;

defm SIMT_FADD_X2 : SIMT_FARITHrr_VLEN<0b000, 0b01, "x2", "v.fadd">;
defm SIMT_FSUB_X2 : SIMT_FARITHrr_VLEN<0b001, 0b01, "x2", "v.fsub">;
defm SIMT_FMUL_X2 : SIMT_FARITHrr_VLEN<0b010, 0b01, "x2", "v.fmul">;
defm SIMT_FDIV_X2 : SIMT_FARITHrr_VLEN<0b011, 0b01, "x2", "v.fdiv">;

defm SIMT_FMADD_X2  : SIMT_FARITHrrr_VLEN<0b100, 0b01, "x2", "v.fmadd">;
defm SIMT_FMSUB_X2  : SIMT_FARITHrrr_VLEN<0b101, 0b01, "x2", "v.fmsub">;
defm SIMT_FNMADD_X2 : SIMT_FARITHrrr_VLEN<0b110, 0b01, "x2", "v.fnmadd">;
defm SIMT_FNMSUB_X2 : SIMT_FARITHrrr_VLEN<0b111, 0b01, "x2", "v.fnmsub">;

def SIMT_FEQ_X2  : SIMT_FCMPrr_Base_VLEN<0b000, 0b01, "v.feq">;
def SIMT_FNE_X2  : SIMT_FCMPrr_Base_VLEN<0b001, 0b01, "v.fne">;
def SIMT_FLT_X2  : SIMT_FCMPrr_Base_VLEN<0b010, 0b01, "v.flt">;
def SIMT_FGE_X2  : SIMT_FCMPrr_Base_VLEN<0b011, 0b01, "v.fge">;
def SIMT_FEQS_X2 : SIMT_FCMPrr_Base_VLEN<0b100, 0b01, "v.feqs">;
def SIMT_FNES_X2 : SIMT_FCMPrr_Base_VLEN<0b101, 0b01, "v.fnes">;
def SIMT_FLTS_X2 : SIMT_FCMPrr_Base_VLEN<0b110, 0b01, "v.flts">;
def SIMT_FGES_X2 : SIMT_FCMPrr_Base_VLEN<0b111, 0b01, "v.fges">;

def SIMT_FMAX_X2 : SIMT_FMAXMIN_Base_VLEN<0b010, 0b01, "v.fmax">;
def SIMT_FMIN_X2 : SIMT_FMAXMIN_Base_VLEN<0b011, 0b01, "v.fmin">;

defm SIMT_FABS_X2   : SIMT_FLOATr_RS_VLEN<0b000, 0b111, 0b101, 0b01, "x2", "v.fabs">;
defm SIMT_FSQRT_X2  : SIMT_FLOATr_RS_VLEN<0b001, 0b111, 0b101, 0b01, "x2", "v.fsqrt">;
defm SIMT_FRECIP_X2 : SIMT_FLOATr_RS_VLEN<0b010, 0b111, 0b101, 0b01, "x2", "v.frecip">;
defm SIMT_FEXP_X2   : SIMT_FLOATr_RS_VLEN<0b011, 0b111, 0b101, 0b01, "x2", "v.fexp">;

def SIMT_FCLASS_X2 : SIMT_FLOATr_Base_VLEN<0b111, 0b111, 0b101, 0b01, "v.fclass">;

defm SIMT_RDADD_X2 : SIMT_Reduce_VLEN_MC<0b000, 0b000, 0b01, "v.rdadd">;
defm SIMT_RDAND_X2 : SIMT_Reduce_VLEN_MC<0b001, 0b000, 0b01, "v.rdand">;
defm SIMT_RDOR_X2  : SIMT_Reduce_VLEN_MC<0b010, 0b000, 0b01, "v.rdor">;
defm SIMT_RDXOR_X2 : SIMT_Reduce_VLEN_MC<0b011, 0b000, 0b01, "v.rdxor">;
defm SIMT_RDMAX_X2 : SIMT_Reduce_VLEN_MC<0b000, 0b001, 0b01, "v.rdmax">;
defm SIMT_RDMIN_X2 : SIMT_Reduce_VLEN_MC<0b001, 0b001, 0b01, "v.rdmin">;

defm SIMT_RDFADD_X2 : SIMT_Reduce_F_VLEN_MC<0b100, 0b000, 0b01, "v.rdfadd">;
defm SIMT_RDFMAX_X2 : SIMT_Reduce_F_VLEN_MC<0b010, 0b001, 0b01, "v.rdfmax">;
defm SIMT_RDFMIN_X2 : SIMT_Reduce_F_VLEN_MC<0b011, 0b001, 0b01, "v.rdfmin">;

//===----------------------------------------------------------------------===//
// x4 instruction definitions
//===----------------------------------------------------------------------===//

defm SIMT_ADD_X4 : SIMT_AUrr_T_N_VLEN_Alias<0b000, 0b10, "x4", "v.add">;
defm SIMT_SUB_X4 : SIMT_AUrr_T_N_VLEN_Alias<0b001, 0b10, "x4", "v.sub">;

def SIMT_AND_X4 : SIMT_LUrr_T_N_VLEN<0b010, 0b10, "v.and">;
def SIMT_OR_X4  : SIMT_LUrr_T_N_VLEN<0b011, 0b10, "v.or">;
def SIMT_XOR_X4 : SIMT_LUrr_T_N_VLEN<0b100, 0b10, "v.xor">;

def SIMT_SRL_X4 : SIMT_ALUrr_ReserveShmat_VLEN<0b101, 0b000, 0b10, "v.srl">;
def SIMT_SRA_X4 : SIMT_ALUrr_ReserveShmat_VLEN<0b110, 0b000, 0b10, "v.sra">;
def SIMT_SLL_X4 : SIMT_ALUrr_ReserveShmat_VLEN<0b111, 0b000, 0b10, "v.sll">;

def SIMT_ADDI_X4 : SIMT_ALUarithri_VLEN<0b000, 0b001, uimm12, 0b10, "v.addi">;
def SIMT_SUBI_X4 : SIMT_ALUarithri_VLEN<0b001, 0b001, uimm12, 0b10, "v.subi">;
def SIMT_ANDI_X4 : SIMT_ALUarithri_VLEN<0b010, 0b001, simm12, 0b10, "v.andi">;
def SIMT_ORI_X4  : SIMT_ALUarithri_VLEN<0b011, 0b001, simm12, 0b10, "v.ori">;
def SIMT_XORI_X4 : SIMT_ALUarithri_VLEN<0b100, 0b001, simm12, 0b10, "v.xori">;

defm SIMT_CMP_EQ_X4  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b000, 0b100, 0b10, "v.cmp.eq">;
defm SIMT_CMP_NE_X4  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b001, 0b100, 0b10, "v.cmp.ne">;
defm SIMT_CMP_AND_X4 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b010, 0b100, 0b10, "v.cmp.and">;
defm SIMT_CMP_OR_X4  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b011, 0b100, 0b10, "v.cmp.or">;
defm SIMT_CMP_LT_X4  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b100, 0b100, 0b10, "v.cmp.lt">;
defm SIMT_CMP_GE_X4  : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b101, 0b100, 0b10, "v.cmp.ge">;
defm SIMT_CMP_LTU_X4 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b110, 0b100, 0b10, "v.cmp.ltu">;
defm SIMT_CMP_GEU_X4 : SIMT_ALUrr_CMP_ReserveShmat_VLEN<0b111, 0b100, 0b10, "v.cmp.geu">;

defm SIMT_CMP_EQI_X4  : SIMT_ALUarithri_CMP_VLEN_VP<0b000, 0b101, simm12, 0b10, "v.cmp.eqi">;
defm SIMT_CMP_NEI_X4  : SIMT_ALUarithri_CMP_VLEN_VP<0b001, 0b101, simm12, 0b10, "v.cmp.nei">;
defm SIMT_CMP_ANDI_X4 : SIMT_ALUarithri_CMP_VLEN_VP<0b010, 0b101, simm12, 0b10, "v.cmp.andi">;
defm SIMT_CMP_ORI_X4  : SIMT_ALUarithri_CMP_VLEN_VP<0b011, 0b101, simm12, 0b10, "v.cmp.ori">;
defm SIMT_CMP_LTI_X4  : SIMT_ALUarithri_CMP_VLEN_VP<0b100, 0b101, simm12, 0b10, "v.cmp.lti">;
defm SIMT_CMP_GEI_X4  : SIMT_ALUarithri_CMP_VLEN_VP<0b101, 0b101, simm12, 0b10, "v.cmp.gei">;
defm SIMT_CMP_LTUI_X4 : SIMT_ALUarithri_CMP_VLEN_VP<0b110, 0b101, uimm12, 0b10, "v.cmp.ltui">;
defm SIMT_CMP_GEUI_X4 : SIMT_ALUarithri_CMP_VLEN_VP<0b111, 0b101, uimm12, 0b10, "v.cmp.geui">;

def SIMT_MUL_X4  : SIMT_MUL_Base_VLEN<0b000, 0b10, "v.mul">;
def SIMT_MADD_X4 : SIMT_MADD_Base_VLEN<0b110, 0b10, "v.madd">;
def SIMT_DIV_X4  : SIMT_DIVrr_Base_VLEN<0b000, 0b10, "v.div">;
def SIMT_REM_X4  : SIMT_DIVrr_Base_VLEN<0b100, 0b10, "v.rem">;

def SIMT_MAX_X4 : SIMT_MAXMIN_Base_VLEN<0b000, 0b10, "v.max">;
def SIMT_MIN_X4 : SIMT_MAXMIN_Base_VLEN<0b001, 0b10, "v.min">;

defm SIMT_FADD_X4 : SIMT_FARITHrr_VLEN<0b000, 0b10, "x4", "v.fadd">;
defm SIMT_FSUB_X4 : SIMT_FARITHrr_VLEN<0b001, 0b10, "x4", "v.fsub">;
defm SIMT_FMUL_X4 : SIMT_FARITHrr_VLEN<0b010, 0b10, "x4", "v.fmul">;
defm SIMT_FDIV_X4 : SIMT_FARITHrr_VLEN<0b011, 0b10, "x4", "v.fdiv">;

defm SIMT_FMADD_X4  : SIMT_FARITHrrr_VLEN<0b100, 0b10, "x4", "v.fmadd">;
defm SIMT_FMSUB_X4  : SIMT_FARITHrrr_VLEN<0b101, 0b10, "x4", "v.fmsub">;
defm SIMT_FNMADD_X4 : SIMT_FARITHrrr_VLEN<0b110, 0b10, "x4", "v.fnmadd">;
defm SIMT_FNMSUB_X4 : SIMT_FARITHrrr_VLEN<0b111, 0b10, "x4", "v.fnmsub">;

def SIMT_FEQ_X4  : SIMT_FCMPrr_Base_VLEN<0b000, 0b10, "v.feq">;
def SIMT_FNE_X4  : SIMT_FCMPrr_Base_VLEN<0b001, 0b10, "v.fne">;
def SIMT_FLT_X4  : SIMT_FCMPrr_Base_VLEN<0b010, 0b10, "v.flt">;
def SIMT_FGE_X4  : SIMT_FCMPrr_Base_VLEN<0b011, 0b10, "v.fge">;
def SIMT_FEQS_X4 : SIMT_FCMPrr_Base_VLEN<0b100, 0b10, "v.feqs">;
def SIMT_FNES_X4 : SIMT_FCMPrr_Base_VLEN<0b101, 0b10, "v.fnes">;
def SIMT_FLTS_X4 : SIMT_FCMPrr_Base_VLEN<0b110, 0b10, "v.flts">;
def SIMT_FGES_X4 : SIMT_FCMPrr_Base_VLEN<0b111, 0b10, "v.fges">;

def SIMT_FMAX_X4 : SIMT_FMAXMIN_Base_VLEN<0b010, 0b10, "v.fmax">;
def SIMT_FMIN_X4 : SIMT_FMAXMIN_Base_VLEN<0b011, 0b10, "v.fmin">;

defm SIMT_FABS_X4   : SIMT_FLOATr_RS_VLEN<0b000, 0b111, 0b101, 0b10, "x4", "v.fabs">;
defm SIMT_FSQRT_X4  : SIMT_FLOATr_RS_VLEN<0b001, 0b111, 0b101, 0b10, "x4", "v.fsqrt">;
defm SIMT_FRECIP_X4 : SIMT_FLOATr_RS_VLEN<0b010, 0b111, 0b101, 0b10, "x4", "v.frecip">;
defm SIMT_FEXP_X4   : SIMT_FLOATr_RS_VLEN<0b011, 0b111, 0b101, 0b10, "x4", "v.fexp">;

def SIMT_FCLASS_X4 : SIMT_FLOATr_Base_VLEN<0b111, 0b111, 0b101, 0b10, "v.fclass">;

defm SIMT_RDADD_X4 : SIMT_Reduce_VLEN_MC<0b000, 0b000, 0b10, "v.rdadd">;
defm SIMT_RDAND_X4 : SIMT_Reduce_VLEN_MC<0b001, 0b000, 0b10, "v.rdand">;
defm SIMT_RDOR_X4  : SIMT_Reduce_VLEN_MC<0b010, 0b000, 0b10, "v.rdor">;
defm SIMT_RDXOR_X4 : SIMT_Reduce_VLEN_MC<0b011, 0b000, 0b10, "v.rdxor">;
defm SIMT_RDMAX_X4 : SIMT_Reduce_VLEN_MC<0b000, 0b001, 0b10, "v.rdmax">;
defm SIMT_RDMIN_X4 : SIMT_Reduce_VLEN_MC<0b001, 0b001, 0b10, "v.rdmin">;

defm SIMT_RDFADD_X4 : SIMT_Reduce_F_VLEN_MC<0b100, 0b000, 0b10, "v.rdfadd">;
defm SIMT_RDFMAX_X4 : SIMT_Reduce_F_VLEN_MC<0b010, 0b001, 0b10, "v.rdfmax">;
defm SIMT_RDFMIN_X4 : SIMT_Reduce_F_VLEN_MC<0b011, 0b001, 0b10, "v.rdfmin">;
