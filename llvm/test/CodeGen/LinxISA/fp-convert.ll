; RUN: llc -mtriple=linx64 -O0 < %s | FileCheck %s
; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

@fmt = private unnamed_addr constant [8 x i8] c"%llx %f\00"

declare i32 @printf(ptr, ...)

define double @promote_float_arg(float %x) {
entry:
  %r = fpext float %x to double
  ret double %r
}

define float @round_double_arg(double %x) {
entry:
  %r = fptrunc double %x to float
  ret float %r
}

define i32 @print_float_vararg(float %x) {
entry:
  %d = fpext float %x to double
  %bits = bitcast double %d to i64
  %r = call i32 (ptr, ...) @printf(ptr @fmt, i64 %bits, double %d)
  ret i32 %r
}

; CHECK-LABEL: promote_float_arg:
; CHECK: fcvt.fs2fd
; CHECK-LABEL: round_double_arg:
; CHECK: fcvt.fd2fs
; CHECK-LABEL: print_float_vararg:
; CHECK: fcvt.fs2fd
