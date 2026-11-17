# RUN: not llvm-mc -triple=linx64v5 %s 2>&1 | FileCheck %s
prf.intra [a0, a1]
# CHECK: error: shareability suffix requires a covered Scalar load/store or atomic instruction
dma.inter [a0], a1
# CHECK: error: shareability suffix requires a covered Scalar load/store or atomic instruction
addi.private a0, 1, ->a1
# CHECK: error: shareability suffix requires a covered Scalar load/store or atomic instruction
TLOAD.intra <Row=2, Col=64, FP32>, [a0], ->T<512B>
# CHECK: error: shareability suffix requires a covered Scalar load/store or atomic instruction
