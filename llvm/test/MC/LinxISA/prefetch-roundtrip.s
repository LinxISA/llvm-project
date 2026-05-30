# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

# CHECK-LABEL: <foo>:
# CHECK: prf [a0, a1]
# CHECK: prfi.u [a0, 0]
# CHECK: hl.prf.l1 [a0, a1]
# CHECK: hl.prfi.u.l1 [a0, 0]

        .text
        .globl  foo
        .type   foo,@function
foo:
        prf [a0, a1]
        prfi.u [a0, 0]
        hl.prf.l1 [a0, a1]
        hl.prfi.u.l1 [a0, 0]
        C.BSTOP
        .size   foo, .-foo
