; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tile.tmov(%linx.tile, i32, i32, i64, i1)

define void @tile_intrinsics_v2v(ptr %src, ptr %dst) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 3, i64 17, i64 8, i64 4, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tmov(%linx.tile %t0, i32 6, i32 3, i64 17, i1 true)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t1, i32 6, i32 3, i64 17, i64 8, i64 4, i64 0)
  ret void
}

; CHECK-LABEL: tile_intrinsics_v2v:
; CHECK: BSTART.TLOAD
; CHECK: B.DATR{{[[:space:]]+}}Layout17
; A killed source can be coalesced with the TMOV result without emitting a copy.
; CHECK-NOT: BSTART.TMOV
; CHECK: BSTART.TSTORE
; CHECK: B.DATR{{[[:space:]]+}}Layout17
; CHECK-NOT: BSTART.TMOV
; CHECK-NOT: B.ARG
