// RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s

// Every B.IOT source carries its own lifetime decision. Two-source forms
// encode {Src0Reuse,Src1Reuse} in Func as 010=last/last, 011=reuse/last,
// 111=last/reuse and 100=reuse/reuse; one-source Func=101 uses bit 26
// (0=reuse, 1=last-use). The decoder must keep the encoded source order and
// report the PE mask, TSize, last flag and destination fields positionally.
.text

// CHECK: B.IOT t#2, u#2, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xaf, 0x1a, 0x44

// CHECK: B.IOT t#2.reuse, u#2, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xbf, 0x1a, 0x44

// CHECK: B.IOT t#2, u#2.reuse, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xff, 0x1a, 0x44

// CHECK: B.IOT t#2.reuse, u#2.reuse, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xcf, 0x1a, 0x44

// CHECK: B.IOT t#2.reuse, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xdf, 0x1a, 0x00

// CHECK: B.IOT t#2, mask=0101, TSize=7, last, ->m
.byte 0x13, 0xdf, 0x1a, 0x04
