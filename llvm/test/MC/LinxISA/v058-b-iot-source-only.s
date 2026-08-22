# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s
# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC

# PTO 0.58.3 source-only B.IOT forms omit DstTile/SizeCode entirely. They preserve
# source order across repeated descriptors without allocating a destination.

B.IOT t#1, u#2, mask=1100
B.IOT m#3, mask=1100, last

# CHECK: B.IOT{{[ 	]+}}t#1, u#2, mask=1100
# CHECK: B.IOT{{[ 	]+}}m#3, mask=1100, last
# ENC: B.IOT{{[ 	]+}}t#1, u#2, mask=1100{{.*}}encoding: [0x13,0x4a,0x00,0x44]
# ENC: B.IOT{{[ 	]+}}m#3, mask=1100, last{{.*}}encoding: [0x13,0x5a,0x28,0x02]
