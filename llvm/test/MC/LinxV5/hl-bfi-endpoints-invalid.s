# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

# CHECK-COUNT-2: error: Match Instruction Error!
hl.bfi t#1, t#1, 64, 63, ->t
hl.bfi t#1, t#1, 32, 64, ->t
