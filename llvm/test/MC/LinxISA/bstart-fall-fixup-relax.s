# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -dr %t | FileCheck %s

foo:
  BSTART.STD FALL, .Lfixup
  addi zero, 0, ->a0

.section .fixup,"ax"
.Lfixup:
  addi zero, 1, ->a0

# CHECK: <foo>:
# CHECK: HL.BSTART.STD
# CHECK: R_LINX_HL_BSTART30_PCREL .fixup
