// RUN: not llvm-mc %s 2>&1 | FileCheck %s --check-prefixes=ASM

// ASM: error: Match Instruction Error!
B.IOR [a0,a1,a6,a7],[a0,a1]