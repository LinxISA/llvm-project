# RUN: true
# Raw B.IOS words for ADR-0097 SharedTileID validation.

# S0.reuse, mask=1111, bit 27 clear and bit 26 retain.
.byte 0x13, 0x1e, 0x00, 0x00
# S0, mask=1111, bit 26 last-use.
.byte 0x13, 0x1e, 0x00, 0x04
# Reserved bit 27 set.
.byte 0x13, 0x1e, 0x00, 0x08
