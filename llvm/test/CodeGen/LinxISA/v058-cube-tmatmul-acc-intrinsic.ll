; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile, %linx.tile, %linx.tile, i32, i32, i32)

define void @tmatmul_acc_roundtrip(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  %acc_seed = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %out = call %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile %acc_seed, %linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 6, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; CHECK-LABEL: tmatmul_acc_roundtrip:
; CHECK: BSTART.TMATMUL
; CHECK: BSTART.TMATMUL.ACC
; The accumulator C and left operand A occupy the first ordered descriptor.
; The terminal descriptor binds B and allocates the explicit 4x4xi32 result;
; CUBE_M32 uses one 128B CELL per S32 output column, so four columns require
; a 512B destination.
; CHECK: B.IOT{{[[:space:]]+[^,]+,[[:space:]]*[^,]+, mask=1111}}
; CHECK: B.IOT{{[[:space:]]+[^,]+, mask=1111, last, ->[mntu]<512B>}}
; CHECK-NOT: BSTART.ACCCVT
; CHECK: BSTART.TSTORE
