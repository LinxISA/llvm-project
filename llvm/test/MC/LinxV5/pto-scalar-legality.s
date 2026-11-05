# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s --check-prefix=ENC
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=DIS

# Raw immediate units are halfwords, including odd values.
c.setret 0, ->ra
# ENC: c.setret 0, ->ra{{.*}}encoding: [0x16,0x50]
c.setret 1, ->ra
# ENC: c.setret 1, ->ra{{.*}}encoding: [0x56,0x50]
c.setret 30, ->ra
# ENC: c.setret 30, ->ra{{.*}}encoding: [0x96,0x57]
c.setret 31, ->ra
# ENC: c.setret 31, ->ra{{.*}}encoding: [0xd6,0x57]
# DIS: c.setret 0, ->ra
# DIS: c.setret 1, ->ra
# DIS: c.setret 30, ->ra
# DIS: c.setret 31, ->ra

acre 0
# ENC: acre 0{{.*}}encoding: [0x2b,0x30,0x00,0x01]
acre 1
# ENC: acre 1{{.*}}encoding: [0x2b,0x30,0x10,0x01]
# DIS: acre 0
# DIS: acre 1
fence.d 0, 0
# ENC: fence.d 0, 0{{.*}}encoding: [0x2b,0x20,0x00,0x00]
fence.d 1, 2
# ENC: fence.d 1, 2{{.*}}encoding: [0x2b,0x20,0x20,0x01]
fence.d r, w
# ENC: fence.d 1, 2{{.*}}encoding: [0x2b,0x20,0x20,0x01]
fence.d 15, 15
# ENC: fence.d 15, 15{{.*}}encoding: [0x2b,0x20,0xf0,0x0f]
# DIS: fence.d 0, 0
# DIS: fence.d 1, 2
# DIS: fence.d 1, 2
# DIS: fence.d 15, 15
