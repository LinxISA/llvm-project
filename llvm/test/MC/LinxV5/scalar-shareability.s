# RUN: llvm-mc -triple=linx64v5 -show-encoding %s | FileCheck %s
# RUN: llvm-mc -triple=linx64v5 -filetype=obj %s -o %t.o
# RUN: llvm-objdump -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=DIS
# RUN: %python %S/../../../utils/linx/verify-scoped-scalar.py
c.ldi.intra [a0, 0], ->t
# CHECK: encoding: [0x1f,0x01,0x9a,0x00,0x00,0x00,0x00,0x00]
# DIS: c.ldi.intra [a0, 0], ->t
ldi.inter [a0, 0], ->a1
# CHECK: encoding: [0x1f,0x06,0x99,0x31,0x01,0x00,0x00,0x00]
# DIS: ldi.inter [a0, 0], ->a1
hl.ldi.private [a0, 0], ->a1
# CHECK: encoding: [0x1f,0x08,0x0e,0x00,0x99,0x31,0x01,0x00]
# DIS: hl.ldi.private [a0, 0], ->a1
casd.aqrl.intra [a0], a1, a2, ->a3
# CHECK: encoding: [0x1f,0x05,0x9b,0x32,0x31,0x26,0x00,0x00]
# DIS: casd.aqrl.intra [a0], a1, a2, ->a3
# Explicit private must retain an envelope; legacy default remains unchanged.
ldi.private [a0, 0], ->a1
# CHECK: encoding: [0x1f,0x04,0x99,0x31,0x01,0x00,0x00,0x00]
ldi [a0, 0], ->a1
# CHECK: encoding: [0x99,0x31,0x01,0x00]
