// RUN: llvm-mc -triple=linx64v5 %s | FileCheck %s

BSTART.TEPL 45, U8
B.DATR Zero, LT, RNONE, sat
B.DIM zero, 1, ->lb0
B.DIM zero, 32, ->lb1
B.DIM zero, 2, ->lb2
B.IOT T#1, mask=1111, last
B.IOR [a0], a1

// CHECK: B.DATR Zero, LT, RNONE, sat
