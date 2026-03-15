//===- LinxISAInstrInfo.td - LinxISA instruction definitions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "LinxISAInstrFormats.td"

//===----------------------------------------------------------------------===//
// DAG nodes for calls
//===----------------------------------------------------------------------===//

def SDT_LinxISACallSeqStart : SDCallSeqStart<[SDTCisVT<0, i64>,
                                             SDTCisVT<1, i64>]>;
def SDT_LinxISACallSeqEnd : SDCallSeqEnd<[SDTCisVT<0, i64>,
                                         SDTCisVT<1, i64>]>;
def SDT_LinxISACall : SDTypeProfile<0, -1, [SDTCisVT<0, iPTR>]>;

def CallSeqStart : SDNode<"ISD::CALLSEQ_START", SDT_LinxISACallSeqStart,
                          [SDNPHasChain, SDNPOutGlue]>;
def CallSeqEnd : SDNode<"ISD::CALLSEQ_END", SDT_LinxISACallSeqEnd,
                        [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

def Call : SDNode<"LinxISD::CALL", SDT_LinxISACall,
                  [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue, SDNPVariadic]>;
def RetGlue : SDNode<"LinxISD::RET_GLUE", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

def BrTargetAsmOperand : AsmOperandClass {
  let Name = "BrTarget";
}

def CallTargetAsmOperand : AsmOperandClass {
  let Name = "CallTarget";
}

def simm12 : Operand<i64>, PatLeaf<(i64 imm), [{ return isInt<12>(N->getSExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm12 : Operand<i64>, PatLeaf<(i64 imm), [{ return isUInt<12>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def simm17 : Operand<i64>, PatLeaf<(i64 imm), [{ return isInt<17>(N->getSExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

// SETC.*I encodes a shifted immediate: simm/uimm = imm12 << shamt (shamt is a
// 5-bit field). Keep the MachineInstr operand as the *final* shifted value and
// let MC lowering split it into {shamt, imm12}.
def simm12sh : Operand<i64>, PatLeaf<(i64 imm), [{
  int64_t V = N->getSExtValue();
  if (isInt<12>(V))
    return true;
  for (unsigned Sh = 1; Sh < 32; ++Sh) {
    int64_t Pow = (1LL << Sh);
    if (V % Pow != 0)
      continue;
    int64_t Base = V / Pow;
    if (isInt<12>(Base))
      return true;
  }
  return false;
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm12sh : Operand<i64>, PatLeaf<(i64 imm), [{
  uint64_t V = N->getZExtValue();
  if (isUInt<12>(V))
    return true;
  for (unsigned Sh = 1; Sh < 32; ++Sh) {
    uint64_t Pow = (1ULL << Sh);
    if ((V % Pow) != 0)
      continue;
    uint64_t Base = V / Pow;
    if (isUInt<12>(Base))
      return true;
  }
  return false;
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def imm20 : Operand<i64>, PatLeaf<(i64 imm), [{ return isInt<20>(N->getSExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def imm32 : Operand<i64>, PatLeaf<(i64 imm), [{ return isInt<32>(N->getSExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

// 24-bit immediates for 48-bit HL.* operations. These are used to reduce
// dynamic instruction count by avoiding constant materialization sequences.
// Prefer the 32-bit immediate forms when possible.
def uimm24 : Operand<i64>, PatLeaf<(i64 imm), [{
  uint64_t V = N->getZExtValue();
  return isUInt<24>(V) && !isUInt<12>(V);
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def simm24 : Operand<i64>, PatLeaf<(i64 imm), [{
  int64_t V = N->getSExtValue();
  return isInt<24>(V) && !isInt<12>(V);
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

// Shifted immediate for HL.SETC.*I: simm/uimm = imm24 << shamt (shamt is a
// 5-bit field). Keep the MachineInstr operand as the *final* shifted value and
// let MC lowering split it into {shamt, imm24}.
def simm24sh : Operand<i64>, PatLeaf<(i64 imm), [{
  int64_t V = N->getSExtValue();
  if (isInt<24>(V))
    return true;
  for (unsigned Sh = 1; Sh < 32; ++Sh) {
    int64_t Pow = (1LL << Sh);
    if (V % Pow != 0)
      continue;
    int64_t Base = V / Pow;
    if (isInt<24>(Base))
      return true;
  }
  return false;
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm5 : Operand<i64>, PatLeaf<(i64 imm), [{ return isUInt<5>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm6 : Operand<i64>, PatLeaf<(i64 imm), [{ return isUInt<6>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm10 : Operand<i64>, PatLeaf<(i64 imm), [{ return isUInt<10>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm2 : Operand<i64>, PatLeaf<(i64 imm), [{ return isUInt<2>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def simm12_w : Operand<i32>, PatLeaf<(i32 imm), [{ return isInt<12>(N->getSExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm12_w : Operand<i32>, PatLeaf<(i32 imm), [{ return isUInt<12>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm24_w : Operand<i32>, PatLeaf<(i32 imm), [{
  uint64_t V = N->getZExtValue();
  return isUInt<24>(V) && !isUInt<12>(V);
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def simm24_w : Operand<i32>, PatLeaf<(i32 imm), [{
  int64_t V = N->getSExtValue();
  return isInt<24>(V) && !isInt<12>(V);
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def uimm5_w : Operand<i32>, PatLeaf<(i32 imm), [{ return isUInt<5>(N->getZExtValue()); }]> {
  let ParserMatchClass = ImmAsmOperand;
}

// Branch target operand.
def BrTarget : Operand<OtherVT> {
  let ParserMatchClass = BrTargetAsmOperand;
}

// Call target operand.
def CallTarget : Operand<iPTR> {
  let ParserMatchClass = CallTargetAsmOperand;
}

//===----------------------------------------------------------------------===//
// Pseudo instructions (expanded in the AsmPrinter)
//===----------------------------------------------------------------------===//

let isCall = 1, hasSideEffects = 1, Size = 6, Uses = [R1], Defs = [R10] in
def PSEUDO_CALL : Pseudo<(outs), (ins CallTarget:$dst), "# PSEUDO_CALL", []>;

// Tail call / tail transfer (musttail): terminates the function and must not
// clobber `ra`. The frame is torn down via an explicit FEXIT block inserted by
// PEI, and Blockify turns this into a DIRECT/IND block transfer.
let isCall = 1, isReturn = 1, isTerminator = 1, hasSideEffects = 1, Size = 6,
    Uses = [R1] in
def PSEUDO_TAILCALL
    : Pseudo<(outs), (ins CallTarget:$dst), "# PSEUDO_TAILCALL", []>;

// Keep RA live at return. Return value registers are attached explicitly by
// LowerReturn as operands on each PSEUDO_RET instance, so they must not be
// hard-wired here (void returns would otherwise require undefined a0/a1).
let isReturn = 1, isTerminator = 1, Size = 4, Uses = [R10] in
def PSEUDO_RET : Pseudo<(outs), (ins), "# PSEUDO_RET", [(RetGlue)]>;

def : Pat<(Call tglobaladdr:$dst), (PSEUDO_CALL tglobaladdr:$dst)>;
def : Pat<(Call texternalsym:$dst), (PSEUDO_CALL texternalsym:$dst)>;

// Call frame pseudos.
let Defs = [R1], Uses = [R1] in {
  def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i64imm:$amt1, i64imm:$amt2),
                                "#ADJCALLSTACKDOWN $amt1 $amt2",
                                [(CallSeqStart timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP : Pseudo<(outs), (ins i64imm:$amt1, i64imm:$amt2),
                              "#ADJCALLSTACKUP $amt1 $amt2",
                              [(CallSeqEnd timm:$amt1, timm:$amt2)]>;
}

//===----------------------------------------------------------------------===//
// Function Entry/Exit Macro Instructions (LinxISA spec)
//===----------------------------------------------------------------------===//
// These are hardware macro instructions that expand to prologue/epilogue code.
// Syntax: FENTRY/FEXIT/FRET.STK [ra ~ sN], sp!, stacksize
//
// The register range [Begin ~ End] specifies which registers to save/restore.
// Stack size is encoded in the uimm field (8-byte aligned).

// Operands for FENTRY/FEXIT family
def fentry_stacksize : Operand<i64>, PatLeaf<(i64 imm), [{ 
  int64_t val = N->getSExtValue();
  return val >= 0 && val <= 32760 && (val & 7) == 0; // 15-bit, 8-byte aligned
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def fentry_regbegin : Operand<i64>, PatLeaf<(i64 imm), [{
  return isUInt<5>(N->getZExtValue());
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

def fentry_regend : Operand<i64>, PatLeaf<(i64 imm), [{
  return isUInt<5>(N->getZExtValue());
}]> {
  let ParserMatchClass = ImmAsmOperand;
}

// FENTRY (32-bit): Function entry - save registers [Begin ~ End], adjust SP
// Syntax: FENTRY [ra ~ s2], sp!, 256
let hasSideEffects = 1, mayStore = 1, Size = 4, Defs = [R1] in
def FENTRY : LinxISAInst<(outs), (ins fentry_regbegin:$begin, fentry_regend:$end, fentry_stacksize:$stacksize), "", []>;

// FEXIT (32-bit): Function exit - restore registers, adjust SP (for indirect return)
// Used with subsequent IND block for computed return address
let hasSideEffects = 1, mayLoad = 1, Size = 4, Uses = [R1], Defs = [R1] in
def FEXIT : LinxISAInst<(outs), (ins fentry_regbegin:$begin, fentry_regend:$end, fentry_stacksize:$stacksize), "", []>;

// FRET.RA (32-bit): Function return via RA - restore registers, adjust SP, return
let hasSideEffects = 1, mayLoad = 1, isReturn = 1, isTerminator = 1, isBarrier = 1,
    Size = 4, Uses = [R1, R10], Defs = [R1] in
def FRET_RA : LinxISAInst<(outs), (ins fentry_regbegin:$begin, fentry_regend:$end, fentry_stacksize:$stacksize), "", []>;

// FRET.STK (32-bit): Function return via stack - restore registers, adjust SP, return
// This is the most common return instruction for non-leaf functions
let hasSideEffects = 1, mayLoad = 1, isReturn = 1, isTerminator = 1, isBarrier = 1,
    Size = 4, Uses = [R1], Defs = [R1, R10] in
def FRET_STK : LinxISAInst<(outs), (ins fentry_regbegin:$begin, fentry_regend:$end, fentry_stacksize:$stacksize), "", []>;

// Bulk memory template blocks (LinxISA spec).
//
// These are standalone template blocks (block start markers) that expand to a
// restartable sequence of micro-ops in hardware.
let hasSideEffects = 1, mayLoad = 1, mayStore = 1, Size = 4 in
def MCOPY : LinxISAInst<(outs), (ins GPR_Arch:$dst, GPR_Arch:$src, GPR_Arch:$size), "", []>;

let hasSideEffects = 1, mayStore = 1, Size = 4 in
def MSET  : LinxISAInst<(outs), (ins GPR_Arch:$dst, GPR_Arch:$val, GPR_Arch:$size), "", []>;

//===----------------------------------------------------------------------===//
// Integer arithmetic/logical ops (selected by patterns)
//===----------------------------------------------------------------------===//

def ADDrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (add (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def SUBrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (sub (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def ANDrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (and (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def ORrr   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (or  (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def XORrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (xor (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;

// Shifted-operand arithmetic (introduced by a late peephole pass).
def ADDrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def SUBrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def ANDrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def ORrr_SH  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def XORrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;

def ADDIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12:$imm), "", [(set (i64 GPR:$rd), (add (i64 GPR:$rs1), uimm12:$imm))]>;
def SUBIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12:$imm), "", [(set (i64 GPR:$rd), (sub (i64 GPR:$rs1), uimm12:$imm))]>;
def ANDIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", [(set (i64 GPR:$rd), (and (i64 GPR:$rs1), simm12:$imm))]>;
def ORIri  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", [(set (i64 GPR:$rd), (or  (i64 GPR:$rs1), simm12:$imm))]>;
def XORIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", [(set (i64 GPR:$rd), (xor (i64 GPR:$rs1), simm12:$imm))]>;

// 48-bit immediate operations (HL.*). These trade code size for fewer dynamic
// guest instructions when immediates do not fit in the 12-bit forms.
let Size = 6 in {
  def HLADDIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm24:$imm), "",
                             [(set (i64 GPR:$rd), (add (i64 GPR:$rs1), uimm24:$imm))]>;
  def HLSUBIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm24:$imm), "",
                             [(set (i64 GPR:$rd), (sub (i64 GPR:$rs1), uimm24:$imm))]>;
  def HLANDIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24:$imm), "",
                             [(set (i64 GPR:$rd), (and (i64 GPR:$rs1), simm24:$imm))]>;
  def HLORIri  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24:$imm), "",
                             [(set (i64 GPR:$rd), (or  (i64 GPR:$rs1), simm24:$imm))]>;
  def HLXORIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24:$imm), "",
                             [(set (i64 GPR:$rd), (xor (i64 GPR:$rs1), simm24:$imm))]>;
}

def SLLrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (shl (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def SRLrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (srl (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def SRArr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (sra (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;

def SLLIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$shamt), "", [(set (i64 GPR:$rd), (shl (i64 GPR:$rs1), uimm5:$shamt))]>;
def SRLIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$shamt), "", [(set (i64 GPR:$rd), (srl (i64 GPR:$rs1), uimm5:$shamt))]>;
def SRAIri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$shamt), "", [(set (i64 GPR:$rd), (sra (i64 GPR:$rs1), uimm5:$shamt))]>;

// 32-bit ops (selected by patterns)
def ADDWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def SUBWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def ANDWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def ORWrr   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def XORWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;

def ADDWrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def SUBWrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def ANDWrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def ORWrr_SH  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def XORWrr_SH : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$shamt), "", []>;
def SLLWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def SRLWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def SRAWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;

def ADDIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12_w:$imm), "", []>;
def SUBIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12_w:$imm), "", []>;
def ANDIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12_w:$imm), "", []>;
def ORIWri  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12_w:$imm), "", []>;
def XORIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12_w:$imm), "", []>;

// 48-bit word immediate operations (HL.*W). These reduce dynamic instruction
// count for large immediates that don't fit the 12-bit forms.
let Size = 6 in {
  def HLADDIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm24_w:$imm), "", []>;
  def HLSUBIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm24_w:$imm), "", []>;
  def HLANDIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24_w:$imm), "", []>;
  def HLORIWri  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24_w:$imm), "", []>;
  def HLXORIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24_w:$imm), "", []>;
}

def SLLIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5_w:$shamt), "", []>;
def SRLIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5_w:$shamt), "", []>;
def SRAIWri : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5_w:$shamt), "", []>;

// Prefer immediate forms when the RHS is a constant.
def : Pat<(add i32:$a, uimm12_w:$b), (ADDIWri $a, $b)>;
def : Pat<(sub i32:$a, uimm12_w:$b), (SUBIWri $a, $b)>;
def : Pat<(and i32:$a, simm12_w:$b), (ANDIWri $a, $b)>;
def : Pat<(or  i32:$a, simm12_w:$b), (ORIWri  $a, $b)>;
def : Pat<(xor i32:$a, simm12_w:$b), (XORIWri $a, $b)>;

// 48-bit HL.*W variants when immediates don't fit 12 bits.
def : Pat<(add i32:$a, uimm24_w:$b), (HLADDIWri $a, $b)>;
def : Pat<(sub i32:$a, uimm24_w:$b), (HLSUBIWri $a, $b)>;
def : Pat<(and i32:$a, simm24_w:$b), (HLANDIWri $a, $b)>;
def : Pat<(or  i32:$a, simm24_w:$b), (HLORIWri  $a, $b)>;
def : Pat<(xor i32:$a, simm24_w:$b), (HLXORIWri $a, $b)>;

def : Pat<(shl i32:$a, uimm5_w:$b), (SLLIWri $a, $b)>;
def : Pat<(srl i32:$a, uimm5_w:$b), (SRLIWri $a, $b)>;
def : Pat<(sra i32:$a, uimm5_w:$b), (SRAIWri $a, $b)>;

// Reg-reg forms.
def : Pat<(add i32:$a, i32:$b), (ADDWrr $a, $b)>;
def : Pat<(sub i32:$a, i32:$b), (SUBWrr $a, $b)>;
def : Pat<(and i32:$a, i32:$b), (ANDWrr $a, $b)>;
def : Pat<(or  i32:$a, i32:$b), (ORWrr  $a, $b)>;
def : Pat<(xor i32:$a, i32:$b), (XORWrr $a, $b)>;
def : Pat<(shl i32:$a, i32:$b), (SLLWrr $a, $b)>;
def : Pat<(srl i32:$a, i32:$b), (SRLWrr $a, $b)>;
def : Pat<(sra i32:$a, i32:$b), (SRAWrr $a, $b)>;

//===----------------------------------------------------------------------===//
// Memory/Control-flow ops (selected manually in C++)
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Block markers (BlockISA)
//
// These are inserted very late (pre-emit) to model the ISA block boundaries.
// The current bring-up treats them as markers around existing control-flow.
//===----------------------------------------------------------------------===//

let hasSideEffects = 1, isBarrier = 1, Size = 2 in
def CBSTART_STD : LinxISAInst<(outs), (ins i64imm:$brtype), "", []>;

// 32-bit block start forms used by the BlockISA control-flow model.
//
// These are inserted very late (pre-emit) and are not selected by the DAG.
let hasSideEffects = 1, isBarrier = 1, Size = 4 in {
  def BSTART_STD_FALL  : LinxISAInst<(outs), (ins), "", []>;
  def BSTART_STD_DIRECT : LinxISAInst<(outs), (ins BrTarget:$dst), "", []>;
  def BSTART_STD_COND  : LinxISAInst<(outs), (ins BrTarget:$dst), "", []>;
  def BSTART_STD_CALL  : LinxISAInst<(outs), (ins CallTarget:$dst), "", []>;
  def BSTART_STD_IND   : LinxISAInst<(outs), (ins), "", []>;
  def BSTART_STD_ICALL : LinxISAInst<(outs), (ins), "", []>;
  def BSTART_STD_RET   : LinxISAInst<(outs), (ins), "", []>;
}

let hasSideEffects = 1, isBarrier = 1, isTerminator = 1, Size = 4 in
def BSTOP : LinxISAInst<(outs), (ins), "", []>;

//===----------------------------------------------------------------------===//
// Tile blocks (TAU) - bring-up (selected manually from intrinsics)
//
// These are expanded very late (pre-emit) into block-structured sequences:
//   BSTART.TMA / BSTART.CUBE + B.IOT/B.IOTI/B.DIM descriptors.
//===----------------------------------------------------------------------===//

let hasSideEffects = 1, isBarrier = 1, mayLoad = 1, Size = 4 in
def PSEUDO_TMA_TLOAD : Pseudo<(outs TILE_TU:$dst), (ins GPR_Arch:$base, i64imm:$size),
                              "# PSEUDO_TMA_TLOAD", []>;

// Spill/reload helper: allow TLOAD into any architectural tile register bank.
let hasSideEffects = 1, isBarrier = 1, mayLoad = 1, Size = 4 in
def PSEUDO_TMA_TLOAD_ANY : Pseudo<(outs TILE:$dst), (ins GPR_Arch:$base, i64imm:$size),
                                  "# PSEUDO_TMA_TLOAD_ANY", []>;

let hasSideEffects = 1, isBarrier = 1, mayLoad = 1, Size = 4 in
def PSEUDO_TMA_TLOAD_DESC : Pseudo<(outs TILE_TU:$dst),
                                   (ins GPR_Arch:$base, i64imm:$dtype,
                                        i64imm:$layout, i64imm:$lb0,
                                        i64imm:$lb1, i64imm:$size,
                                        GPR_Arch:$stride_bytes),
                                   "# PSEUDO_TMA_TLOAD_DESC", []>;

let hasSideEffects = 1, isBarrier = 1, mayStore = 1, Size = 4 in
def PSEUDO_TMA_TSTORE : Pseudo<(outs), (ins GPR_Arch:$base, TILE:$src, i64imm:$size),
                               "# PSEUDO_TMA_TSTORE", []>;

let hasSideEffects = 1, isBarrier = 1, mayStore = 1, Size = 4 in
def PSEUDO_TMA_TSTORE_DESC : Pseudo<(outs),
                                    (ins GPR_Arch:$base, TILE:$src,
                                         i64imm:$dtype, i64imm:$layout,
                                         i64imm:$lb0, i64imm:$lb1,
                                         i64imm:$size, GPR_Arch:$stride_bytes),
                                    "# PSEUDO_TMA_TSTORE_DESC", []>;

// Compiler-inserted tile SSA edge balancing pseudo.
//
// Operand contract:
//   dst/src       : tile destination/source values
//   size          : SizeCode propagated from source tile metadata
//   dtype         : DataType (0 when unknown; strict mismatch checking happens
//                   in LinxISATileSSABalance before this pseudo is emitted)
//   layout        : layout payload (valid when has_layout=1)
//   has_layout    : 0/1 bit
//   mode          : TMOV mode (PR2 defaults to V2V=0)
//   src_reuse     : 0/1 bit derived from COPY kill semantics
let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_TMA_TMOV : Pseudo<(outs TILE:$dst),
                             (ins TILE:$src, i64imm:$size, i64imm:$dtype,
                                  i64imm:$layout, i64imm:$has_layout,
                                  i64imm:$mode, i64imm:$src_reuse),
                             "# PSEUDO_TMA_TMOV", []>;

let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_CUBE_MAMULB : Pseudo<(outs TILE_MN:$dst),
                                (ins TILE:$srcA, TILE:$srcB,
                                     i64imm:$m, i64imm:$n, i64imm:$k),
                                "# PSEUDO_CUBE_MAMULB", []>;

let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_CUBE_MAMULB_ACC : Pseudo<(outs TILE_MN:$dst),
                                    (ins TILE:$acc, TILE:$srcA, TILE:$srcB,
                                         i64imm:$m, i64imm:$n, i64imm:$k),
                                    "# PSEUDO_CUBE_MAMULB_ACC", []>;

// PR5 roadmap stub: explicit CUBE ACCCVT stage for frontend intrinsic use.
let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_CUBE_ACCCVT : Pseudo<(outs TILE_MN:$dst),
                                (ins TILE:$acc,
                                     i64imm:$size, i64imm:$dtype,
                                     i64imm:$qarg0, i64imm:$qarg1),
                                "# PSEUDO_CUBE_ACCCVT", []>;

// PR5 TEPL template-op dispatch pseudos.
let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_TEPL_UNARY : Pseudo<(outs TILE:$dst),
                               (ins TILE:$src, i64imm:$tileop,
                                    i64imm:$size, i64imm:$dtype),
                               "# PSEUDO_TEPL_UNARY", []>;

let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_TEPL_BINARY : Pseudo<(outs TILE:$dst),
                                (ins TILE:$srcA, TILE:$srcB, i64imm:$tileop,
                                     i64imm:$size, i64imm:$dtype),
                                "# PSEUDO_TEPL_BINARY", []>;

let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_TEPL_BINARY_SCALAR : Pseudo<(outs TILE:$dst),
                                       (ins TILE:$srcA, GPR_Arch:$srcS,
                                            i64imm:$tileop, i64imm:$size,
                                            i64imm:$dtype, i64imm:$mode),
                                       "# PSEUDO_TEPL_BINARY_SCALAR", []>;

let hasSideEffects = 1, isBarrier = 1, Size = 4 in
def PSEUDO_TEPL_SPLAT : Pseudo<(outs TILE:$dst),
                               (ins GPR_Arch:$srcS, i64imm:$tileop,
                                    i64imm:$size, i64imm:$dtype,
                                    i64imm:$mode),
                               "# PSEUDO_TEPL_SPLAT", []>;

// Vector decoupled blocks (bring-up): lowered late into BSTART.VSEQ/VPAR headers
// with B.TEXT + B.DIM descriptors and a per-function empty body stub.
let hasSideEffects = 1, Size = 4 in
def PSEUDO_VBLOCK_LAUNCH : Pseudo<(outs),
                                  (ins i64imm:$vkind, i64imm:$dim0, i64imm:$dim1,
                                       i64imm:$dim2, i64imm:$attr_bits,
                                       GPR_Arch:$bind0, GPR_Arch:$bind1,
                                       GPR_Arch:$bind2, GPR_Arch:$bind3,
                                       GPR_Arch:$bind4, GPR_Arch:$bind5,
                                       GPR_Arch:$bind6, GPR_Arch:$bind7,
                                       GPR_Arch:$bind8, GPR_Arch:$bind9,
                                       GPR_Arch:$bind10, GPR_Arch:$bind11),
                                  "# PSEUDO_VBLOCK_LAUNCH", []>;

// Dynamic-dimension variant used when Dim1 (group count) is runtime-varying.
let hasSideEffects = 1, Size = 4 in
def PSEUDO_VBLOCK_LAUNCH_DYN1 : Pseudo<(outs),
                                       (ins i64imm:$vkind, i64imm:$dim0,
                                            GPR_Arch:$dim1,
                                            i64imm:$dim2, i64imm:$attr_bits,
                                            GPR_Arch:$bind0, GPR_Arch:$bind1,
                                            GPR_Arch:$bind2, GPR_Arch:$bind3,
                                            GPR_Arch:$bind4, GPR_Arch:$bind5,
                                            GPR_Arch:$bind6, GPR_Arch:$bind7,
                                            GPR_Arch:$bind8, GPR_Arch:$bind9,
                                            GPR_Arch:$bind10, GPR_Arch:$bind11),
                                       "# PSEUDO_VBLOCK_LAUNCH_DYN1", []>;

// SIMT tile binary ops (auto-mode): lowered in Blockify into decoupled
// BSTART.VPAR headers + B.TEXT bodies.
let hasSideEffects = 1, isBarrier = 1, Size = 4 in {
  def PSEUDO_VPAR_TADD : Pseudo<(outs TILE:$dst),
                                (ins TILE:$srcA, TILE:$srcB, i64imm:$size),
                                "# PSEUDO_VPAR_TADD", []>;
  def PSEUDO_VPAR_TSUB : Pseudo<(outs TILE:$dst),
                                (ins TILE:$srcA, TILE:$srcB, i64imm:$size),
                                "# PSEUDO_VPAR_TSUB", []>;
  def PSEUDO_VTILE_ADD : Pseudo<(outs TILE:$dst), (ins TILE:$srcA, TILE:$srcB),
                                "# PSEUDO_VTILE_ADD", []>;
  def PSEUDO_VTILE_SUB : Pseudo<(outs TILE:$dst), (ins TILE:$srcA, TILE:$srcB),
                                "# PSEUDO_VTILE_SUB", []>;
}

// Vector body pseudo ops. These carry lane-register encodings (ri*/vt/vu/vm/vn
// namespace IDs) as immediates and are lowered in MCInstLower to canonical
// strict-v0.3 V.* encodings.
	let hasSideEffects = 1, Size = 4 in {
	  def PSEUDO_V_ADD : Pseudo<(outs),
	                            (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
	                                 i64imm:$srcRType, i64imm:$shamt),
	                            "# PSEUDO_V_ADD", []>;
	  def PSEUDO_V_SUB : Pseudo<(outs),
	                            (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
	                                 i64imm:$srcRType, i64imm:$shamt),
	                            "# PSEUDO_V_SUB", []>;
	  def PSEUDO_V_MUL : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
	                            "# PSEUDO_V_MUL", []>;
	  def PSEUDO_V_FADD : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
	                             "# PSEUDO_V_FADD", []>;
	  def PSEUDO_V_FSUB : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
	                             "# PSEUDO_V_FSUB", []>;
	  def PSEUDO_V_FMUL : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
	                             "# PSEUDO_V_FMUL", []>;
	  def PSEUDO_V_FDIV : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
	                             "# PSEUDO_V_FDIV", []>;
	  def PSEUDO_V_FABS : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
	                             "# PSEUDO_V_FABS", []>;
	  def PSEUDO_V_FSQRT : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
	                              "# PSEUDO_V_FSQRT", []>;

  def PSEUDO_V_CMP_EQ : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                               "# PSEUDO_V_CMP_EQ", []>;
  def PSEUDO_V_CMP_NE : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                               "# PSEUDO_V_CMP_NE", []>;
  def PSEUDO_V_CMP_LT : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                               "# PSEUDO_V_CMP_LT", []>;
  def PSEUDO_V_CMP_LTU : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                                "# PSEUDO_V_CMP_LTU", []>;
  def PSEUDO_V_CMP_GE : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                               "# PSEUDO_V_CMP_GE", []>;
  def PSEUDO_V_CMP_GEU : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                                "# PSEUDO_V_CMP_GEU", []>;
  def PSEUDO_V_FEQ : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                            "# PSEUDO_V_FEQ", []>;
  def PSEUDO_V_FNE : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                            "# PSEUDO_V_FNE", []>;
  def PSEUDO_V_FLT : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                            "# PSEUDO_V_FLT", []>;
  def PSEUDO_V_FGE : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR),
                            "# PSEUDO_V_FGE", []>;

  def PSEUDO_V_CSEL : Pseudo<(outs),
                             (ins i64imm:$dst, i64imm:$srcP, i64imm:$srcL,
                                  i64imm:$srcR, i64imm:$srcRType),
                             "# PSEUDO_V_CSEL", []>;
  def PSEUDO_V_PSEL : Pseudo<(outs),
                             (ins i64imm:$dst, i64imm:$srcP, i64imm:$srcL),
                             "# PSEUDO_V_PSEL", []>;

  let mayLoad = 1 in
  def PSEUDO_V_LB_BRG : Pseudo<(outs),
                               (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_LB_BRG", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LH_BRG : Pseudo<(outs),
                               (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_LH_BRG", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LBU_BRG : Pseudo<(outs),
                                (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
                                     i64imm:$shamt, i64imm:$localBit),
                                "# PSEUDO_V_LBU_BRG", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LHU_BRG : Pseudo<(outs),
                                (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
                                     i64imm:$shamt, i64imm:$localBit),
                                "# PSEUDO_V_LHU_BRG", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LW_BRG : Pseudo<(outs),
                               (ins i64imm:$dst, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_LW_BRG", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LWI_U : Pseudo<(outs),
                              (ins i64imm:$dst, i64imm:$srcL, i64imm:$simm24,
                                   i64imm:$localBit, i64imm:$brgBit),
                              "# PSEUDO_V_LWI_U", []>;
  let mayLoad = 1 in
  def PSEUDO_V_LDI_U : Pseudo<(outs),
                              (ins i64imm:$dst, i64imm:$srcL, i64imm:$simm24,
                                   i64imm:$localBit, i64imm:$brgBit),
                              "# PSEUDO_V_LDI_U", []>;
  let mayStore = 1 in
  def PSEUDO_V_SB_BRG : Pseudo<(outs),
                               (ins i64imm:$srcD, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_SB_BRG", []>;
  let mayStore = 1 in
  def PSEUDO_V_SH_BRG : Pseudo<(outs),
                               (ins i64imm:$srcD, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_SH_BRG", []>;
  let mayStore = 1 in
  def PSEUDO_V_SW_BRG : Pseudo<(outs),
                               (ins i64imm:$srcD, i64imm:$srcL, i64imm:$srcR,
                                    i64imm:$shamt, i64imm:$localBit),
                               "# PSEUDO_V_SW_BRG", []>;
  let mayStore = 1 in
  def PSEUDO_V_SWI_U : Pseudo<(outs),
                              (ins i64imm:$srcD, i64imm:$base, i64imm:$simm24,
                                   i64imm:$localBit, i64imm:$brgBit),
                              "# PSEUDO_V_SWI_U", []>;
  let mayStore = 1 in
  def PSEUDO_V_SDI_U : Pseudo<(outs),
                              (ins i64imm:$srcD, i64imm:$base, i64imm:$simm24,
                                   i64imm:$localBit, i64imm:$brgBit),
                              "# PSEUDO_V_SDI_U", []>;

  def PSEUDO_V_RDADD : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                              "# PSEUDO_V_RDADD", []>;
  def PSEUDO_V_RDAND : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                              "# PSEUDO_V_RDAND", []>;
  def PSEUDO_V_RDFADD : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                               "# PSEUDO_V_RDFADD", []>;
  def PSEUDO_V_RDFMAX : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                               "# PSEUDO_V_RDFMAX", []>;
  def PSEUDO_V_RDFMIN : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                               "# PSEUDO_V_RDFMIN", []>;
  def PSEUDO_V_RDMAX : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                              "# PSEUDO_V_RDMAX", []>;
  def PSEUDO_V_RDMIN : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                              "# PSEUDO_V_RDMIN", []>;
  def PSEUDO_V_RDOR : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                             "# PSEUDO_V_RDOR", []>;
  def PSEUDO_V_RDXOR : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                              "# PSEUDO_V_RDXOR", []>;

  def PSEUDO_V_B_EQ : Pseudo<(outs),
                             (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                             "# PSEUDO_V_B_EQ", []>;
  def PSEUDO_V_B_NE : Pseudo<(outs),
                             (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                             "# PSEUDO_V_B_NE", []>;
  def PSEUDO_V_B_LT : Pseudo<(outs),
                             (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                             "# PSEUDO_V_B_LT", []>;
  def PSEUDO_V_B_GE : Pseudo<(outs),
                             (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                             "# PSEUDO_V_B_GE", []>;
  def PSEUDO_V_B_LTU : Pseudo<(outs),
                              (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                              "# PSEUDO_V_B_LTU", []>;
  def PSEUDO_V_B_GEU : Pseudo<(outs),
                              (ins i64imm:$srcL, i64imm:$srcR, BrTarget:$target),
                              "# PSEUDO_V_B_GEU", []>;
  def PSEUDO_V_B_Z : Pseudo<(outs), (ins BrTarget:$target),
                            "# PSEUDO_V_B_Z", []>;
  def PSEUDO_V_B_NZ : Pseudo<(outs), (ins BrTarget:$target),
                             "# PSEUDO_V_B_NZ", []>;
}

let hasSideEffects = 1, isTerminator = 1, isBarrier = 1, Size = 4 in
def PSEUDO_V_J : Pseudo<(outs), (ins BrTarget:$target), "# PSEUDO_V_J", []>;

// Scalar body pseudo ops. These carry scalar register encodings as immediates
// (same namespace used in vector bodies) and are lowered in MCInstLower to
// strict-v0.3 scalar encodings.
let hasSideEffects = 1, Size = 2 in {
  def PSEUDO_V_C_MOVR : Pseudo<(outs), (ins i64imm:$dst, i64imm:$srcL),
                               "# PSEUDO_V_C_MOVR", []>;
}

// Real tile-block descriptor instructions (emitted after register allocation).
let hasSideEffects = 1, isBarrier = 1, Size = 4 in {
  def BSTART_TMA  : LinxISAInst<(outs), (ins uimm5:$dtype, uimm5:$func), "", []>;
  def BSTART_CUBE : LinxISAInst<(outs), (ins uimm5:$dtype, uimm5:$func), "", []>;
  def BSTART_TEPL : LinxISAInst<(outs), (ins uimm5:$dtype, uimm10:$tileop), "", []>;

  // Vector block start markers (decoupled headers).
  //
  // The bring-up encoding exposes a single "Mode" field; keep it explicit in
  // the MachineInstr for now (vblock.launch emits Mode=0).
  def BSTART_VPAR : LinxISAInst<(outs), (ins uimm5:$mode), "", []>;
  def BSTART_VSEQ : LinxISAInst<(outs), (ins uimm5:$mode), "", []>;
  def BSTART_MPAR : LinxISAInst<(outs), (ins uimm5:$mode), "", []>;
  def BSTART_MSEQ : LinxISAInst<(outs), (ins uimm5:$mode), "", []>;

  // Decoupled-body pointer for tile/vector headers (see ISA manual: decoupled blocks).
  def B_TEXT : LinxISAInst<(outs), (ins BrTarget:$dst), "", []>;

  def B_ARG : LinxISAInst<(outs), (ins i64imm:$format), "", []>;

  // Full B.ATTR descriptor (strict-v0.3):
  // {C, DR, DataLayout, DataType, PadValue, T, aq, atom, far, rl}
  def B_ATTR : LinxISAInst<(outs), (ins i64imm:$c, i64imm:$dr,
                                       i64imm:$layout, i64imm:$dtype,
                                       i64imm:$pad, i64imm:$t,
                                       i64imm:$aq, i64imm:$atom,
                                       i64imm:$far, i64imm:$rl), "", []>;

  def B_DIM_LB0 : LinxISAInst<(outs), (ins GPR_Arch:$reg, i64imm:$imm), "", []>;
  def B_DIM_LB1 : LinxISAInst<(outs), (ins GPR_Arch:$reg, i64imm:$imm), "", []>;
  def B_DIM_LB2 : LinxISAInst<(outs), (ins GPR_Arch:$reg, i64imm:$imm), "", []>;

  // B.IOR descriptor form: multiple descriptors are legal per block.
  def B_IOR : LinxISAInst<(outs), (ins GPR_Arch:$regdst,
                                       GPR_Arch:$regsrc0,
                                       GPR_Arch:$regsrc1,
                                       GPR_Arch:$regsrc2), "", []>;

  // B.IOT / B.IOTI descriptor forms: group bit is selected by opcode.
  def B_IOT_G0  : LinxISAInst<(outs), (ins i64imm:$dst, GPR_Arch:$regsrc,
                                       i64imm:$s0r, i64imm:$s0v,
                                       i64imm:$s1r, i64imm:$s1v,
                                       i64imm:$src0, i64imm:$src1), "", []>;
  def B_IOT_G1  : LinxISAInst<(outs), (ins i64imm:$dst, GPR_Arch:$regsrc,
                                       i64imm:$s0r, i64imm:$s0v,
                                       i64imm:$s1r, i64imm:$s1v,
                                       i64imm:$src0, i64imm:$src1), "", []>;
  def B_IOTI_G0 : LinxISAInst<(outs), (ins i64imm:$dst,
                                       i64imm:$s0r, i64imm:$s0v,
                                       i64imm:$s1r, i64imm:$s1v,
                                       i64imm:$src0, i64imm:$src1,
                                       i64imm:$size), "", []>;
  def B_IOTI_G1 : LinxISAInst<(outs), (ins i64imm:$dst,
                                       i64imm:$s0r, i64imm:$s0v,
                                       i64imm:$s1r, i64imm:$s1v,
                                       i64imm:$src0, i64imm:$src1,
                                       i64imm:$size), "", []>;
}

// Branch-condition setters used by BlockISA (no explicit branch instructions).
let hasSideEffects = 1, Size = 2 in {
  def CSETC_EQ  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def CSETC_NE  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def CSETC_TGT : LinxISAInst<(outs), (ins GPR:$rs), "", []>;
}

// Compressed helpers. These encodings typically use implicit block-private
// destination registers (e.g. `->t`) and are emitted late.
let Size = 2 in {
  def C_ZEXT_B : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;
  def C_ZEXT_H : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;
  def C_ZEXT_W : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;

  def C_SEXT_B : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;
  def C_SEXT_H : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;
  def C_SEXT_W : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs), "", []>;
}

// Compressed tile/block descriptor helpers.
let hasSideEffects = 1, isBarrier = 1, Size = 2 in {
  def C_B_DIMI : LinxISAInst<(outs), (ins i64imm:$LoopNest, i64imm:$imm), "", []>;
}

// Full-width condition setters. These map to `SETC.*` forms in the ISA spec.
let hasSideEffects = 1, Size = 4 in {
  def SETC_EQ  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_NE  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_LT  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_GE  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_LTU : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_GEU : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;

	// Variants that encode a SrcR conversion modifier. These are emitted by
	// peepholes to avoid explicit word/half conversions before comparisons.
	// Block-condition setters specialized for boolean AND/OR checks.
	// These correspond to `SETC.AND/SETC.OR` in the ISA spec and are used by
	// Blockify to avoid materializing a temporary boolean value just to drive a
	// block-commit condition.
  def SETC_AND : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;
  def SETC_OR  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2), "", []>;

  // Immediate condition setters (used to avoid materializing constants just to
  // set the block commit condition).
  def SETC_EQI  : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;
  def SETC_NEI  : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;
  def SETC_LTI  : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;
  def SETC_GEI  : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;
  def SETC_LTUI : LinxISAInst<(outs), (ins GPR:$rs1, uimm12sh:$imm), "", []>;
  def SETC_GEUI : LinxISAInst<(outs), (ins GPR:$rs1, uimm12sh:$imm), "", []>;

  def SETC_ANDI : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;
  def SETC_ORI  : LinxISAInst<(outs), (ins GPR:$rs1, simm12sh:$imm), "", []>;

  def SETC_TGT : LinxISAInst<(outs), (ins GPR:$rs), "", []>;
}

// 48-bit immediate forms for SETC.ANDI/ORI when masks do not fit 12 bits.
let hasSideEffects = 1, Size = 6 in {
  def HLSETC_ANDI : LinxISAInst<(outs), (ins GPR:$rs1, simm24sh:$imm), "", []>;
  def HLSETC_ORI  : LinxISAInst<(outs), (ins GPR:$rs1, simm24sh:$imm), "", []>;
}

// Set return address for CALL blocks (BlockISA). Writes `ra` (R10) implicitly.
let hasSideEffects = 1, Defs = [R10] in
def SETRET : LinxISAInst<(outs), (ins BrTarget:$dst), "", []>;

let mayLoad = 1 in
def LBI   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LBUI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LHI   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LHUI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LWI   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LWUI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;
let mayLoad = 1 in
def LDI   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, simm12:$off), "", []>;

// PC-relative loads from a symbol (no base register).
// These are primarily used to shrink global/constant-pool accesses:
//   lw.pcr [symbol], ->rd
// and can be relaxed to HL.LW.PCR by the assembler/linker if out of range.
let mayLoad = 1, Size = 4 in {
  def LB_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LBU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LH_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LHU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LW_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LWU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def LD_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
}

// 48-bit long-range PC-relative loads from a symbol (HL.*.PCR; simm29).
// Used when the final symbol placement may exceed the 17-bit range of *.PCR,
// such as ET_REL images loaded by the Linx virt machine.
let mayLoad = 1, Size = 6 in {
  def HL_LB_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LBU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LH_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LHU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LW_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LWU_PCR  : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
  def HL_LD_PCR   : LinxISAInst<(outs GPR:$rd), (ins CallTarget:$sym), "", []>;
}

// 48-bit HL memory ops for dynamic instruction count reduction.
//
// - Writeback: pre/post-indexed addressing (base update is an explicit GPR result).
// - Pair: two adjacent accesses transfer to/from two GPRs.
let mayLoad = 1, Size = 6 in {
  def HL_LWI_PO   : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWI_PR   : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWI_UPO  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWI_UPR  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;

  def HL_LWUI_PO  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWUI_PR  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWUI_UPO : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWUI_UPR : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;

  def HL_LDI_PO   : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LDI_PR   : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LDI_UPO  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LDI_UPR  : LinxISAInst<(outs GPR:$rd,  GPR:$wb), (ins GPR:$base, simm17:$off), "", []>;

  def HL_LWIP     : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWIP_U   : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWUIP    : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LWUIP_U  : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LDIP     : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
  def HL_LDIP_U   : LinxISAInst<(outs GPR:$rd0, GPR:$rd1), (ins GPR:$base, simm17:$off), "", []>;
}

// Load register offset: base + (idx << shamt).
let mayLoad = 1 in {
  def LB   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LBU  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LH   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LHU  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LW   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LWU  : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
  def LD   : LinxISAInst<(outs GPR:$rd), (ins GPR:$base, GPR:$idx, uimm5:$shamt), "", []>;
}

let mayStore = 1 in
def SBI   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, simm12:$off), "", []>;
let mayStore = 1 in
def SHI   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, simm12:$off), "", []>;
let mayStore = 1 in
def SWI   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, simm12:$off), "", []>;
let mayStore = 1 in
def SDI   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, simm12:$off), "", []>;

// PC-relative stores to a symbol (no base register).
let mayStore = 1, Size = 4 in {
  def SB_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def SH_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def SW_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def SD_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
}

// 48-bit long-range PC-relative stores to a symbol (HL.*.PCR; simm29).
let mayStore = 1, Size = 6 in {
  def HL_SB_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def HL_SH_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def HL_SW_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
  def HL_SD_PCR : LinxISAInst<(outs), (ins GPR:$rs, CallTarget:$sym), "", []>;
}

let mayStore = 1, Size = 6 in {
  def HL_SWI_PO   : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SWI_PR   : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SWI_UPO  : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SWI_UPR  : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;

  def HL_SDI_PO   : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SDI_PR   : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SDI_UPO  : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;
  def HL_SDI_UPR  : LinxISAInst<(outs GPR:$wb), (ins GPR:$rs, GPR:$base, simm17:$off), "", []>;

  def HL_SWIP     : LinxISAInst<(outs), (ins GPR:$rs0, GPR:$rs1, GPR:$base, simm17:$off), "", []>;
  def HL_SWIP_U   : LinxISAInst<(outs), (ins GPR:$rs0, GPR:$rs1, GPR:$base, simm17:$off), "", []>;
  def HL_SDIP     : LinxISAInst<(outs), (ins GPR:$rs0, GPR:$rs1, GPR:$base, simm17:$off), "", []>;
  def HL_SDIP_U   : LinxISAInst<(outs), (ins GPR:$rs0, GPR:$rs1, GPR:$base, simm17:$off), "", []>;
}

// Store register offset: base + (idx << scale). The scale is implied by the
// store width (SB=<<0, SH=<<1, SW=<<2, SD=<<3).
let mayStore = 1 in {
  def SB   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, GPR:$idx), "", []>;
  def SH   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, GPR:$idx), "", []>;
  def SW   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, GPR:$idx), "", []>;
  def SD   : LinxISAInst<(outs), (ins GPR:$rs, GPR:$base, GPR:$idx), "", []>;
}

def LUI   : LinxISAInst<(outs GPR:$rd), (ins imm20:$imm), "", []>;
let Size = 6 in
def HLLUI : LinxISAInst<(outs GPR:$rd), (ins imm32:$imm), "", []>;
def ADDTPC : LinxISAInst<(outs GPR:$rd), (ins imm20:$imm), "", []>;

let isBranch = 1, isBarrier = 1, isTerminator = 1 in
def JUMP : LinxISAInst<(outs), (ins BrTarget:$dst), "", []>;

let isBranch = 1, isTerminator = 1 in
def BEQ  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;
let isBranch = 1, isTerminator = 1 in
def BNE  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;
let isBranch = 1, isTerminator = 1 in
def BLT  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;
let isBranch = 1, isTerminator = 1 in
def BGE  : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;
let isBranch = 1, isTerminator = 1 in
def BLTU : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;
let isBranch = 1, isTerminator = 1 in
def BGEU : LinxISAInst<(outs), (ins GPR:$rs1, GPR:$rs2, BrTarget:$dst), "", []>;

let isBranch = 1, isBarrier = 1, isIndirectBranch = 1, isTerminator = 1 in
def JR : LinxISAInst<(outs), (ins GPR:$rs), "", []>;

// SetCC (returns 0/1)
def CMPEQ  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPNE  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPLT  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPGE  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPLTU : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPGEU : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;

// Boolean helpers: CMP.AND/CMP.OR compute a 0/1 predicate without materializing
// the intermediate AND/OR result.
def CMPAND : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def CMPOR  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;

def CMPEQI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;
def CMPNEI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;
def CMPLTI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;
def CMPGEI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;
def CMPLTUI : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12:$imm), "", []>;
def CMPGEUI : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm12:$imm), "", []>;

def CMPANDI : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;
def CMPORI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm12:$imm), "", []>;

let Size = 6 in {
  def HLCMPANDI : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24:$imm), "", []>;
  def HLCMPORI  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, simm24:$imm), "", []>;
}

// Floating-point arithmetic/compare/convert (hard-float bring-up).
def FADDrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FSUBrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FMULrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FDIVrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FABSrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm2:$SrcType), "", []>;

def FEQrr   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FLTrr   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;
def FGErr   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm2:$SrcType), "", []>;

def FCVT    : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$DstType, uimm2:$SrcType), "", []>;
def FCVTZ   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$DstType, uimm2:$SrcType), "", []>;
def SCVTF   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$DstType, uimm2:$SrcType), "", []>;
def UCVTF   : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm5:$DstType, uimm2:$SrcType), "", []>;

// Conditional select. SrcP is a 0/1 predicate value.
// Note: The predicate must be in an architectural GPR (not T/U queue),
// because Blockify may rewrite T/U queue values after the comparison.
let isSelect = 1 in
def CSELrrr : LinxISAInst<(outs GPR:$rd),
                          (ins GPR_Arch:$pred, GPR:$src_true, GPR:$src_false),
                          "", []>;

// Multiply/divide/remainder (selected by patterns)
def MULrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (mul (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def DIVrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (sdiv (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def DIVUrr : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (udiv (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def REMrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (srem (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;
def REMUrr : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", [(set (i64 GPR:$rd), (urem (i64 GPR:$rs1), (i64 GPR:$rs2)))]>;

def MULWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def DIVWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def DIVUWrr : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def REMWrr  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;
def REMUWrr : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2), "", []>;

// Multiply-add helpers (peepholed in Blockify):
//   rd = rs1 * rs2 + rs3
def MADD  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, GPR:$rs3), "", []>;
def MADDW : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, GPR:$rs3), "", []>;

def : Pat<(mul i32:$a, i32:$b), (MULWrr $a, $b)>;
def : Pat<(sdiv i32:$a, i32:$b), (DIVWrr $a, $b)>;
def : Pat<(udiv i32:$a, i32:$b), (DIVUWrr $a, $b)>;
def : Pat<(srem i32:$a, i32:$b), (REMWrr $a, $b)>;
def : Pat<(urem i32:$a, i32:$b), (REMUWrr $a, $b)>;

//===----------------------------------------------------------------------===//
// Bit manipulation ops (selected by patterns)
//===----------------------------------------------------------------------===//

// Scalar CLZ/CTZ/BCNT count over a bitfield: M=imms, N=imml+1 (ISA manual).
// For generic LLVM intrinsics we use full-width ranges:
//   i64 -> M=0, N=64 (imml=63, imms=0)
//   i32 -> M=0, N=32 (imml=31, imms=0)
def CLZ  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm6:$imml, uimm6:$imms), "", []>;
def CTZ  : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm6:$imml, uimm6:$imms), "", []>;
def BCNT : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm6:$imml, uimm6:$imms), "", []>;

// Bitfield extract (selected by late peepholes).
def BXS : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm6:$imml, uimm6:$imms), "", []>;
def BXU : LinxISAInst<(outs GPR:$rd), (ins GPR:$rs1, uimm6:$imml, uimm6:$imms), "", []>;

def : Pat<(i64 (ctlz i64:$a)), (CLZ $a, (i64 63), (i64 0))>;
def : Pat<(i64 (cttz i64:$a)), (CTZ $a, (i64 63), (i64 0))>;
def : Pat<(i64 (ctpop i64:$a)), (BCNT $a, (i64 63), (i64 0))>;
def : Pat<(i64 (ctlz_zero_undef i64:$a)), (CLZ $a, (i64 63), (i64 0))>;
def : Pat<(i64 (cttz_zero_undef i64:$a)), (CTZ $a, (i64 63), (i64 0))>;

def : Pat<(i32 (ctlz i32:$a)), (CLZ $a, (i64 31), (i64 0))>;
def : Pat<(i32 (cttz i32:$a)), (CTZ $a, (i64 31), (i64 0))>;
def : Pat<(i32 (ctpop i32:$a)), (BCNT $a, (i64 31), (i64 0))>;
def : Pat<(i32 (ctlz_zero_undef i32:$a)), (CLZ $a, (i64 31), (i64 0))>;
def : Pat<(i32 (cttz_zero_undef i32:$a)), (CTZ $a, (i64 31), (i64 0))>;
