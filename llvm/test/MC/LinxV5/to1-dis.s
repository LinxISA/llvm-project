// RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
// RUN: llvm-objdump -d %t | FileCheck %s --dump-input always -vv

// CHECK: v.lwi.local [to3, lc0<<2, 1024], ->vt.w
v.lwi.local [to3, lc0<<2, 1024], ->vt.w