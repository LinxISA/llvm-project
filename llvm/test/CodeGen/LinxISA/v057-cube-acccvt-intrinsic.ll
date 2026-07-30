; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.mamulb(%linx.tile, %linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.cube.acccvt(%linx.tile, i32, i32, i64, i64)

define void @acccvt_roundtrip(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %acc_seed = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %out = call %linx.tile @llvm.linx.cube.acccvt(%linx.tile %acc_seed, i32 8, i32 0, i64 0, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: acccvt_roundtrip:
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TLOAD
; CHECK: BSTART.TMATMUL
; CHECK: BSTART.ACCCVT
; CHECK: BSTART.ACCCVT
; CHECK: BSTART.TSTORE
; CHECK-NOT: B.ARG
