; RUN: llc -mtriple=linx64 -O0 < %s | FileCheck %s

; The Linx GPR encoding space includes queue pseudo-registers (t#k/u#k and the
; special RegDst encodings for ->t/->u). These are not valid "general-purpose"
; registers for scalar C/LLVM inline asm operands. Ensure the generic "r"
; constraint maps scalar values to architectural GPRs only; vector values use
; the tile-register path covered by inlineasm-tile-r-constraint.ll.

define i64 @inlineasm_r() {
entry:
  %out = call i64 asm sideeffect "# OUT=$0", "=r"()
  ret i64 %out
}

; CHECK-LABEL: inlineasm_r:
; CHECK: # OUT=
; CHECK-NOT: # OUT=t#
; CHECK-NOT: # OUT=u#
