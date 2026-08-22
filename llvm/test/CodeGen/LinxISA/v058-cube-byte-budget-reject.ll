; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)

define void @bad_cube_tile_bytes(ptr %a, ptr %b) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  ; 32*32*32*4B = 131072B > 65536B architectural CUBE output cap.
  %tc = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 32, i32 32, i32 32)
  ret void
}

; CHECK: Linx: cube.tmatmul tile-byte check failed: bytes=131072B
; CHECK: exceeds architectural max 65536B
