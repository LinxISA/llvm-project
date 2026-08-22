; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%linx.tile = type target("linx.tile")

declare %linx.tile @llvm.linx.tile.tload(ptr, i32, i32, i64, i64, i64, i64)
declare %linx.tile @llvm.linx.cube.tmatmul(%linx.tile, %linx.tile, i32, i32, i32)
declare %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile, %linx.tile, %linx.tile, i32, i32, i32)

define void @cube_arbitrary_13x19x5(ptr %a, ptr %b) {
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 13, i32 19, i32 5)
  ret void
}

define void @cube_acc_16k_output_13x97x5(ptr %acc, ptr %a, ptr %b) {
  %tacc = call %linx.tile @llvm.linx.tile.tload(ptr %acc, i32 8, i32 17, i64 0, i64 32, i64 97, i64 0)
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 6, i32 17, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul.acc(%linx.tile %tacc, %linx.tile %ta, %linx.tile %tb, i32 13, i32 97, i32 5)
  ret void
}

define void @cube_capacity_boundary(ptr %a, ptr %b) {
  %ta = call %linx.tile @llvm.linx.tile.tload(ptr %a, i32 3, i32 17, i64 0, i64 8, i64 8, i64 0)
  %tb = call %linx.tile @llvm.linx.tile.tload(ptr %b, i32 7, i32 17, i64 0, i64 8, i64 8, i64 0)
  %out = call %linx.tile @llvm.linx.cube.tmatmul(%linx.tile %ta, %linx.tile %tb, i32 32, i32 512, i32 4)
  ret void
}

; CHECK-LABEL: cube_arbitrary_13x19x5:
; CHECK: BSTART.TMATMUL{{[[:space:]]+}}S32
; CHECK: C.B.DIMI{{[[:space:]]+}}13, ->lb0
; CHECK: C.B.DIMI{{[[:space:]]+}}19, ->lb1
; CHECK: C.B.DIMI{{[[:space:]]+}}5, ->lb2
; CHECK: B.IOT{{.*}}->m<4KB>
; CHECK-LABEL: cube_acc_16k_output_13x97x5:
; CHECK: BSTART.TMATMUL.ACC{{[[:space:]]+}}S32
; CHECK: B.IOT{{.*}}->m<16KB>
; CHECK-LABEL: cube_capacity_boundary:
; CHECK: BSTART.TMATMUL{{[[:space:]]+}}S32
; CHECK: B.IOT{{.*}}->m<64KB>
