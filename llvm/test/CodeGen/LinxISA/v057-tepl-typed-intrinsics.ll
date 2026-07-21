; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tepl.tadd(%linx.tile, %linx.tile, i32, i32)
declare %linx.tile @llvm.linx.tepl.tsub(%linx.tile, %linx.tile, i32, i32)
declare %linx.tile @llvm.linx.tepl.trowmax(%linx.tile, i32, i32)

define void @typed_tepl_binops(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tc = call %linx.tile @llvm.linx.tepl.tadd(%linx.tile %ta, %linx.tile %tb, i32 8, i32 1)
  %td = call %linx.tile @llvm.linx.tepl.tsub(%linx.tile %tc, %linx.tile %tb, i32 8, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %td, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: typed_tepl_binops:
; CHECK: BSTART.TADD
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.ARG{{[[:space:]]+}}VV
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#2, t#1.reuse, last, ->t<4KB>
; CHECK: BSTART.TSUB
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.ARG{{[[:space:]]+}}VV
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, t#2, last, ->t<4KB>
; CHECK: BSTART.TSTORE

define void @typed_tepl_rowmax(ptr %a, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  %tr = call %linx.tile @llvm.linx.tepl.trowmax(%linx.tile %ta, i32 8, i32 1)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %tr, i32 8, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: typed_tepl_rowmax:
; CHECK: BSTART.TROWMAX
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb0
; CHECK-NEXT: C.B.DIMI{{[[:space:]]+}}32,{{[[:space:]]+}}->lb1
; CHECK-NEXT: B.ARG{{[[:space:]]+}}VV
; CHECK-NEXT: B.IOT{{[[:space:]]+}}t#1, last, ->t<4KB>
; CHECK: BSTART.TSTORE
