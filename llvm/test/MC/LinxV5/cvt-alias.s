// RUN: llvm-mc %s --triple=linx64 --show-encoding | FileCheck %s --dump-input always -vv

// CHECK: v.fcvt.e5m22e4m3
v.cvt.e5m22e4m3 vt#1.fb, ->vt.b

// CHECK: v.icvtf.s642e4m3
v.cvt.s642e4m3 vt#1.sd, ->vt.b