; RUN: llc -mtriple=linx64v5 -mattr=+relax -enable-all-vector-as-tilereg=true \
; RUN:     < %s | FileCheck %s

;; Issue #77: adjacent scalar stores merged into sub-tile-size vector stores
;; (v2i64/v4i64) must keep scalar lowering. Such sizes have no B.IOT SizeCode
;; (minimum 128 B), so they must not become BLK_TSTORE and must not crash
;; instruction selection.

define i32 @merged_zero_stores(i32 %argc) {
; CHECK-LABEL: merged_zero_stores:
; CHECK-NOT: TSTORE
entry:
  %conv = zext i32 %argc to i64
  %shl = shl i64 %conv, 4
  %and = and i64 %shl, 17179869168
  %base = inttoptr i64 %and to ptr
  store <2 x i64> zeroinitializer, ptr %base, align 16
  store <2 x i64> zeroinitializer, ptr %base, align 16
  %loaded = load i32, ptr %base, align 16
  ret i32 %loaded
}
