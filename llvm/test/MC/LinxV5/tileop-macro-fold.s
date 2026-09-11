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
