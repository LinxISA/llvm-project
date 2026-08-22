; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile, %linx.tile, %linx.tile, i32, i32, i32)

define void @cube_16k_output(ptr %a, ptr %b) {
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 64, i32 64, i32 4)
  ret void
}

define void @cube_acc_16k_output(ptr %acc, ptr %a, ptr %b) {
  %tacc = call %linx.tile @llvm.linx.tile.tload(ptr %acc, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile %tacc, %linx.tile %ta, %linx.tile %tb, i32 64, i32 64, i32 4)
  ret void
}

; CHECK-LABEL: cube_16k_output:
; CHECK: BSTART.TMATMUL{{[[:space:]]+}}S32
; CHECK: B.IOT{{.*}}->m<16KB>
; CHECK-LABEL: cube_acc_16k_output:
; CHECK: BSTART.TMATMUL.ACC{{[[:space:]]+}}S32
; CHECK: B.IOT{{.*}}->m<16KB>
