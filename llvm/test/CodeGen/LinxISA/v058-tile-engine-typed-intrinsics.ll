; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.vec.tadd(%linx.tile, %linx.tile, i32, i32)
declare %linx.tile @llvm.linx.vec.tsub(%linx.tile, %linx.tile, i32, i32)
declare %linx.tile @llvm.linx.sfu.trowmax(%linx.tile, i32, i32)

define void @typed_vec_binops(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.vec.tadd(%linx.tile %ta, %linx.tile %tb, i32 6, i32 1)
  %td = call %linx.tile @llvm.linx.vec.tsub(%linx.tile %tc, %linx.tile %tb, i32 6, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %td, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: typed_vec_binops:
; CHECK: BSTART.VEC{{[[:space:]]+}}TADD,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#2, t#1, mask=1111, last, ->t<4KB>
; CHECK: BSTART.VEC{{[[:space:]]+}}TSUB,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, t#2, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE

define void @typed_sfu_rowmax(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tr = call %linx.tile @llvm.linx.sfu.trowmax(%linx.tile %ta, i32 6, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tr, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: typed_sfu_rowmax:
; CHECK: BSTART.SFU{{[[:space:]]+}}TROWMAX,
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, mask=1111, last, ->t<4KB>
; CHECK: BSTART.TSTORE
; CHECK-NOT: B.ARG
