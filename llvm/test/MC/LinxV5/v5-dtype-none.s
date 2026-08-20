// PTO 0.58.1: code 31 is the canonical DTYPE_NONE inheritance sentinel.
// Work Package P0-5: token + numeric 31 must assemble; reserved codes
// (15, 21..23, 29..30) rejected.
// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s
// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=NEG

// CHECK: B.DATR NORM.normal, Null
B.DATR dtype_none, RNONE, NOSAT
// CHECK: B.DATR NORM.normal, Null
B.DATR 31, RNONE, NOSAT

// NEG: error: Match Instruction Error!
B.DATR 15, RNONE, NOSAT
// NEG: error: Match Instruction Error!
B.DATR 29, RNONE, NOSAT
// NEG: error: Match Instruction Error!
B.DATR 214, RNONE, NOSAT
