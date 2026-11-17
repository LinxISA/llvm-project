# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s
.quad 0x000000000000031f
# CHECK: <unknown>
.quad 0x0000000000000c1f
# CHECK: <unknown>
.quad 0x000000010000001f
# CHECK: <unknown>
# Wrapped ADDI is excluded, as are DMA and Prefetch.
.quad 0x000000000015041f
# CHECK: <unknown>
.quad 0x00000000700b041f
# CHECK: <unknown>
# Reserved register modifier SrcRType=3 must not acquire a scoped form.
.quad 0x000006000009041f
# CHECK: <unknown>
# The embedded encoding must agree with its declared length.
.quad 0x00000000001a041f
# CHECK: <unknown>
# LR's ignored SrcZero is canonicalized just as in the unqualified form.
.quad 0x00000010000b051f
# CHECK: lr.b.intra
