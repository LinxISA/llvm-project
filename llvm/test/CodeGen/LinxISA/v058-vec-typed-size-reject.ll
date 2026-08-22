; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.vec.tadd(%linx.tile, %linx.tile, i32, i32)

define void @bad_vec_tadd_size(ptr %a, ptr %b) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 0, i64 0, i64 8, i64 10, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 0, i64 0, i64 8, i64 10, i64 0)
  %tc = call %linx.tile @llvm.linx.vec.tadd(%linx.tile %ta, %linx.tile %tb, i32 0, i32 0)
  ret void
}

; CHECK: Linx: vec.tadd requires SizeCode in [1,10] (128B..64KB)
