# RUN: llvm-mc -filetype=obj -triple=linx64-linux-musl -mattr=-relax %s -o %t.o
# RUN: ld.lld -static -pie %t.o -o %t
# RUN: llvm-readelf -s %t | FileCheck %s --check-prefix=SYMS

# SYMS: _DYNAMIC

  .text
  .globl _start
  .type _start,@function
_start:
  addtpc %tpcrel_hi(_DYNAMIC), ->a1
  addi   a1, %tpcrel_lo(_DYNAMIC), ->a1
  .size _start, .-_start

  .weak _DYNAMIC
  .hidden _DYNAMIC
