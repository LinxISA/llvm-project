// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s

// PTO-ISA v0.58.4 permits an explicit PE mask without LAST for a
// source-only local B.IOT binding.
B.IOT t#1, mask=1111
// CHECK: B.IOT t#1, mask=1111
// CHECK-SAME: encoding: [0x13,0x5e,0x00,0x00]
