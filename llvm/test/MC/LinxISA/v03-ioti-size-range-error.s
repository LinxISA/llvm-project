# RUN: not llvm-mc -triple=linx64 -show-encoding %s 2>&1 | FileCheck %s

BSTART.TMOV FP16
B.IOT t#1, last, ->u<8KB>
C.BSTOP

# CHECK: error: tile size must be in strict range 512B..4KB
