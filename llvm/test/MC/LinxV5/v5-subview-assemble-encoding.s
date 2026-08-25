# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn - | FileCheck %s

# PTO-ISA ADR-0098: raw reserved/illegal range-modifier words must fail
# closed. A word that hits the B.SUBVIEW/B.ASSEMBLE match but carries an
# illegal cross-field combination decodes as <unknown> rather than a
# made-up instruction. .byte sequences are little-endian word order (L32).
#
# Legal:
#   B.SUBVIEW 0, zero, 0, 1       -> 0x000000d3  [0xd3,0x00,0x00,0x00]
#   B.SUBVIEW 1, a0, 2047, 12     -> 0xfff10653  [0x53,0x06,0xf1,0xff]
#   B.SUBVIEW 0, r23, 100, 5      -> 0x064b82d3  [0xd3,0x82,0x4b,0x06]
#   B.ASSEMBLE 1,0, zero, 0, 1    -> 0x800010d3  [0xd3,0x10,0x00,0x80]
#   B.ASSEMBLE 1,1, a0, 2047, 12  -> 0xfff11e53  [0x53,0x1e,0xf1,0xff]
#   B.ASSEMBLE 0,0, r23, 1, 0     -> 0x001b9053  [0x53,0x90,0x1b,0x00]
# Illegal (must be <unknown>):
#   B.SUBVIEW size=13             -> 0x000006d3
#   B.SUBVIEW size=0              -> 0x00000053
#   B.SUBVIEW RegSrc=24           -> 0x000c00d3
#   B.ASSEMBLE INIT=1,size=0      -> 0x80001053
#   B.ASSEMBLE INIT=0,size=12     -> 0x00011e53
#   B.ASSEMBLE size=13            -> 0x800016d3
#   B.ASSEMBLE RegSrc=24          -> 0x800c10d3

.text
# --- legal B.SUBVIEW words ---
.byte 0xd3, 0x00, 0x00, 0x00
.byte 0x53, 0x06, 0xf1, 0xff
.byte 0xd3, 0x82, 0x4b, 0x06
# --- illegal B.SUBVIEW words ---
.byte 0xd3, 0x06, 0x00, 0x00
.byte 0x53, 0x00, 0x00, 0x00
.byte 0xd3, 0x00, 0x0c, 0x00
# --- legal B.ASSEMBLE words ---
.byte 0xd3, 0x10, 0x00, 0x80
.byte 0x53, 0x1e, 0xf1, 0xff
.byte 0x53, 0x90, 0x1b, 0x00
# --- illegal B.ASSEMBLE words ---
.byte 0x53, 0x10, 0x00, 0x80
.byte 0x53, 0x1e, 0x01, 0x00
.byte 0xd3, 0x16, 0x00, 0x80
.byte 0xd3, 0x10, 0x0c, 0x80

# CHECK: B.SUBVIEW{{.*}}0, zero, 0, 1
# CHECK: B.SUBVIEW{{.*}}1, a0, 2047, 12
# CHECK: B.SUBVIEW{{.*}}0, x3, 100, 5
# CHECK: <unknown>
# CHECK: <unknown>
# CHECK: <unknown>
# CHECK: B.ASSEMBLE{{.*}}1, 0, zero, 0, 1
# CHECK: B.ASSEMBLE{{.*}}1, 1, a0, 2047, 12
# CHECK: B.ASSEMBLE{{.*}}0, 0, x3, 1, 0
# CHECK: <unknown>
# CHECK: <unknown>
# CHECK: <unknown>
# CHECK: <unknown>