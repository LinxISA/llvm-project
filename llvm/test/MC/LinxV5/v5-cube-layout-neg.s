# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

# PTO-ISA ADR-0070 direction/role legality: CUBE transport selectors are
# direction-restricted. Load codes (21..23) are illegal on TSTORE, store
# codes (24..26) illegal on TLOAD, and the six codes are illegal on any
# other binding (e.g. a bare B.DATR). Reserved codes never assemble because
# no spelling maps to them.

# CHECK: error: Match Instruction Error!
TLOAD.M322ND <LB0: 1, LB1: 1, LB2: 1, FP32, Zero> [a0], ->t<1KB>

# CHECK: error: Match Instruction Error!
TSTORE.ND2M32 <LB0: 1, LB1: 1, LB2: 1, FP32>, T#1, [a1]

# CHECK: error: CUBE layout selectors are legal only on TLOAD/TSTORE
B.DATR ND2M32, FP32, Zero, 0, 0, 0, 0, 0
