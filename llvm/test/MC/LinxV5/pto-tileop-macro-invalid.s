# RUN: not llvm-mc -triple=linx64v5 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s

TADD <Row=7, Col=64, FP32>, T#1, T#2, ->T<2KB>
TADD <Row=8, Col=64, FP32>
TADD <Row=8, Col=64, FP32>, a0, T#2, ->T<2KB>
TADD <Row=8, Col=64, FP32>, T#1, T#2, T#3, ->T<2KB>
TLOAD <ValidK=16, ValidN=8, TotalK=32, FP16, ND2DN>, [a0, a1, a2], ->S0<128B>
TLOAD <ValidK=16, ValidN=8, TotalK=32, FP16, OHWI2NK>, [a0, a1], ->S0<128B>
TLOAD <ValidK=16, ValidN=8, TotalK=32, FP16, OHWI2NK>, [a0, a1, a2, a3], ->S0<128B>
TLOAD.CUBE <Row=1, FP32, ND2M32>, [base=a0, stride=a1], ->T<2KB>
TLOAD.WEIGHT <ValidK=16, ValidN=8, TotalK=32, FP16, OHWI2NK>, [a0, a1, a2], ->S0<128B>
TSTORE.CUBE <Row=32, Col=1, FP32, M322ND>, T#1, [base=a0, stride=a1]
TMATMUL <M=1, N=1, K=1, FP32, PE0>, S0, S1, ->T<128B>
TMATMUL <FP32, FPAttrs(TransposeB=1)>, T#1, T#2, ->T<128B>
TMATMUL <FP32, TransposeB=1>, T#1, T#2, ->T<128B>
TADD <Row=8, Col=64, FP32>, T#1[base=r24, offset=0], T#2, ->T<2KB>
TADD <Row=8, Col=64, FP32>, T#1[base=a0, offset=2048], T#2, ->T<2KB>
TADDS <Row=8, Col=64, FP32>, T#1, a0[base=a1, offset=0], ->T<2KB>
TADD <Row=8, Col=64, FP32>, T#1[subview=(base=a0, offset=0)], T#2, ->T<2KB>
TADD <Row=8, Col=64, FP32>, T#1[base=a0, offset=0, size=<128B>], T#2, ->T<2KB>
TEXPANDS <Row=16, Col=8, ValidRow=8, U8, CUBE_M16>, a2, ->T<2KB>
TADD <Row=8, Col=64, FP32>, T#1, T#2, ->T#1<2KB>

# CHECK-COUNT-20: error: {{(unknown operand|TileOp operands/configuration do not match any exact PTO 0.58.6 form)}}
