; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare void @llvm.linx.tile.tstore(ptr, %linx.tile, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.mamulb(%linx.tile, %linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.cube.mamulb.acc(%linx.tile, %linx.tile, %linx.tile, i32, i32, i32)

define void @acc_carrier_pressure(ptr %a, ptr %b, ptr %dst) {
entry:
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  %s0 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s1 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s2 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s3 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s4 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s5 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s6 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s7 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %s8 = call %linx.tile @llvm.linx.cube.mamulb(%linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  %out = call %linx.tile @llvm.linx.cube.mamulb.acc(%linx.tile %s0, %linx.tile %ta, %linx.tile %tb, i32 4, i32 4, i32 4)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s1, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s2, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s3, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s4, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s5, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s6, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s7, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %s8, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  call void @llvm.linx.tile.tstore(ptr %dst, %linx.tile %out, i32 8, i32 0, i64 0, i64 8, i64 8, i64 0)
  ret void
}

; The carrier is removed before the MAMULB.ACC result is published. Under
; pressure this preserves the remaining M/N ranks instead of retaining a stale
; carrier entry when the allocator chooses a different destination register.
; CHECK-LABEL: acc_carrier_pressure:
; CHECK:      BSTART.TMATMUL.ACC
; CHECK:      BSTART.ACCCVT
; CHECK:      BSTART.TSTORE
; CHECK:      B.IOT{{[[:space:]]+[mn]#[1-8], last, ->[mn]<4KB>}}
; CHECK-NOT:  {{m|n}}#9
