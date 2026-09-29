# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s

# The second BSTART begins a continuation generation.  The catalog currently
# folds only one allocating generation, so the continuation's source-only
# binder and B.ASSEMBLE must remain physical.  In particular, the first macro
# must not consume or skip the second header/body while looking ahead.

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last, ->T<2KB>
B.ASSEMBLE 1, 0, a0, 0, 5

BSTART.TEPL TADD, FP32
C.B.DIMI 8, ->lb0
C.B.DIMI 64, ->lb1
C.B.DIMI 64, ->lb2
B.IOT T#1, T#2, mask=1111, last
B.ASSEMBLE 0, 1, a0, 4, 5
BSTOP

# CHECK: TADD{{ +}}<Row=8, Col=64, ValidRow=64, ValidCol=8, FP32>, T#1, T#2, ->T<2KB>{{\[}}base=a0, offset=0, assemble=init, writer=5{{\]}}
# CHECK: BSTART.TEPL{{.*}}TADD, FP32
# CHECK-NEXT: B.DIM{{.*}}0
# CHECK-NEXT: B.DIM{{.*}}1
# CHECK-NEXT: B.DIM{{.*}}2
# CHECK-NEXT: B.IOT{{.*}}t#1, t#2, mask=1111, last
# CHECK-NEXT: B.ASSEMBLE{{.*}}0, 1, a0, 4, 5
# CHECK-NEXT: BSTOP