# REQUIRES: linxv5
# RUN: llvm-mc -triple=linx64v5 -linxv5-emit-pto-isa-identity -filetype=obj %s -o %t.o
# RUN: ld.lld --pto-isa-identity -r %t.o %t.o -o %t.link.o
# RUN: llvm-readobj --notes %t.link.o | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.missing.o
# RUN: not ld.lld --pto-isa-identity -r %t.o %t.missing.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MISSING
# RUN: llvm-mc -triple=linx64v5 -defsym=MISMATCH=1 -filetype=obj %s -o %t.mismatch.o
# RUN: not ld.lld --pto-isa-identity -r %t.o %t.mismatch.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MISMATCH
# RUN: ld.lld --pto-isa-identity -e 0 --gc-sections %t.o %t.o -o %t.elf
# RUN: llvm-readobj --notes --program-headers %t.elf | FileCheck %s --check-prefix=ELF
.text
.byte 0
.ifdef MISMATCH
.section .note.pto.isa,"a",@note
.p2align 2
.long 4, 3, 1
.asciz "PTO"
.ascii "old"
.p2align 2
.endif
# CHECK-COUNT-1: Owner: PTO
# CHECK-NOT: Owner: PTO
# MISSING: expected exactly one PTO ISA identity
# MISMATCH: invalid or mismatched PTO ISA identity
# ELF: Type: PT_NOTE
# ELF-COUNT-1: Owner: PTO
# ELF-NOT: Owner: PTO
