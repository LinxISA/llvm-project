; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tile.tmov(%linx.tile, i32, i32, i32, i64, i1)

define void @bad_tmov_mode(ptr %src) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %t1 = call %linx.tile @llvm.linx.tile.tmov(%linx.tile %t0, i32 2, i32 8, i32 0, i64 0, i1 false)
  ret void
}

; CHECK: Linx: tile.tmov mode must be 0(V2V) or 1(A2V)
