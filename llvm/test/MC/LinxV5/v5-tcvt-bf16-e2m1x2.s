# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=DIS

# Issue #115: keep the minimal BF16 -> e2m1x2 conversion legal.  The source
# datatype is carried by BSTART and the destination datatype by B.DATR.

# CHECK: BSTART.TEPL TCVT, BF16
# CHECK: B.DATR e2m1x2
# DIS: BSTART.TEPL TCVT, BF16
# DIS: B.DATR e2m1x2
BSTART.TEPL TCVT, BF16
B.DATR e2m1x2, byte0, Null, RNONE, NOSAT
C.B.DIMI 32, ->lb0
C.B.DIMI 1, ->lb1
C.B.DIMI 1, ->lb2
B.IOT T#1, mask=1111, last, ->T<128B>
BSTOP