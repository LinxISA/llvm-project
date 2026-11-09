; RUN: llc -mtriple=linx64v5 -linxv5-emit-pto-isa-identity -filetype=obj %s -o %t.o
; RUN: llvm-readobj --notes %t.o | FileCheck %s
; RUN: %python %S/../../../utils/linxv5/verify_pto_identity_object.py %t.o
; RUN: llc -mtriple=linx64v5 -linxv5-emit-pto-isa-identity %s -o %t.s
; RUN: llvm-mc -triple=linx64v5 -linxv5-emit-pto-isa-identity -filetype=obj %t.s -o %t.asm.o
; RUN: llvm-readobj --notes %t.asm.o | FileCheck %s
; CHECK: Owner: PTO
; CHECK: Data size: 0xA5
define void @identity_probe() {
  ret void
}
