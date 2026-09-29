# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=MACRO
# RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=PHYS

# Issue #115: the real BF16 -> E2M1X2 TCVT producer must retain the
# B.ASSEMBLE generation phase when folded and when printed again.

# A single-bundle INIT, with a non-default writer extent.
BSTART.TEPL TCVT, BF16
B.DATR e2m1x2, byte0, Null, RNONE, nosat
C.B.DIMI 32, ->lb0
C.B.DIMI 1, ->lb1
C.B.DIMI 1, ->lb2
B.IOT T#1, mask=1111, last, ->T<128B>
B.ASSEMBLE 1, 0, zero, 0, 3
BSTOP

# A MIDDLE fragment and a LAST fragment are independently foldable.  They
# must not be normalized to the historical INIT_LAST/writer=1 spelling.
BSTART.TEPL TCVT, BF16
B.DATR e2m1x2, byte0, Null, RNONE, nosat
C.B.DIMI 32, ->lb0
C.B.DIMI 1, ->lb1
C.B.DIMI 1, ->lb2
B.IOT T#1, mask=1111, last, ->T<128B>
B.ASSEMBLE 0, 0, a0, 4, 3
BSTOP

BSTART.TEPL TCVT, BF16
B.DATR e2m1x2, byte0, Null, RNONE, nosat
C.B.DIMI 32, ->lb0
C.B.DIMI 1, ->lb1
C.B.DIMI 1, ->lb2
B.IOT T#1, mask=1111, last, ->T<128B>
B.ASSEMBLE 0, 1, a0, 8, 3
BSTOP

# MACRO: TCVT{{ +}}<Col=1, ValidRow=1, ValidCol=32, BF16, e2m1x2>, T#1, ->T<128B>{{\[}}base=zero, offset=0, assemble=init, writer=3{{\]}}
# MACRO: TCVT{{ +}}<Col=1, ValidRow=1, ValidCol=32, BF16, e2m1x2>, T#1, ->T<128B>{{\[}}base=a0, offset=4, assemble=middle, writer=3{{\]}}
# MACRO: TCVT{{ +}}<Col=1, ValidRow=1, ValidCol=32, BF16, e2m1x2>, T#1, ->T<128B>{{\[}}base=a0, offset=8, assemble=last, writer=3{{\]}}

# PHYS: BSTART.TEPL{{.*}}TCVT, BF16
# PHYS: B.DATR{{.*}}e2m1x2
# PHYS: B.ASSEMBLE{{.*}}1, 0, zero, 0, 3
# PHYS: BSTART.TEPL{{.*}}TCVT, BF16
# PHYS: B.ASSEMBLE{{.*}}0, 0, a0, 4, 3
# PHYS: BSTART.TEPL{{.*}}TCVT, BF16
# PHYS: B.ASSEMBLE{{.*}}0, 1, a0, 8, 3

# Feed the printed forms back through the assembler.  This verifies that the
# phase/writer information is not merely cosmetic in the disassembler.
TCVT <Col=1, ValidRow=1, BF16, e2m1x2>, T#1, ->T<128B>[base=zero, offset=0, assemble=init, writer=3]
TCVT <Col=1, ValidRow=1, BF16, e2m1x2>, T#1, ->T<128B>[base=a0, offset=4, assemble=middle, writer=3]
TCVT <Col=1, ValidRow=1, BF16, e2m1x2>, T#1, ->T<128B>[base=a0, offset=8, assemble=last, writer=3]