; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)

define void @bad_cube_tile_bytes(ptr %a, ptr %b) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  ; CUBE_M32 output requires N*128B. N=513 exceeds 64 KiB by one CELL.
  %tc = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 32, i32 513, i32 4)
  ret void
}

; CHECK: Linx: cube.tmatmul requires output CUBE capacity 65664B
; CHECK: exceeding Local maximum 65536B
