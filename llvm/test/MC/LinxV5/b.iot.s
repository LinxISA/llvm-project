// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --check-prefix=DIS
// RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC

// Source lifetime is independent from the B.IOT `last` sequence flag.
// A bare source is last-use; `.reuse` retains the source vtag value.

// ENC: B.IOT t#1, mask=1111{{.*}}encoding: [0x13,0xd0,0x07,0x04]
// DIS: B.IOT t#1, mask=1111
B.IOT t#1

// The old one-source encoding remains retain and prints explicitly.
// ENC: B.IOT t#1.reuse, mask=1111{{.*}}encoding: [0x13,0xd0,0x07,0x00]
// DIS: B.IOT t#1.reuse, mask=1111
B.IOT t#1.reuse

// ENC: B.IOT t#1, u#1, mask=1111{{.*}}encoding: [0x13,0xa0,0x07,0x40]
// DIS: B.IOT t#1, u#1, mask=1111
B.IOT t#1, u#1

// ENC: B.IOT t#1.reuse, u#1, mask=1111{{.*}}encoding: [0x13,0xb0,0x07,0x40]
// DIS: B.IOT t#1.reuse, u#1, mask=1111
B.IOT t#1.reuse, u#1

// ENC: B.IOT t#1, u#1.reuse, mask=1111{{.*}}encoding: [0x13,0xf0,0x07,0x40]
// DIS: B.IOT t#1, u#1.reuse, mask=1111
B.IOT t#1, u#1.reuse

// Legacy Func=100 remains retain/retain and disassembles explicitly.
// ENC: B.IOT t#1.reuse, u#1.reuse, mask=1111{{.*}}encoding: [0x13,0xc0,0x07,0x40]
// DIS: B.IOT t#1.reuse, u#1.reuse, mask=1111
B.IOT t#1.reuse, u#1.reuse
