// Shared IDs are B.IOS operands, not MOVR destinations.  Accepting the
// alternate S names here aliases the five-bit MOVR destination to a GPR.
// RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s

// CHECK-COUNT-3: error: Match Instruction Error!
c.movr r2, ->S0
c.movr r2, ->S1
c.movr r2, ->S63

// Scalar aliases remain valid and are intentionally lowercase.
// RUN: printf 'c.movr r2, ->s0\nc.movr r2, ->s1\n' | llvm-mc -triple=linx64v5 -show-encoding
