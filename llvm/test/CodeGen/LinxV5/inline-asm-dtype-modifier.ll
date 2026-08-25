; RUN: llc -mtriple=linx64v5 -O0 < %s | FileCheck %s

define void @dtype_fp32() {
; CHECK-LABEL: dtype_fp32:
; CHECK: BSTART.TLSU{{[ \t]+}}TLOAD, FP32
  call void asm sideeffect "BSTART.TLSU TLOAD, ${0:D}\0A", "i"(i32 1)
  ret void
}

define void @dtype_bf16() {
; CHECK-LABEL: dtype_bf16:
; CHECK: BSTART.TLSU{{[ \t]+}}TLOAD, BF16
  call void asm sideeffect "BSTART.TLSU TLOAD, ${0:D}\0A", "i"(i32 5)
  ret void
}
