// PTO 0.58.1 B.FPATR field/combo legality (Work Package P0-2).
// Legal values/combinations assemble; illegal ones are rejected by the
// AsmParser without reaching the encoder.
// RUN: llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=POS
// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --check-prefix=NEG

// POS: B.FPATR 24, 2, 0, 0, 0, 0, 0
B.FPATR 24, 2, 0, 0, 0, 0, 0
// POS: B.FPATR 0, 0, 2, 1, 1, 1, 1
B.FPATR 0, 0, 2, 1, 1, 1, 1
// POS: B.FPATR 0, 0, 9, 1, 1, 0, 0
B.FPATR 0, 0, 9, 1, 1, 0, 0

// NEG: error: Match Instruction Invalid!
B.FPATR 63, 7, 15, 1, 1, 1, 1
// NEG: error: Match Instruction Invalid!
B.FPATR 0, 0, 10, 1, 1, 0, 0
// NEG: error: Match Instruction Invalid!
B.FPATR 0, 0, 0, 0, 0, 1, 0
// NEG: error: Match Instruction Invalid!
B.FPATR 0, 0, 0, 0, 1, 0, 1
