; RUN: llc -mtriple=linx64-unknown-linux-musl -o - %s | FileCheck %s

define i32 @f(i64 %n) nounwind {
entry:
  %p = alloca i8, i64 %n, align 16
  %q = ptrtoint ptr %p to i64
  %r = trunc i64 %q to i32
  ret i32 %r
}

; CHECK-LABEL: f:
; CHECK: FENTRY
; CHECK: c.movr	sp,	->s7
; CHECK: addi
; CHECK: sub	sp,
; CHECK: ->sp
; CHECK: c.movr	s7,	->sp
; CHECK: FRET.STK
