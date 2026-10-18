# RUN: true
# Raw B.IOS words for ADR-0097 SharedTileID validation.

# S0 retained, mask=1111, source last-use bit 26 clear.
.byte 0x13, 0x1e, 0x00, 0x00
# S0 consumed, mask=1111, source last-use bit 26 set.
.byte 0x13, 0x1e, 0x00, 0x04
