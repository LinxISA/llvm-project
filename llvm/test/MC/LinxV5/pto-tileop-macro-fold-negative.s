# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/valid.s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/cube.s -o %t/cube.o
# RUN: not --crash llvm-objdump -d %t/cube.o 2>&1 | FileCheck %s --check-prefix=CUBE-ERROR
# RUN: llvm-objdump -d -M no-tile-macros %t/cube.o | FileCheck %s --check-prefix=CUBE-PHYSICAL
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/gmov.s -o %t/gmov.o
# RUN: not --crash llvm-objdump -d %t/gmov.o 2>&1 | FileCheck %s --check-prefix=GMOV-ERROR
# RUN: llvm-objdump -d -M no-tile-macros %t/gmov.o | FileCheck %s --check-prefix=GMOV-PHYSICAL
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tepl.s -o %t/tepl.o
# RUN: not --crash llvm-objdump -d %t/tepl.o 2>&1 | FileCheck %s --check-prefix=TEPL-ERROR
# RUN: llvm-objdump -d -M no-tile-macros %t/tepl.o | FileCheck %s --check-prefix=TEPL-PHYSICAL

#--- valid.s

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

# Omitted LB2 now has the architecture default one and folds canonically.
BSTART.TEPL TCVT, FP32
B.DATR FP16, byte0, Null, RNONE, nosat
C.B.DIMI 1, ->lb0
C.B.DIMI 64, ->lb1
B.IOT T#1, mask=1111, last, ->T<128B>
C.BSTART.STD

#--- cube.s
# Matrix CCTRL has no lossless macro field.
# CUBE-ERROR: LLVM ERROR: LinxV5: cannot print BSTART_CUBE at 0x0 in macro instruction form
BSTART.CUBE TMATMUL, FP32
B.DATR FP32, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT T#1, T#2, mask=1111, last, ->T<128B>
C.BSTART.STD

#--- gmov.s
# Mixed PE masks across exact GMOV command groups.
# GMOV-ERROR: LLVM ERROR: LinxV5: cannot print BSTART_GMOV at 0x0 in macro instruction form
BSTART.GMOV FP32
B.IOT T#1, mask=1111
B.IOT mask=1100, last, ->T<128B>
C.BSTART.STD

#--- tepl.s
# The only IOT is not marked last.
# TEPL-ERROR: LLVM ERROR: LinxV5: cannot print BSTART_TEPL_NoMode at 0x0 in macro instruction form
BSTART.TEPL TADD, FP32
C.B.DIMI 64, ->lb0
C.B.DIMI 8, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, ->T<2KB>
C.BSTART.STD

# CHECK: TEXPANDS{{ +}}<Row=32, Col=1, FP32>, a0, ->T<128B>
# CHECK: TLOAD{{ +}}<Row=32, Col=1, FP32>, [base=a0, stride=a1], ->S0<128B>
# CHECK: TCVT{{ +}}<Col=1, ValidRow=64, FP32, FP16>, T#1, ->T<128B>
# CUBE-PHYSICAL: BSTART.CUBE TMATMUL
# GMOV-PHYSICAL: BSTART.GMOV FP32
# TEPL-PHYSICAL: BSTART.TEPL TADD
