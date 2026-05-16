# RUN: llvm-mc -filetype=obj -triple=linx64-linux-musl -mattr=-relax %s -o %t.o
# RUN: ld.lld -shared %t.o -o %t.so
# RUN: llvm-readelf -r %t.so | FileCheck %s --check-prefix=RELOC

# RELOC: There are no relocations in this file.

  .text
  .globl foo
  .type foo,@function
foo:
.Ltmp0:
  addtpc  %tpcrel_hi(local), ->t
  ldi.u   [t#1, %tpcrel_lo(.Ltmp0)], ->a0
.Lfunc_end0:
  .size foo, .Lfunc_end0-foo

  .section .data
local:
  .word 0x11223344
