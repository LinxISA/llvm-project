// RUN: not llvm-mc -triple=linx64v5 %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEG

// Function 9-14 (deleted TMATMUL*_FIXP) and Function 8 (legacy ACCCVT) are
// reserved/illegal. The assembler must reject their mnemonics; they must not
// parse to a public CUBE operation.

BSTART.CUBE TMATMUL.FIXP, FP32
BSTART.CUBE TMATMUL.BIAS.FIXP, FP32
BSTART.CUBE TMATMUL.ACC.FIXP, FP32
BSTART.CUBE TMATMULMX.FIXP, FP32
BSTART.CUBE TMATMULMX.BIAS.FIXP, FP32
BSTART.CUBE TMATMULMX.ACC.FIXP, FP32

// NEG: error: Match Instruction Error!
// NEG: error: Match Instruction Error!
// NEG: error: Match Instruction Error!
// NEG: error: Match Instruction Error!
// NEG: error: Match Instruction Error!
// NEG: error: Match Instruction Error!
