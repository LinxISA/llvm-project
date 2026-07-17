; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)

define void @bad_tstore_tile_bytes(ptr %src, ptr %dst) {
entry:
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 17, i64 0, i64 8, i64 8, i64 0)
  ; 32*32*1*4B = 4096B, but SizeCode=5 only allows 512B.
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %t, i32 5, i32 17, i64 0, i64 32, i64 32, i64 0)
  ret void
}

; CHECK: Linx: tile.tstore tile-byte check failed: bytes=4096B
; CHECK: exceeds descriptor limit 512B (SizeCode=5)
