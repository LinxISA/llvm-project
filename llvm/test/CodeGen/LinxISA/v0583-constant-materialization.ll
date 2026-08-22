; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define i64 @signed_low32() {
  ret i64 305419896
}

; CHECK-LABEL: signed_low32:
; CHECK: lui 74565,
; CHECK: addi {{.*}}, 1656,
; CHECK-NOT: hl.lui

define i32 @signed_i32() {
  ret i32 -19088743
}

; CHECK-LABEL: signed_i32:
; CHECK: lui -4661,
; CHECK: addi {{.*}}, 2713,
; CHECK-NOT: hl.lui

define i64 @full_i64() {
  ret i64 1311768467463790320
}

; CHECK-LABEL: full_i64:
; CHECK: lui -414771,
; CHECK: addi {{.*}}, 3824,
; CHECK: hl.lui 305419896,
