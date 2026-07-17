; RUN: llc -mtriple=linx64 -O2 < %s > %t 2>&1 || true
; RUN: FileCheck %s < %t

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.acccvt(%linx.tile, i32, i32, i64, i64)

define void @bad_acccvt_chain(ptr %acc_src) {
entry:
  %acc = call %linx.tile @llvm.linx.tile.tload(ptr %acc_src, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.acccvt(%linx.tile %acc, i32 8, i32 1, i64 0, i64 0)
  ret void
}

; CHECK: LLVM ERROR: Linx: cube.acccvt requires accumulator operand produced by cube.mamulb or cube.mamulb.acc
