# RUN: rm -rf %t && split-file %s %t && cd %t
# RUN: llvm-mc -filetype=obj -triple=linx64 main.s -o main.o
# RUN: ld.lld -T link.ld main.o -o linked
# RUN: llvm-readobj -r linked | FileCheck %s --check-prefix=RELOC
# RUN: llvm-objdump -d --no-show-raw-insn --triple=linx64 linked | FileCheck %s --check-prefix=DIS

# This covers linker-defined section-boundary symbols that live in synthetic
# output sections like .init.data. Linx LO12 recovery must fall back to
# symbol-based HI20 pairing instead of rejecting those anchors early.

# RELOC:      Relocations [
# RELOC-NEXT: ]

# DIS:      <_start>:
# DIS:      addtpc
# DIS:      addi

#--- main.s
  .text
  .globl _start
_start:
  addtpc __setup_start, ->t
  addi   t#1, __setup_start, ->t
  c.bstop

  .section .init.data,"aw",@progbits
  .quad 0x1122334455667788

#--- link.ld
SECTIONS {
  . = 0x10000;
  .text : { *(.text) }
  .init.data : {
    __setup_start = .;
    *(.init.data)
    __setup_end = .;
  }
}
