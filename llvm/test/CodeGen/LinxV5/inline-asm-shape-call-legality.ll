; RUN: opt -mtriple=linx64v5 -passes=linx-v5-inline-asm-constant-prepare -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s --check-prefix=OPT
; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each %s | llc -mtriple=linx64v5 -filetype=obj -o /dev/null

target triple = "linx64v5"

; A weak body can be replaced by a strong definition at link time. Keep the
; call live even though the asm immediate can be folded without its result.
define weak i64 @replaceable_shape() {
  ret i64 32
}
; CHECK-LABEL: define i64 @interposable_call(
; CHECK: %shape = call i64 @replaceable_shape()
; CHECK: ret i64 %shape
; OPT-LABEL: define i64 @interposable_call(
; OPT: %shape = {{.*}}call i64 @replaceable_shape()
; OPT: call void asm sideeffect "", "i"(i64 0)
; OPT: ret i64 %shape
define i64 @interposable_call() {
  %shape = call i64 @replaceable_shape()
  %imm = and i64 %shape, 0
  call void asm sideeffect "", "i"(i64 %imm)
  ret i64 %shape
}

; Body viability is insufficient when caller/callee attributes disagree.
; Check the early pass: later interprocedural constant propagation can
; legitimately replace this pure call without inlining its body.
define internal i64 @nullable_shape() null_pointer_is_valid {
  ret i64 64
}
; CHECK-LABEL: define i64 @incompatible_attributes(
; CHECK: %shape = call i64 @nullable_shape()
; CHECK: ret i64 %shape
define i64 @incompatible_attributes() {
  %shape = call i64 @nullable_shape()
  %imm = and i64 %shape, 0
  call void asm sideeffect "", "i"(i64 %imm)
  ret i64 %shape
}
