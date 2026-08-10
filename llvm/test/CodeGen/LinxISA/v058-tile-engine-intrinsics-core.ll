; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tileop.unary(%linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.tileop.binary(%linx.tile, %linx.tile, i32, i32, i32)

define void @vec_binary_tadd(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.tileop.binary(%linx.tile %ta, %linx.tile %tb, i32 0, i32 6, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: vec_binary_tadd:
; CHECK: BSTART.VEC{{[[:space:]]+}}TADD,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#2, t#1, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE

define void @sfu_unary_rowmax(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.tileop.unary(%linx.tile %ta, i32 65, i32 6, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: sfu_unary_rowmax:
; CHECK: BSTART.SFU{{[[:space:]]+}}TROWMAX,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE

define void @vec_unary_int8_shape(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 19, i64 0, i64 128, i64 32, i64 0)
  %tc = call %linx.tile @llvm.linx.tileop.unary(%linx.tile %ta, i32 15, i32 6, i32 19)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tc, i32 6, i32 19, i64 0, i64 128, i64 32, i64 0)
  ret void
}

; CHECK-LABEL: vec_unary_int8_shape:
; CHECK: BSTART.VEC{{[[:space:]]+}}TABS,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}128,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE
