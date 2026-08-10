; RUN: not --crash llc -mtriple=linx64 -O2 < %s 2>&1 | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.tileop.unary(%linx.tile, i32, i32, i32)

define void @bad_tileop_selector(ptr %src) {
entry:
  %t0 = call %linx.tile @llvm.linx.tile.tload(ptr %src, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %t1 = call %linx.tile @llvm.linx.tileop.unary(%linx.tile %t0, i32 128, i32 6, i32 0)
  ret void
}

; CHECK: Linx: tileop.unary requires packed Mode/Function in range 0..127
