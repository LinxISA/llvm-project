// RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s

// CHECK: error: Match Instruction Invalid!
TGEMV <M:2, N:32, K:64, FP32, FP32> T#1, T#2, ->T<4KB>
