// RUN: llvm-mc %s --triple=linx64 --show-encoding | FileCheck %s --dump-input always -vv

// CHECK: setret .LA, ->ra
// CHECK: encoding: [0x07,0x05,0bAAAA0000,A]
// CHECK: fixup A - offset: 0, value: .LA, kind: fixup_linxv5_addpc
setret .LA, ->ra

// CHECK: setret .LA, ->ra
// CHECK: encoding: [0x07,0x05,0bAAAA0000,A]
// CHECK: fixup A - offset: 0, value: .LA, kind: fixup_linxv5_addpc
setret .LA

// CHECK: c.add a0, a1, ->t
// CHECK: encoding: [0x88,0x18]
c.add a0, a1, ->t

// CHECK: c.ldi [t#1, 8], ->t
// CHECK: encoding: [0x1a,0x0e]
c.ldi [t#1, 8], ->t

// CHECK: c.sext.b a0, ->t
// CHECK: encoding: [0x9c,0x40]
c.sext.b a0, ->t

// CHECK: c.swi t#1, [t#1, 0]
// CHECK: encoding: [0x2a,0x06]
c.swi t#1, [t#1, 0]

// CHECK: c.addi t#1, 3, ->t
// CHECK: encoding: [0x0c,0x1e]
c.addi  t#1, 3, ->t

// CHECK: c.cmp.eqi t#1, 3, ->t
// CHECK: encoding: [0xec,0x00]
c.cmp.eqi t#1, 3, ->t
.LA: