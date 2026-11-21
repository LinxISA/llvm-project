# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %s -o %t.scoped.o
# RUN: echo 'C.BSTOP' | llvm-mc -triple=linx64 -filetype=obj -o %t.legacy.o
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped %t.scoped.o -o %t.scoped
# RUN: llvm-readobj --string-dump=.note.pto.isa %t.scoped | FileCheck %s --check-prefix=NOTE
# RUN: llvm-objdump -d %t.scoped | FileCheck %s --check-prefix=CODE
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped -r %t.scoped.o -o %t.scoped.r.o
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped %t.scoped.r.o -o %t.scoped.relinked
# RUN: llvm-readobj --string-dump=.note.pto.isa %t.scoped.relinked | FileCheck %s --check-prefix=NOTE
# RUN: not ld.lld -m elf64linx %t.scoped.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=DEFAULT
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t.scoped.o %t.legacy.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=MIXED
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped -shared %t.scoped.o -o %t.scoped.so
# RUN: ld.lld -m elf64linx --linx-pto-v0591-scoped %t.scoped.o %t.scoped.so -o %t.dynamic
# RUN: llvm-readobj --string-dump=.note.pto.isa %t.scoped.so %t.dynamic | FileCheck %s --check-prefix=DSO
# RUN: ld.lld -m elf64linx -shared %t.legacy.o -o %t.legacy.so
# RUN: not ld.lld -m elf64linx --linx-pto-v0591-scoped %t.scoped.o %t.legacy.so -o /dev/null 2>&1 | FileCheck %s --check-prefix=OLD-DSO
# RUN: not ld.lld -m elf64linx %t.legacy.o %t.scoped.so -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEW-DSO

        .text
        .globl _start
_start:
        lwi.intra [a0, 4], ->a1
        C.BSTOP

# NOTE: {"encoding_abi":"pto-isa-0.59.1-mode-function-v1","encoding_projection_sha256":"d90c881754d66f2cd26f6014d0b582cba88307e70123583d87fc59a918229c28","release":"0.59.1"}
# CODE: <_start>:
# CODE-NEXT: {{.*}}1f 05 99 21 11 00 00 00{{.*}}lwi.intra
# DEFAULT: scoped.o: invalid .note.pto.isa: release mismatch (expected 0.58.7)
# MIXED: legacy.o: invalid .note.pto.isa: release mismatch (expected 0.59.1)
# DSO-COUNT-2: "release":"0.59.1"
# OLD-DSO: legacy.so: invalid .note.pto.isa: release mismatch (expected 0.59.1)
# NEW-DSO: scoped.so: invalid .note.pto.isa: release mismatch (expected 0.58.7)
