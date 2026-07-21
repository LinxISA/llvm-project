; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define void @copy_f16(ptr %dst, ptr %src) {
entry:
  %value = load half, ptr %src, align 2
  store half %value, ptr %dst, align 2
  ret void
}

define void @copy_bf16(ptr %dst, ptr %src) {
entry:
  %value = load bfloat, ptr %src, align 2
  store bfloat %value, ptr %dst, align 2
  ret void
}

; CHECK-LABEL: copy_f16:
; CHECK: lhi
; CHECK: shi
; CHECK-LABEL: copy_bf16:
; CHECK: lhi
; CHECK: shi
