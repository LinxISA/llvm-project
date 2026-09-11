# RUN: llvm-mc %s --triple=linx64v5 -filetype=obj -o %t
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=FOLD
# RUN: llvm-objdump -d -M linxv5-no-aliases %t 2>&1 | FileCheck %s --check-prefix=PHYS

# PTO 0.58.6 TileOp macro bundle folding (issue #90): a complete physical
# bundle that exactly matches a schema plain form disassembles back to one
# macro line; -linxv5-no-aliases keeps physical assembly.

# FOLD: TADD <32, 16, 32, FP32>, t#0, t#1, ->t<8KB>
# FOLD: TMATMUL <16, 32, 64, FP32>, t#0, t#1, ->t<2KB>
# PHYS: BSTART.TEPL TADD, FP32
# PHYS: BSTART.CUBE TMATMUL, FP32
# PHYS-NOT: TADD <32

TADD <32, 16, 32, FP32>, t#1, t#2, ->t#3<8KB>
TMATMUL <16, 32, 64, FP32>, t#1, t#2, ->t#3<2KB>

# fail-closed cases (delivery stage 5): noncanonical or incomplete
# bundles must stay physical. Each case emits an object and checks that
# no folded macro line appears.
# RUN: echo 'BSTART.TEPL TADD, FP32' > %t.phys1
# RUN: echo 'B.DATR NORM.normal, Zero' >> %t.phys1
# RUN: echo 'C.B.DIMI 32, ->lb0' >> %t.phys1
# RUN: echo 'B.IOT t#1, t#2, mask=1111, last, ->t<8KB>' >> %t.phys1
# RUN: echo 'BSTART.CUBE TMATMUL, FP32' >> %t.phys1
# RUN: echo 'B.FPATR 0, 0, 0, 0, 0, 0, 0, 0, 0, 0' >> %t.phys1
# RUN: echo 'C.B.DIMI 16, ->lb0' >> %t.phys1
# RUN: echo 'B.IOT t#1, mask=1111' >> %t.phys1
# RUN: echo 'B.IOT mask=1111, last, ->t<2KB>' >> %t.phys1
# RUN: echo 'BSTOP' >> %t.phys1
# Missing BSTOP after the first bundle: the TADD bundle is followed by a
# BSTART (next-bundle boundary commits it but the object must fold only
# complete shapes); the file then ends with an unterminated CUBE bundle.
# RUN: llvm-mc %t.phys1 --triple=linx64v5 -filetype=obj -o %t2.o
# RUN: llvm-objdump -d %t2.o | FileCheck %s --check-prefix=NOCLOSE
# NOCLOSE-NOT: TADD <
# NOCLOSE: BSTART.TEPL TADD

# Incomplete bundle (no records between DIM and BSTOP) stays physical.
# RUN: echo 'BSTART.TEPL TADD, FP32' > %t.phys2
# RUN: echo 'C.B.DIMI 32, ->lb0' >> %t.phys2
# RUN: echo 'BSTOP' >> %t.phys2
# RUN: llvm-mc %t.phys2 --triple=linx64v5 -filetype=obj -o %t3.o
# RUN: llvm-objdump -d %t3.o | FileCheck %s --check-prefix=NOREC
# NOREC-NOT: TADD <
# NOREC: BSTART.TEPL TADD
