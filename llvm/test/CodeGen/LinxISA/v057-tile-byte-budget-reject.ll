; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)

define void @bad_tload_tile_bytes(ptr %src) {
entry:
  ; 64*64*1*4B = 16384B > 8192B strict cap (dtype=INT32 code 17).
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 17, i64 0, i64 64, i64 64, i64 0)
  ret void
}

; CHECK: Linx: tile.tload tile-byte check failed: bytes=16384B
; CHECK: exceeds strict max 8192B
