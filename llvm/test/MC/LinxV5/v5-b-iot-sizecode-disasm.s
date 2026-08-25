# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# P0-2: raw B.IOT destination words with SizeCode 0 / 13..15 must fail to
# decode (the Local contract is 1..12). Bytes are stored big-endian-ish as
# .byte sequences (little-endian word order) for an L32 B_IOT_NoSrc_Dst
# (funct3=101, PEMode=111).

# SizeCode=1  (128B)   -> decodes as B.IOT
.byte 0x13, 0xde, 0x00, 0x00
# SizeCode=10 (64KB)   -> decodes as B.IOT
.byte 0x13, 0x5e, 0x05, 0x00
# SizeCode=11 (128KB)  -> decodes as B.IOT
.byte 0x13, 0xde, 0x05, 0x00
# SizeCode=12 (256KB)  -> decodes as B.IOT
.byte 0x13, 0x5e, 0x06, 0x00
# SizeCode=0           -> source-only encoding, not a destination -> <unknown>
.byte 0x13, 0x6e, 0x00, 0x00

# CHECK: B.IOT{{.*}}->t<128B>
# CHECK: B.IOT{{.*}}->t<64KB>
# CHECK: B.IOT{{.*}}->t<128KB>
# CHECK: B.IOT{{.*}}->t<256KB>
# CHECK: <unknown>