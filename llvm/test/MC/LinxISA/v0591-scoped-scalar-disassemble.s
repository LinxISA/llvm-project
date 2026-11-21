# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s

        .text
        .8byte 0x00000000001a001f # C.LDI.private, 16-bit inner instruction
        .8byte 0x000000002019051f # LWI.intra, 32-bit inner instruction
        .8byte 0x00003059000e0a1f # HL.SDI.inter, 48-bit inner instruction
        .8byte 0x00002631329b061f # CASD.AQRL.inter
        .8byte 0x000000002039051f # LW.PCR.intra
        .8byte 0x00000000001a031f # Reserved scope
        .8byte 0x00000000001a0c1f # Reserved length
        .8byte 0x00000001001a001f # Nonzero padding
        .8byte 0x00000000700b041f # DMA is excluded
        .8byte 0x06000009003e081f # HL.LB.PO with illegal SrcRType
        C.BSTOP

# CHECK: 0: {{.*}}c.ldi.private
# CHECK-NEXT: 8: {{.*}}lwi.intra
# CHECK-NEXT: 10: {{.*}}hl.sdi.inter
# CHECK-NEXT: 18: {{.*}}casd.aqrl.inter
# CHECK-NEXT: 20: {{.*}}lw.pcr.intra
# CHECK-NEXT: 28: {{.*}}<unknown>
# CHECK-NEXT: 30: {{.*}}<unknown>
# CHECK-NEXT: 38: {{.*}}<unknown>
# CHECK-NEXT: 40: {{.*}}<unknown>
# CHECK-NEXT: 48: {{.*}}<unknown>
# CHECK-NEXT: 50: {{.*}}C.BSTOP
