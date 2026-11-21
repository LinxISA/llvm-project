# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped -filetype=obj %s -o %t
# RUN: llvm-readobj --string-dump=.note.pto.isa %t | FileCheck %s --check-prefix=NOTE
# RUN: echo 'C.BSTOP' | llvm-mc -triple=linx64 -filetype=obj -o %t.legacy
# RUN: llvm-readobj --string-dump=.note.pto.isa %t.legacy | FileCheck %s --check-prefix=LEGACY
# RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
# RUN: not llvm-mc -triple=linx64 -filetype=obj %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=FEATURE
# RUN: echo 'dma.private [a0], a1' | not llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped 2>&1 | FileCheck %s --check-prefix=EXCLUDED
# RUN: echo 'C.BSTART.private' | not llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped 2>&1 | FileCheck %s --check-prefix=EXCLUDED
# RUN: echo 'lw.pcr.intra [external], ->a0' | not llvm-mc -triple=linx64 -mattr=+pto-v0591-scoped 2>&1 | FileCheck %s --check-prefix=SYMBOL

        .text
        c.ldi.private [zero, 0], ->t
        lwi.intra [a0, 4], ->a1
        hl.sdi.inter a0, [a1, 8]
        casd.aqrl.inter [a0], a1, a2, ->a3
        lw.pcr.intra [4], ->a1
        C.BSTOP

# ENC: c.ldi.private{{.*}} encoding: [0x1f,0x00,0x1a,0x00,0x00,0x00,0x00,0x00]
# ENC: lwi.intra{{.*}} encoding: [0x1f,0x05,0x99,0x21,0x11,0x00,0x00,0x00]
# ENC: hl.sdi.inter{{.*}} encoding: [0x1f,0x0a,0x0e,0x00,0x59,0x30,0x31,0x02]
# ENC: casd.aqrl.inter{{.*}} encoding: [0x1f,0x06,0x9b,0x32,0x31,0x26,0x00,0x00]
# ENC: lw.pcr.intra{{.*}} encoding: [0x1f,0x05,0xb9,0x21,0x02,0x00,0x00,0x00]
# ENC: C.BSTOP{{.*}} encoding: [0x00,0x00]
# NOTE: {"encoding_abi":"pto-isa-0.59.1-mode-function-v1","encoding_projection_sha256":"d90c881754d66f2cd26f6014d0b582cba88307e70123583d87fc59a918229c28","release":"0.59.1"}
# LEGACY: "release":"0.58.7"
# DIS: 0: {{.*}}c.ldi.private
# DIS-NEXT: 8: {{.*}}lwi.intra
# DIS-NEXT: 10: {{.*}}hl.sdi.inter
# DIS-NEXT: 18: {{.*}}casd.aqrl.inter
# DIS-NEXT: 20: {{.*}}lw.pcr.intra
# DIS-NEXT: 28: {{.*}}C.BSTOP
# FEATURE: error: scoped Scalar requires +pto-v0591-scoped
# EXCLUDED: error: no matching scoped Scalar form
# SYMBOL: error: scoped Scalar symbol operands require a scoped relocation
