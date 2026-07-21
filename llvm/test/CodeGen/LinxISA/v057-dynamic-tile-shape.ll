; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare <1024 x i32> @llvm.linx.tma.tload.shape.v1024i32(
    ptr, i32 immarg, i32 immarg, i64, i64, i64, i32 immarg, i64)
declare void @llvm.linx.tma.tstore.shape.v1024i32(
    ptr, <1024 x i32>, i32 immarg, i32 immarg, i64, i64, i64,
    i32 immarg, i64)
declare <1024 x i32> @llvm.linx.tepl.binary.shape.v1024i32(
    <1024 x i32>, <1024 x i32>, i32 immarg, i32 immarg, i32 immarg,
    i64, i64, i64)

define void @dynamic_rect_add(ptr %lhs, ptr %rhs, ptr %dst,
                              i64 %valid_col, i64 %valid_row,
                              i64 %stride) {
entry:
  %a = call <1024 x i32> @llvm.linx.tma.tload.shape.v1024i32(
      ptr %lhs, i32 1, i32 0, i64 %valid_col, i64 %valid_row, i64 32,
      i32 8, i64 %stride)
  %b = call <1024 x i32> @llvm.linx.tma.tload.shape.v1024i32(
      ptr %rhs, i32 1, i32 0, i64 %valid_col, i64 %valid_row, i64 32,
      i32 8, i64 %stride)
  %sum = call <1024 x i32> @llvm.linx.tepl.binary.shape.v1024i32(
      <1024 x i32> %a, <1024 x i32> %b, i32 0, i32 8, i32 1,
      i64 %valid_col, i64 %valid_row, i64 32)
  call void @llvm.linx.tma.tstore.shape.v1024i32(
      ptr %dst, <1024 x i32> %sum, i32 1, i32 0, i64 %valid_col,
      i64 %valid_row, i64 32, i32 8, i64 %stride)
  ret void
}

; CHECK-LABEL: dynamic_rect_add:
; CHECK: BSTART.TLOAD
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb0
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb1
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb2
; CHECK: BSTART.TADD
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb0
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb1
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb2
; CHECK: BSTART.TSTORE
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb0
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb1
; CHECK-NEXT: B.DIM{{[[:space:]]+}}{{[^,]+}}, 0, ->lb2
