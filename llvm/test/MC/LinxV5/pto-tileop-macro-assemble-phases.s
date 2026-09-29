# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s

# B.ASSEMBLE is part of the destination binding and must not prevent folding
# merely because it is not the historical INIT_LAST/writer=1 spelling.

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last, ->T<2KB>
B.ASSEMBLE 1, 0, a0, 0, 0
BSTOP

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last, ->T<2KB>
B.ASSEMBLE 1, 0, a0, 0, 1
BSTOP

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last, ->T<2KB>
B.ASSEMBLE 0, 0, a0, 0, 5
BSTOP

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last, ->T<2KB>
B.ASSEMBLE 0, 1, a0, 0, 12
BSTOP

# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=64, ValidCol=8, FP32>, T#1, T#2, ->T<2KB>{{\[}}base=a0, offset=0, assemble=init, writer=0{{\]}}
# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=64, ValidCol=8, FP32>, T#1, T#2, ->T<2KB>{{\[}}base=a0, offset=0, assemble=init, writer=1{{\]}}
# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=64, ValidCol=8, FP32>, T#1, T#2, ->T<2KB>{{\[}}base=a0, offset=0, assemble=middle, writer=5{{\]}}
# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=64, ValidCol=8, FP32>, T#1, T#2, ->T<2KB>{{\[}}base=a0, offset=0, assemble=last, writer=12{{\]}}

# The canonical suffix is accepted by the macro parser as well as printed by
# the disassembler.  It carries the writer extent, not the destination size.
TADD <Row=8, Col=64, FP32>, T#1, T#2, ->T<2KB>[base=a0, offset=0, assemble=middle, writer=5]