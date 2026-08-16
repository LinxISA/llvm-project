; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx32 -O2 < %s | FileCheck %s

; Tile post-processing attributes pass boolean template parameters through the
; generic immediate constraint.  A true i1 must remain the lexical value 1;
; treating it as a signed one-bit integer produces -1 and used to assert while
; constructing the target constant.

define void @inlineasm_bool_immediate() {
entry:
  call void asm sideeffect "# bool ${0:c}", "i"(i1 true)
  ret void
}

; CHECK-LABEL: inlineasm_bool_immediate:
; CHECK: # bool 1
