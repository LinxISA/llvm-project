# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s

hl.bfi a0, a1, 0, 64, ->a2

# CHECK: error: Match Instruction Error!
