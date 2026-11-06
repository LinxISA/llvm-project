# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s | llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros - | FileCheck %s --check-prefix=DIS
B.IOR [a0, zero, a2], []
# CHECK: B.IOR{{.*}}[a0, zero, a2], []{{.*}}encoding: [0x13,0x00,0x01,0x20]
# DIS: B.IOR{{.*}}[a0, zero, a2], []
B.IOR [zero, zero, a2], []
# CHECK: B.IOR{{.*}}[zero, zero, a2], []{{.*}}encoding: [0x13,0x00,0x00,0x20]
# DIS: B.IOR{{.*}}[zero, zero, a2], []
