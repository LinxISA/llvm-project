# RUN: not llvm-mc -triple=linx64 -show-encoding %s 2>&1 | FileCheck %s

BSTART.TMOV FP16
B.IOT t#1, mask=1111, last, ->u<16KB>
C.BSTOP

# CHECK: error: tile size must be in strict range 128B..8KB
