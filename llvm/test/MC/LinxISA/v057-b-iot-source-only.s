# RUN: llvm-mc -triple=linx64 -filetype=obj %s -o %t
# RUN: llvm-objdump -d %t | FileCheck %s
# RUN: llvm-mc -triple=linx64 -show-encoding %s | FileCheck %s --check-prefix=ENC

# v0.57 source-only B.IOT forms use DstTile=111 and imm4=0. They preserve
# source order across repeated descriptors without allocating a destination.

B.IOT t#1, u#2
B.IOT m#3.reuse, last

# CHECK: B.IOT{{[ 	]+}}t#1, u#2
# CHECK: B.IOT{{[ 	]+}}m#3.reuse, last
# ENC: B.IOT t#1, u#2{{.*}}encoding: [0x13,0x40,0xd1,0x01]
# ENC: B.IOT m#3.reuse, last{{.*}}encoding: [0x13,0xd1,0xc0,0x61]
