// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS

// Generic PTO SrcRType: 00=.sw, 01=.uw, 10=.neg/.not, 11=no modifier.
// CSEL is an exception: 00/01/10 are unmodified aliases and 11 is .neg.

// ENC: add a1, a2, ->a0                    // encoding: [0x05,0x81,0x41,0x06]
// ENC: add a1, a2.sw, ->a0                // encoding: [0x05,0x81,0x41,0x00]
// ENC: add a1, a2.uw, ->a0                // encoding: [0x05,0x81,0x41,0x02]
// ENC: add a1, a2.neg, ->a0               // encoding: [0x05,0x81,0x41,0x04]
// ENC: or a1, a2, ->a0                    // encoding: [0x05,0xb1,0x41,0x06]
// ENC: or a1, a2.sw, ->a0                 // encoding: [0x05,0xb1,0x41,0x00]
// ENC: or a1, a2.uw, ->a0                 // encoding: [0x05,0xb1,0x41,0x02]
// ENC: or a1, a2.not, ->a0                // encoding: [0x05,0xb1,0x41,0x04]
// ENC: csel a0, a1, a2, ->a3              // encoding: [0xf7,0x82,0x41,0x10]
// ENC: csel a0, a1, a2.neg, ->a3         // encoding: [0xf7,0x82,0x41,0x16]

add a1, a2, ->a0
add a1, a2.sw, ->a0
add a1, a2.uw, ->a0
add a1, a2.neg, ->a0
or a1, a2, ->a0
or a1, a2.sw, ->a0
or a1, a2.uw, ->a0
or a1, a2.not, ->a0
csel a0, a1, a2, ->a3
csel a0, a1, a2.neg, ->a3

// DIS: {{.*}}add a1, a2, ->a0
// DIS: {{.*}}add a1, a2.sw, ->a0
// DIS: {{.*}}add a1, a2.uw, ->a0
// DIS: {{.*}}add a1, a2.neg, ->a0
// DIS: {{.*}}or a1, a2, ->a0
// DIS: {{.*}}or a1, a2.sw, ->a0
// DIS: {{.*}}or a1, a2.uw, ->a0
// DIS: {{.*}}or a1, a2.not, ->a0
// DIS: {{.*}}csel a0, a1, a2, ->a3
// DIS: {{.*}}csel a0, a1, a2.neg, ->a3
