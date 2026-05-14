# RUN: llvm-mc -filetype=obj -triple=linx64v5-linux-musl -mattr=+relax %s -o %t.o
# RUN: ld.lld -Ttext=0x0 -Tdata=0x1ff000 %t.o -o %t
# RUN: llvm-objdump -d %t | FileCheck %s

  .text
  .globl foo
  .p2align 1
  .type foo,@function
foo:
.Ltmp0:
  // Here we want to test when reloaction value satisfied isInt<22>(value+0x800) can be relax at addtpc instruction.
  // This case the relocation value is 0x1ff000, so the addtpc is relax.
  # CHECK: <foo>:
  # CHECK-NEXT: 001fff87      addtpc 511, 	->t
  # CHECK-NEXT: 000c3129      ldi.u	[t#1, 0], 	->a0
  addtpc  %tpcrel_hi(val), ->t
  ldi.u   [t#1, %tpcrel_lo(.Ltmp0)], ->a0
.Lfunc_end0:
  .size   foo, .Lfunc_end0-foo

  .section .data
val:
  .word 0x66666666
