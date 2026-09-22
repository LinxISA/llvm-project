// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC

// Source lifetime is independent from the B.IOT `last` sequence flag: a bare
// source is the last-use consumer for the participating PEs and `.reuse`
// retains the source vtag value. Every pre-change word stays retain/retain.

// ENC: encoding: [0x13,0xee,0x09,0x00]
// DIS: B.IOT mask=1111, last, ->t<512B>
B.IOT mask=1111, last, ->t<512B>

// ENC: encoding: [0x13,0xee,0x01,0x00]
// DIS: B.IOT mask=1111, ->t<512B>
B.IOT mask=1111, ->t<512B>

// One-source Func=101: bit 26 one is last-use, zero keeps the source.
// ENC: encoding: [0x13,0x5e,0x00,0x04]
// DIS: B.IOT t#1, mask=1111
B.IOT t#1, mask=1111

// ENC: encoding: [0x13,0x5e,0x00,0x00]
// DIS: B.IOT t#1.reuse, mask=1111
B.IOT t#1.reuse, mask=1111

// ENC: encoding: [0x13,0x5e,0x08,0x05]
// DIS: B.IOT u#1, mask=1111, last
B.IOT u#1, mask=1111, last

// ENC: encoding: [0x13,0x5e,0x08,0x01]
// DIS: B.IOT u#1.reuse, mask=1111, last
B.IOT u#1.reuse, mask=1111, last

// ENC: encoding: [0x93,0xde,0x1c,0x06]
// DIS: B.IOT m#2, mask=1111, last, ->u<32KB>
B.IOT m#2, mask=1111, last, ->u<32KB>

// Destination forms carry the source after DstTile, mask, SizeCode and last
// in the MC operand list. Pin the reuse selection for every such shape.
// ENC: encoding: [0x93,0xde,0x1c,0x02]
// DIS: B.IOT m#2.reuse, mask=1111, last, ->u<32KB>
B.IOT m#2.reuse, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xdf,0x34,0x07]
// DIS: B.IOT n#4, mask=1111, ->n<32KB>
B.IOT n#4, mask=1111, ->n<32KB>

// Two-source Func 010/011/111/100 carry the four lifetime combinations.
// ENC: encoding: [0x13,0x2e,0x00,0x40]
// DIS: B.IOT t#1, u#1, mask=1111
B.IOT t#1, u#1, mask=1111

// ENC: encoding: [0x13,0x3e,0x00,0x40]
// DIS: B.IOT t#1.reuse, u#1, mask=1111
B.IOT t#1.reuse, u#1, mask=1111

// ENC: encoding: [0x13,0x7e,0x00,0x40]
// DIS: B.IOT t#1, u#1.reuse, mask=1111
B.IOT t#1, u#1.reuse, mask=1111

// The historical two-source word is retain/retain and prints explicitly.
// ENC: encoding: [0x13,0x4e,0x00,0x40]
// DIS: B.IOT t#1.reuse, u#1.reuse, mask=1111
B.IOT t#1.reuse, u#1.reuse, mask=1111

// ENC: encoding: [0x13,0x2e,0x08,0x45]
// DIS: B.IOT u#1, u#2, mask=1111, last
B.IOT u#1, u#2, mask=1111, last

// ENC: encoding: [0x93,0xae,0x1c,0x5a]
// DIS: B.IOT m#2, u#7, mask=1111, last, ->u<32KB>
B.IOT m#2, u#7, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xbe,0x1c,0x5a]
// DIS: B.IOT m#2.reuse, u#7, mask=1111, last, ->u<32KB>
B.IOT m#2.reuse, u#7, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xfe,0x1c,0x5a]
// DIS: B.IOT m#2, u#7.reuse, mask=1111, last, ->u<32KB>
B.IOT m#2, u#7.reuse, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xce,0x1c,0x5a]
// DIS: B.IOT m#2.reuse, u#7.reuse, mask=1111, last, ->u<32KB>
B.IOT m#2.reuse, u#7.reuse, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xaf,0x34,0xc3]
// DIS: B.IOT n#4, n#1, mask=1111, ->n<32KB>
B.IOT n#4, n#1, mask=1111, ->n<32KB>

// Absolute physical Tile register spellings are emitted when a long-lived
// inline-asm Tile operand remains in Tile_ABS after register allocation. They
// carry the same six-bit source encoding as their output-stack counterparts.
// ENC: encoding: [0x13,0x5e,0x00,0x04]
// DIS: B.IOT t#1, mask=1111
B.IOT tile_t1, mask=1111

// ENC: encoding: [0x13,0x5e,0x08,0x05]
// DIS: B.IOT u#1, mask=1111, last
B.IOT tile_u1, mask=1111, last

// ENC: encoding: [0x93,0xde,0x1c,0x06]
// DIS: B.IOT m#2, mask=1111, last, ->u<32KB>
B.IOT tile_m2, mask=1111, last, ->u<32KB>

// ENC: encoding: [0x93,0xaf,0x34,0xc3]
// DIS: B.IOT n#4, n#1, mask=1111, ->n<32KB>
B.IOT tile_n4, tile_n1, mask=1111, ->n<32KB>

// Absolute register spellings retain the same lifetime marker.
// ENC: encoding: [0x13,0x5e,0x00,0x00]
// DIS: B.IOT t#1.reuse, mask=1111
B.IOT tile_t1.reuse, mask=1111
