// RUN: llvm-mc %s --triple=linx64v5 --show-encoding | FileCheck %s --dump-input always -vv

// CHECK: hl.sb.pr a0, [a1, t#3.uw], ->u
// CHECK: hl.sb.po a0, [a1, t#3.uw], ->u
// CHECK: hl.sw.upr a0, [a1, t#3.uw], ->u
hl.sb.pr a0, [a1, t#3.uw], ->u
hl.sb.po a0, [a1, t#3.uw], ->u
hl.sw.upr a0, [a1, t#3.uw], ->u

// CHECK: hl.cmp.lti u#1, -2048, ->u
// CHECK: hl.cmp.nei u#1, -2048, ->u
// CHECK: hl.cmp.gei u#1, -2048, ->u
hl.cmp.lti u#1, -2048, ->u
hl.cmp.nei u#1, -2048, ->u
hl.cmp.gei u#1, -2048, ->u