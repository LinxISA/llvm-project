# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# A one-source reuse word requires SrcTile1[5:1] (bits 31:27) to remain zero.
# Bit 26 alone is the lifetime bit. Reserved high bits must not be mistaken for
# a valid source or silently decoded by the lifetime fallback.
.byte 0x13, 0x5e, 0x00, 0x00
.byte 0x13, 0x5e, 0x00, 0x08
.byte 0x13, 0x5e, 0x00, 0x80

# CHECK: B.IOT t#1.reuse, mask=1111
# CHECK-NEXT: {{.*}}<unknown>
# CHECK-NEXT: {{.*}}<unknown>
