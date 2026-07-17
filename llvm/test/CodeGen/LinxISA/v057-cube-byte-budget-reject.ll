; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.mamulb(%linx.tile, %linx.tile, i32, i32, i32)

define void @bad_cube_tile_bytes(ptr %a, ptr %b) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  ; 16*16*16*4B = 16384B > 4096B strict CUBE output cap.
  %tc = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 16, i32 16, i32 16)
  ret void
}

; CHECK: Linx: cube.mamulb tile-byte check failed: bytes=16384B
; CHECK: exceeds strict max 4096B
