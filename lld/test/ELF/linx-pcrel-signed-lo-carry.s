# RUN: llvm-mc -filetype=obj -triple=linx64 %s -o %t.o
# RUN: ld.lld --no-relax -m elf64linx -Ttext=0x10000 \
# RUN:   --defsym=positive_below=0x107ff --defsym=positive_carry=0x10808 \
# RUN:   --defsym=negative_no_carry=0xf810 --defsym=negative_carry=0xf817 \
# RUN:   -e _start %t.o -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

# ADDTPC contributes a page-scaled signed HI20 while ADDI contributes a signed
# LO12. Check both sides of bit 11 for positive and negative displacements.

# CHECK:      <_start>:
# CHECK-NEXT: addtpc  0, ->a0
# CHECK-NEXT: addi    a0, 2047, ->a0
# CHECK-NEXT: addtpc  1, ->a1
# CHECK-NEXT: addi    a1, 2048, ->a1
# CHECK-NEXT: addtpc  0, ->a2
# CHECK-NEXT: addi    a2, 2048, ->a2
# CHECK-NEXT: addtpc  1048575, ->a3
# CHECK-NEXT: addi    a3, 2047, ->a3

  .text
  .globl _start
_start:
  addtpc %tpcrel_hi(positive_below), ->a0
  addi a0, %tpcrel_lo(positive_below), ->a0
  addtpc %tpcrel_hi(positive_carry), ->a1
  addi a1, %tpcrel_lo(positive_carry), ->a1
  addtpc %tpcrel_hi(negative_no_carry), ->a2
  addi a2, %tpcrel_lo(negative_no_carry), ->a2
  addtpc %tpcrel_hi(negative_carry), ->a3
  addi a3, %tpcrel_lo(negative_carry), ->a3
