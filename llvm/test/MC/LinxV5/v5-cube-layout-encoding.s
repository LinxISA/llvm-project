# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# PTO-ISA ADR-0070: B.DATR.Layout assigned codes decode to their canonical
# names; reserved codes (2,5,7,10..16,19,29,31) must fail closed (raw words
# print <unknown> rather than a made-up layout name).

# ND2M32 (21)
.byte 0xa3, 0x1a, 0xf0, 0x01
# M322ND (24)
.byte 0x23, 0x1c, 0xf0, 0x19
# reserved layout 2
.byte 0x23, 0x11, 0xf0, 0x01
# reserved layout 31
.byte 0xa3, 0x1f, 0xf0, 0x01

# CHECK: B.DATR{{.*}}ND2M32.normal
# CHECK: B.DATR{{.*}}M322ND.normal
# CHECK: <unknown>
# CHECK: <unknown>