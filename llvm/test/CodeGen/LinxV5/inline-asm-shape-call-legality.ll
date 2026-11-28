; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s --check-prefix=OPT
; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each %s | llc -mtriple=linx64v5 -filetype=obj -o /dev/null

target triple = "linx64v5"

; A weak body can be replaced by a strong definition at link time. Keep the
; call live even though the asm immediate can be folded without its result.
define weak i64 @replaceable_shape() {
  ret i64 32
}
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

; Ordinary interprocedural constant propagation may fold a pure call even
; when caller/callee attributes prevent inlining its body.
define internal i64 @nullable_shape() null_pointer_is_valid {
  ret i64 64
}
; OPT-LABEL: define i64 @incompatible_attributes(
; OPT: call void asm sideeffect "", "i"(i64 0)
; OPT: ret i64 64
define i64 @incompatible_attributes() {
  %shape = call i64 @nullable_shape()
  %imm = and i64 %shape, 0
  call void asm sideeffect "", "i"(i64 %imm)
  ret i64 %shape
}
