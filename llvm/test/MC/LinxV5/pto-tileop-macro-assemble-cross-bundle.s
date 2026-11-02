# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: not --crash llvm-objdump -d --no-show-raw-insn %t.o 2>&1 | FileCheck %s --check-prefix=ERROR
# RUN: llvm-objdump -d --no-show-raw-insn -M no-tile-macros %t.o | FileCheck %s --check-prefix=PHYSICAL

# The second BSTART begins a continuation generation.  The catalog currently
# folds only one allocating generation, so the continuation's source-only
# binder and B.ASSEMBLE cannot be printed as a macro. The default view must
# report that failure; explicit physical output still shows both generations.

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

# ERROR: LLVM ERROR: LinxV5: cannot print BSTART_TEPL_NoMode at 0x12 in macro instruction form
# PHYSICAL: BSTART.TEPL{{.*}}TADD, FP32
# PHYSICAL: B.ASSEMBLE{{.*}}1, 0, a0, 0, 5
# PHYSICAL-NEXT: BSTART.TEPL{{.*}}TADD, FP32
# PHYSICAL-NEXT: B.DIM{{.*}}0
# PHYSICAL-NEXT: B.DIM{{.*}}1
# PHYSICAL-NEXT: B.DIM{{.*}}2
# PHYSICAL-NEXT: B.IOT{{.*}}t#1, t#2, mask=1111, last
# PHYSICAL-NEXT: B.ASSEMBLE{{.*}}0, 1, a0, 4, 5
# PHYSICAL-NEXT: BSTOP
