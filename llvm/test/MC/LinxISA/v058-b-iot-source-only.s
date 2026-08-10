# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s
# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC

# PTO 0.58 source-only B.IOT forms omit DstTile/TSize entirely. They preserve
# source order across repeated descriptors without allocating a destination.

B.IOT t#1, u#2, mask=0011
B.IOT m#3, mask=1100, last

# CHECK: B.IOT{{[ 	]+}}t#1, u#2, mask=0011
# CHECK: B.IOT{{[ 	]+}}m#3, mask=1100, last
# ENC: B.IOT{{[ 	]+}}t#1, u#2, mask=0011{{.*}}encoding: [0x13,0xc0,0x01,0x44]
# ENC: B.IOT{{[ 	]+}}m#3, mask=1100, last{{.*}}encoding: [0x13,0x50,0x2e,0x02]
