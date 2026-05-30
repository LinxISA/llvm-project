# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

# CHECK-LABEL: <foo>:
# CHECK: hl.addi {{[[:space:]]+}}s0, 1,{{[[:space:]]+}}->s1
# CHECK: hl.subi {{[[:space:]]+}}s0, 2,{{[[:space:]]+}}->s1
# CHECK: hl.andi {{[[:space:]]+}}s0, 3,{{[[:space:]]+}}->s1
# CHECK: hl.ori {{[[:space:]]+}}s0, 4,{{[[:space:]]+}}->s1
# CHECK: hl.xori {{[[:space:]]+}}s0, 5,{{[[:space:]]+}}->s1
# CHECK: hl.addiw {{[[:space:]]+}}s0, 6,{{[[:space:]]+}}->s1
# CHECK: hl.subiw {{[[:space:]]+}}s0, 7,{{[[:space:]]+}}->s1
# CHECK: hl.andiw {{[[:space:]]+}}s0, 8,{{[[:space:]]+}}->s1
# CHECK: hl.oriw {{[[:space:]]+}}s0, 9,{{[[:space:]]+}}->s1
# CHECK: hl.xoriw {{[[:space:]]+}}s0, 10,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.eqi {{[[:space:]]+}}s0, 11,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.nei {{[[:space:]]+}}s0, 12,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.andi {{[[:space:]]+}}s0, 13,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.ori {{[[:space:]]+}}s0, 14,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.lti {{[[:space:]]+}}s0, 15,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.gei {{[[:space:]]+}}s0, 16,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.ltui {{[[:space:]]+}}s0, 17,{{[[:space:]]+}}->s1
# CHECK: hl.cmp.geui {{[[:space:]]+}}s0, 18,{{[[:space:]]+}}->s1
# CHECK: hl.setc.eqi {{[[:space:]]+}}s0, 19
# CHECK: hl.setc.nei {{[[:space:]]+}}s0, 20
# CHECK: hl.setc.andi {{[[:space:]]+}}s0, 21
# CHECK: hl.setc.ori {{[[:space:]]+}}s0, 22
# CHECK: hl.setc.lti {{[[:space:]]+}}s0, 23
# CHECK: hl.setc.gei {{[[:space:]]+}}s0, 24
# CHECK: hl.setc.ltui {{[[:space:]]+}}s0, 25
# CHECK: hl.setc.geui {{[[:space:]]+}}s0, 26
# CHECK: hl.addtpc {{[[:space:]]+}}28,{{[[:space:]]+}}->s1
# CHECK: hl.setret {{[[:space:]]+}}30,{{[[:space:]]+}}->ra
# CHECK: hl.div {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.divu {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.divw {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.divuw {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.rem {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.remu {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.remw {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3
# CHECK: hl.remuw {{[[:space:]]+}}s0, s1,{{[[:space:]]+}}->s2, s3

        .text
        .globl  foo
        .type   foo,@function
foo:
        hl.addi s0, 1, ->s1
        hl.subi s0, 2, ->s1
        hl.andi s0, 3, ->s1
        hl.ori s0, 4, ->s1
        hl.xori s0, 5, ->s1
        hl.addiw s0, 6, ->s1
        hl.subiw s0, 7, ->s1
        hl.andiw s0, 8, ->s1
        hl.oriw s0, 9, ->s1
        hl.xoriw s0, 10, ->s1
        hl.cmp.eqi s0, 11, ->s1
        hl.cmp.nei s0, 12, ->s1
        hl.cmp.andi s0, 13, ->s1
        hl.cmp.ori s0, 14, ->s1
        hl.cmp.lti s0, 15, ->s1
        hl.cmp.gei s0, 16, ->s1
        hl.cmp.ltui s0, 17, ->s1
        hl.cmp.geui s0, 18, ->s1
        hl.setc.eqi s0, 19
        hl.setc.nei s0, 20
        hl.setc.andi s0, 21
        hl.setc.ori s0, 22
        hl.setc.lti s0, 23
        hl.setc.gei s0, 24
        hl.setc.ltui s0, 25
        hl.setc.geui s0, 26
        hl.addtpc 28, ->s1
        hl.setret 30, ->ra
        hl.div s0, s1, ->s2, s3
        hl.divu s0, s1, ->s2, s3
        hl.divw s0, s1, ->s2, s3
        hl.divuw s0, s1, ->s2, s3
        hl.rem s0, s1, ->s2, s3
        hl.remu s0, s1, ->s2, s3
        hl.remw s0, s1, ->s2, s3
        hl.remuw s0, s1, ->s2, s3
        C.BSTOP
        .size foo, .-foo
