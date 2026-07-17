; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define i64 @subi_i64_1(i64 %x) {
  %r = add i64 %x, -1
  ret i64 %r
}
; CHECK-LABEL: subi_i64_1:
; CHECK-NOT: lui
; CHECK: subi a0, 1,{{[[:space:]]+}}->a0

define i64 @subi_i64_4095(i64 %x) {
  %r = add i64 %x, -4095
  ret i64 %r
}
; CHECK-LABEL: subi_i64_4095:
; CHECK-NOT: lui
; CHECK: subi a0, 4095,{{[[:space:]]+}}->a0

define i64 @hl_subi_i64_4096(i64 %x) {
  %r = add i64 %x, -4096
  ret i64 %r
}
; CHECK-LABEL: hl_subi_i64_4096:
; CHECK-NOT: lui
; CHECK: hl.subi a0, 4096,{{[[:space:]]+}}->a0

define i64 @hl_subi_i64_ffffff(i64 %x) {
  %r = add i64 %x, -16777215
  ret i64 %r
}
; CHECK-LABEL: hl_subi_i64_ffffff:
; CHECK-NOT: lui
; CHECK: hl.subi a0, 16777215,{{[[:space:]]+}}->a0

define i32 @subiw_i32_1(i32 %x) {
  %r = add i32 %x, -1
  ret i32 %r
}
; CHECK-LABEL: subiw_i32_1:
; CHECK-NOT: lui
; CHECK: subiw a0, 1,{{[[:space:]]+}}->a0

define i32 @subiw_i32_4095(i32 %x) {
  %r = add i32 %x, -4095
  ret i32 %r
}
; CHECK-LABEL: subiw_i32_4095:
; CHECK-NOT: lui
; CHECK: subiw a0, 4095,{{[[:space:]]+}}->a0

define i32 @hl_subiw_i32_4096(i32 %x) {
  %r = add i32 %x, -4096
  ret i32 %r
}
; CHECK-LABEL: hl_subiw_i32_4096:
; CHECK-NOT: lui
; CHECK: hl.subiw a0, 4096,{{[[:space:]]+}}->a0

define i32 @hl_subiw_i32_ffffff(i32 %x) {
  %r = add i32 %x, -16777215
  ret i32 %r
}
; CHECK-LABEL: hl_subiw_i32_ffffff:
; CHECK-NOT: lui
; CHECK: hl.subiw a0, 16777215,{{[[:space:]]+}}->a0

define i64 @zero_i64(i64 %x) {
  %r = add i64 %x, 0
  ret i64 %r
}
; CHECK-LABEL: zero_i64:
; CHECK-NOT: subi
; CHECK: FRET.STK

define i32 @zero_i32(i32 %x) {
  %r = add i32 %x, 0
  ret i32 %r
}
; CHECK-LABEL: zero_i32:
; CHECK-NOT: subiw
; CHECK: FRET.STK

define i64 @overflow_i64(i64 %x) {
  %r = add i64 %x, -16777216
  ret i64 %r
}
; CHECK-LABEL: overflow_i64:
; CHECK-NOT: hl.subi
; CHECK: add

define i32 @overflow_i32(i32 %x) {
  %r = add i32 %x, -16777216
  ret i32 %r
}
; CHECK-LABEL: overflow_i32:
; CHECK-NOT: hl.subiw
; CHECK: addw

define i64 @min_i64(i64 %x) {
  %r = add i64 %x, -9223372036854775808
  ret i64 %r
}
; CHECK-LABEL: min_i64:
; CHECK-NOT: subi
; CHECK-NOT: hl.subi
; CHECK: add

define i32 @min_i32(i32 %x) {
  %r = add i32 %x, -2147483648
  ret i32 %r
}
; CHECK-LABEL: min_i32:
; CHECK-NOT: subiw
; CHECK-NOT: hl.subiw
; CHECK: addw
