; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.acccvt(%linx.tile, i32, i32, i64, i64)

define void @bad_acccvt_qarg0(ptr %src) {
entry:
  %acc = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.acccvt(%linx.tile %acc, i32 8, i32 1, i64 1, i64 0)
  ret void
}

; CHECK: Linx: cube.acccvt requires retired qarg0/qarg1 operands to be zero in PTO ISA 0.57.1
