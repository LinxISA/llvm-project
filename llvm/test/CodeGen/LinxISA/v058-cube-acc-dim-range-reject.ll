; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile, %linx.tile, %linx.tile, i32, i32, i32)

define void @bad_cube_acc_dim(ptr %acc_src, ptr %a, ptr %b) {
entry:
  %acc = call %linx.tile @llvm.linx.tile.tload(ptr %acc_src, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile %acc, %linx.tile %ta, %linx.tile %tb, i32 200000, i32 4, i32 4)
  ret void
}

; CHECK: Linx: cube.tmatmul.acc requires m to be a positive power of two in range 1..65536
