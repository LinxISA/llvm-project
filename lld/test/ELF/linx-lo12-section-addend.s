# RUN: llvm-mc -filetype=obj -triple=linx64 %s -o %t.o
# RUN: ld.lld %t.o -Ttext=0x10000 -o %t
# RUN: llvm-readobj -r %t | FileCheck %s --check-prefix=RELOC
# RUN: llvm-objdump -d --no-show-raw-insn --triple=linx64 %t | FileCheck %s --check-prefix=DIS

# Section-symbol + addend pairs are how the Linx kernel stores early function
# pointers into structs like _pt_ops. The linker must recover the matching
# HI20 relocation using the full section-relative target, not just the bare
# section symbol, or the low 12 bits get zeroed and indirect calls jump into
# the middle of a block.

# RELOC:      Relocations [
# RELOC-NEXT: ]

# DIS:      <_start>:
# DIS-NEXT: addtpc  0, ->t
# DIS-NEXT: addi    t#1, 48, ->t
# DIS-NEXT: c.bstop
# DIS:      10030:  c.bstop

  .text
  .globl _start
_start:
  addtpc .text+0x30, ->t
  addi   t#1, .text+0x30, ->t
  c.bstop
  .space 0x30
  c.bstop
