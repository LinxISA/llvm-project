# RUN: not llvm-mc -triple=linx64v5 -show-encoding %s 2>&1 | FileCheck %s

# B.ASSEMBLE accepts only binary INIT/LAST flags, absolute GPR R0..R23, and
# WriterSizeCode 0..12.  Keep these malformed encodings fail-closed.

# CHECK: B.ASSEMBLE 2, 0, r0, 0, 1
B.ASSEMBLE 2, 0, r0, 0, 1
# CHECK: B.ASSEMBLE 0, 2, r0, 0, 1
B.ASSEMBLE 0, 2, r0, 0, 1
# CHECK: B.ASSEMBLE 1, 0, r24, 0, 1
B.ASSEMBLE 1, 0, r24, 0, 1
# CHECK: B.ASSEMBLE 1, 0, u#1, 0, 1
B.ASSEMBLE 1, 0, u#1, 0, 1
# CHECK: B.ASSEMBLE 1, 0, r0, 0, 13
B.ASSEMBLE 1, 0, r0, 0, 13