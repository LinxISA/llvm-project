# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# Cross-kind operand-command order is canonicalized within one Block.
BSTART.TEPL TEXPANDS, FP32
C.B.DIMI 1, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 1, ->lb2
B.IOT mask=1111, last, ->T<128B>
B.IOR [a0], []
C.BSTART.STD

# The same rule applies to B.IOS/B.IOR order.
BSTART.TLSU TLOAD, FP32
C.B.DIMI 1, ->lb0
C.B.DIMI 32, ->lb1
C.B.DIMI 1, ->lb2
B.IOS mask=1111, ->S0<128B>
B.IOR [a0, a1], []
C.BSTART.STD

# Missing required Col/LB2.
BSTART.TEPL TCVT, FP32
B.DATR FP16, byte0, Null, RNONE, nosat
C.B.DIMI 1, ->lb0
C.B.DIMI 64, ->lb1
B.IOT T#1, mask=1111, last, ->T<128B>
C.BSTART.STD

# Matrix CCTRL has no lossless macro field.
BSTART.CUBE TMATMUL, FP32
B.DATR FP32, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT T#1, T#2, mask=1111, last, ->T<128B>
C.BSTART.STD

# Mixed PE masks across exact GMOV command groups.
BSTART.GMOV FP32
B.IOT T#1, mask=1111
B.IOT mask=1100, last, ->T<128B>
C.BSTART.STD

# The only IOT is not marked last.
BSTART.TEPL TADD, FP32
C.B.DIMI 64, ->lb0
C.B.DIMI 8, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, ->T<2KB>
C.BSTART.STD

# CHECK: TEXPANDS{{ +}}<Row=32, Col=1, FP32>, a0, ->T<128B>
# CHECK: TLOAD{{ +}}<Row=32, Col=1, FP32>, [base=a0, stride=a1], ->S0<128B>
# CHECK: BSTART.TEPL TCVT
# CHECK: BSTART.CUBE TMATMUL
# CHECK: BSTART.GMOV FP32
# CHECK: BSTART.TEPL TADD
