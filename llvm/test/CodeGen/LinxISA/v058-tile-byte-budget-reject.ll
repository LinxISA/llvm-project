; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)

define void @bad_tload_tile_bytes(ptr %src) {
entry:
  ; 256*128*1*4B = 131072B > 65536B architectural cap.
  %t = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 10, i32 17, i64 0, i64 256, i64 128, i64 0)
  ret void
}

; CHECK: Linx: tile.tload tile-byte check failed: bytes=131072B
; CHECK: exceeds architectural max 65536B
