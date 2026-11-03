# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: not llvm-objdump -d --no-show-raw-insn %t.o >%t.out 2>%t.err
# RUN: FileCheck %s --check-prefix=ERR < %t.err
# RUN: FileCheck %s --check-prefix=PHYS < %t.out
# ERR-COUNT-3: has no complete macro instruction form
# ERR: 3 TileOp bundle(s) have no complete macro instruction form

# Duplicate dimensions must not be silently combined.
BSTART.TEPL TCVT, FP32
B.DATR FP16, byte0, Null, RNONE, nosat
C.B.DIMI 32, ->lb1
B.IOT T#1, mask=1111, last, ->T<128B>
C.B.DIMI 16, ->lb1
# PHYS: BSTART.TEPL TCVT
# PHYS: C.B.DIMI 16,

# A second attribute command remains an unconsumed record.
BSTART.TEPL TADD, FP32
B.DATR CUBE_M32.normal, Null
B.DATR CUBE_M32.normal, Null
C.B.DIMI 32, ->lb1
B.IOT T#1, U#1, mask=1111, last, ->T<128B>
# PHYS: BSTART.TEPL TADD

# A named CCTRL must not hide the independent ByteId bits.
BSTART.CUBE TMATMUL.ACC, FP32
B.DATR FP32, byte1, CCTRL.RawAccumulatorInternalAccHint, RNONE, nosat
B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
B.IOT M#1, mask=1111
B.IOT T#1, U#1, mask=1111, last, ->N<128B>
# PHYS: BSTART.CUBE TMATMUL.ACC
