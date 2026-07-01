; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

@fmt = private unnamed_addr constant [8 x i8] c"%llx %f\00"

declare i32 @printf(ptr, ...)

define i32 @inlineasm_boundary_before_vararg_call(double %x) {
entry:
  call void asm sideeffect "acrc 1\0Ac.bstop\0AC.BSTART", "~{a0},~{a1},~{a2},~{a7},~{x0},~{x1},~{x2},~{x3},~{memory}"()
  %bits = bitcast double %x to i64
  %r = call i32 (ptr, ...) @printf(ptr @fmt, i64 %bits, double %x)
  ret i32 %r
}

; CHECK-LABEL: inlineasm_boundary_before_vararg_call:
; CHECK-NOT: BSTART{{[[:space:]]+}}CALL, printf
; CHECK: {{# APP}}
; CHECK: acrc
; CHECK: C.BSTOP
; CHECK: C.BSTART
; CHECK: {{# NO_APP}}
; CHECK: BSTART{{[[:space:]]+}}CALL, printf{{(@plt)?}}, ra=
