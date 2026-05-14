// RUN: not llvm-mc %s --triple=linx64v5 --show-encoding 2>&1 | FileCheck %s --dump-input always -vv
// CHECK: error: Match Instruction Error!
BSTART.VPAR