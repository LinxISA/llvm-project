# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
# RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s --check-prefix=DIS

hl.bfi a0, a1, 2, 4, ->a2
hl.bfi a0, a1, 63, 0, ->a2
hl.bfi a0, a1, 17, 17, ->a2
hl.bfi a0, a1, 0, 63, ->a2

# ENC: hl.bfi a0, a1, 2, 4, ->a2
# ENC-SAME: encoding: [0x2e,0x10,0x4d,0x22,0x31,0x00]
# ENC: hl.bfi a0, a1, 63, 0, ->a2
# ENC-SAME: encoding: [0xfe,0x03,0x4d,0x22,0x31,0x00]
# ENC: hl.bfi a0, a1, 17, 17, ->a2
# ENC-SAME: encoding: [0x1e,0x45,0x4d,0x22,0x31,0x00]
# ENC: hl.bfi a0, a1, 0, 63, ->a2
# ENC-SAME: encoding: [0x0e,0xfc,0x4d,0x22,0x31,0x00]

# DIS: hl.bfi a0, a1, 2, 4, ->a2
# DIS-NEXT: hl.bfi a0, a1, 63, 0, ->a2
# DIS-NEXT: hl.bfi a0, a1, 17, 17, ->a2
# DIS-NEXT: hl.bfi a0, a1, 0, 63, ->a2
