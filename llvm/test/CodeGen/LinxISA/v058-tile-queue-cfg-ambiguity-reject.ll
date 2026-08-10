; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @ambiguous_tile_queue(ptr %a, ptr %b, ptr %dst, i1 %take_load) {
entry:
  %base = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  br i1 %take_load, label %with_load, label %without_load

with_load:
  %extra = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  br label %join

without_load:
  br label %join

join:
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %base, i32 6, i32 1, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK: Linx: tile queue order is ambiguous at a control-flow join
