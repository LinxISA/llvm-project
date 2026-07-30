; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tepl.unary(%linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.tepl.binary(%linx.tile, %linx.tile, i32, i32, i32)

define void @tepl_binary_tadd(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.tepl.binary(%linx.tile %ta, %linx.tile %tb, i32 0, i32 8, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tepl_binary_tadd:
; CHECK: BSTART.TADD
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK: BSTART.TSTORE

define void @tepl_unary_rowmax(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.tepl.unary(%linx.tile %ta, i32 65, i32 8, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tepl_unary_rowmax:
; CHECK: BSTART.TROWMAX
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK: BSTART.TSTORE

define void @tepl_unary_int8_shape(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 19, i64 0, i64 128, i64 32, i64 0)
  %tc = call %linx.tile @llvm.linx.tepl.unary(%linx.tile %ta, i32 15, i32 8, i32 19)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 8, i32 19, i64 0, i64 128, i64 32, i64 0)
  ret void
}

; CHECK-LABEL: tepl_unary_int8_shape:
; CHECK: BSTART.TABS
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}128,{{[[:space:]]+}}->lb1
; CHECK: BSTART.TSTORE
