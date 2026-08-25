// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s

// Invalid B.FPATR field/combinations.
B.FPATR 63, 7, 15, 1, 1, 1, 1, 0, 0, 0
B.FPATR 0, 0, 10, 1, 1, 0, 0, 0, 0, 0
B.FPATR 0, 0, 0, 0, 0, 1, 0, 0, 0, 0
B.FPATR 0, 0, 0, 0, 1, 0, 1, 0, 0, 0
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 2

// CHECK: error: Match Instruction Invalid!
// CHECK: error: Match Instruction Invalid!
// CHECK: error: Match Instruction Invalid!
// CHECK: error: Match Instruction Invalid!
// CHECK: error: Match Instruction Error!
