// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s

// Binary ALU: 00=.sw, 01=.uw, 10=.neg/.not, 11=unmodified.
// CHECK: add a0, a1, ->a2
// CHECK: encoding: [0x05,0x02,0x31,0x06]
add a0, a1, ->a2
// CHECK: add a0, a1.sw, ->a2
// CHECK: encoding: [0x05,0x02,0x31,0x00]
add a0, a1.sw, ->a2
// CHECK: add a0, a1.uw, ->a2
// CHECK: encoding: [0x05,0x02,0x31,0x02]
add a0, a1.uw, ->a2
// CHECK: add a0, a1.neg, ->a2
// CHECK: encoding: [0x05,0x02,0x31,0x04]
add a0, a1.neg, ->a2

// CHECK: xor a3, a4, ->a5
// CHECK: encoding: [0x85,0xc3,0x62,0x06]
xor a3, a4, ->a5
// CHECK: xor a3, a4.sw, ->a5
// CHECK: encoding: [0x85,0xc3,0x62,0x00]
xor a3, a4.sw, ->a5
// CHECK: xor a3, a4.uw, ->a5
// CHECK: encoding: [0x85,0xc3,0x62,0x02]
xor a3, a4.uw, ->a5
// CHECK: xor a3, a4.not, ->a5
// CHECK: encoding: [0x85,0xc3,0x62,0x04]
xor a3, a4.not, ->a5

// Address, comparison and select families retain their own mappings.
// CHECK: ld [a0, a1], ->a2
// CHECK: encoding: [0x09,0x32,0x31,0x00]
ld [a0, a1], ->a2
// CHECK: ld [a0, a1.sw], ->a2
// CHECK: encoding: [0x09,0x32,0x31,0x02]
ld [a0, a1.sw], ->a2
// CHECK: ld [a0, a1.uw], ->a2
// CHECK: encoding: [0x09,0x32,0x31,0x04]
ld [a0, a1.uw], ->a2
// CHECK: cmp.eq a0, a1, ->a2
// CHECK: encoding: [0x45,0x02,0x31,0x00]
cmp.eq a0, a1, ->a2
// CHECK: cmp.eq a0, a1.sw, ->a2
// CHECK: encoding: [0x45,0x02,0x31,0x02]
cmp.eq a0, a1.sw, ->a2
// CHECK: cmp.eq a0, a1.uw, ->a2
// CHECK: encoding: [0x45,0x02,0x31,0x04]
cmp.eq a0, a1.uw, ->a2
// CHECK: csel a0, a1, a2, ->a3
// CHECK: encoding: [0xf7,0x82,0x41,0x10]
csel a0, a1, a2, ->a3
// CHECK: csel a0, a1, a2.neg, ->a3
// CHECK: encoding: [0xf7,0x82,0x41,0x16]
csel a0, a1, a2.neg, ->a3
