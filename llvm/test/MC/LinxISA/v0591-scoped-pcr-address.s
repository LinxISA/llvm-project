# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

        .text
        .globl backward_target
backward_target:
        C.BSTOP
        .8byte 0x00000002a139051f # 2: align_down(2, 4) + 5 * 4
        .8byte 0xffff21b9fffe0a1f # a: align_down(10, 4) - 2 * 4
        C.BSTOP
        .globl forward_target
forward_target:
        C.BSTOP

# CHECK: 2: {{.*}}lw.pcr.intra{{.*}} <forward_target>
# CHECK: a: {{.*}}hl.lw.pcr.inter{{.*}} <backward_target>
