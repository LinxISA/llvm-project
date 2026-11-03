# RUN: split-file %s %t
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/valid.s -o %t/valid.o
# RUN: llvm-objdump -d --no-show-raw-insn - < %t/valid.o | FileCheck %s
# RUN: llvm-objdump -d %t/valid.o >/dev/null 2>/dev/null
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/cube.s -o %t/cube.o
# RUN: not llvm-objdump -d --no-show-raw-insn %t/cube.o >%t/cube.out 2>%t/cube.err
# RUN: FileCheck %s --check-prefix=CUBE-OUT < %t/cube.out
# RUN: FileCheck %s --check-prefix=CUBE-ERR < %t/cube.err
# RUN: llvm-objdump -d -M no-tile-macros %t/cube.o | FileCheck %s --check-prefix=CUBE-PHYSICAL
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/gmov.s -o %t/gmov.o
# RUN: not llvm-objdump -d --no-show-raw-insn %t/gmov.o >%t/gmov.out 2>%t/gmov.err
# RUN: FileCheck %s --check-prefix=GMOV-OUT < %t/gmov.out
# RUN: FileCheck %s --check-prefix=GMOV-ERR < %t/gmov.err
# RUN: llvm-objdump -d -M no-tile-macros %t/gmov.o | FileCheck %s --check-prefix=GMOV-PHYSICAL
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %t/tepl.s -o %t/tepl.o
# RUN: not llvm-objdump -d --no-show-raw-insn %t/tepl.o >%t/tepl.out 2>%t/tepl.err
# RUN: FileCheck %s --check-prefix=TEPL-OUT < %t/tepl.out
# RUN: FileCheck %s --check-prefix=TEPL-ERR < %t/tepl.err
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
# InternalAccHint requires an ACC operation. An illegal hint remains physical
# even though all four CCTRL values now have a lossless macro field.
# CUBE-ERR: LinxV5 error: TileOp header 'BSTART.CUBE' at 0x0 has no complete macro instruction form
# CUBE-OUT: BSTART.CUBE TMATMUL
# CUBE-OUT: B.FPATR PreQuantMode=
# CUBE-OUT: B.IOT t#1, t#2, mask=1111, last, ->t<128B>
BSTART.CUBE TMATMUL, FP32
B.DATR FP32, byte0, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT T#1, T#2, mask=1111, last, ->T<128B>
C.BSTART.STD

#--- gmov.s
# Mixed PE masks across exact GMOV command groups.
# GMOV-ERR: LinxV5 error: TileOp header 'BSTART.GMOV' at 0x0 has no complete macro instruction form
# GMOV-OUT: BSTART.GMOV FP32
# GMOV-OUT: B.IOT mask=1100, last, ->t<128B>
BSTART.GMOV FP32
B.IOT T#1, mask=1111
B.IOT mask=1100, last, ->T<128B>
C.BSTART.STD

#--- tepl.s
# The only IOT is not marked last.
# TEPL-ERR: LinxV5 error: TileOp header 'BSTART.TEPL' at 0x0 has no complete macro instruction form
# TEPL-OUT: BSTART.TEPL TADD
# TEPL-OUT: B.IOT t#1, t#2, mask=1111, ->t<2KB>
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
