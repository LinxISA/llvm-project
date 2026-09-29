; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

; CHECK-LABEL: <elementwise_tsel>:
; CHECK: BSTART.TEPL TCMP, FP32
; CHECK: TSEL <Row=

define void @elementwise_tsel(ptr %lhs_ptr, ptr %rhs_ptr) #0 {
entry:
  %lhs = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %lhs_ptr, i64 16)
  %rhs = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %rhs_ptr, i64 16)
  %pred = call <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32(
      i64 16, i64 8, i64 1, i64 31,
      <128 x float> %lhs, <128 x float> %rhs, i64 2)
  %out = call <128 x float> @llvm.linx.experimental.ew.tsel.v128f32(
      i64 16, i64 8, i64 1, i64 31,
      <128 x float> %pred, <128 x float> %lhs, <128 x float> %rhs)
  call void @llvm.linx.blk.tstore.v128f32(
      i64 16, i64 8, i64 1, i64 1, i64 0, ptr %lhs_ptr, i64 16,
      <128 x float> %out)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(
    i64, i64, i64, i64, i64, i64, ptr, i64)
declare <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32(
    i64, i64, i64, i64, <128 x float>, <128 x float>, i64)
declare <128 x float> @llvm.linx.experimental.ew.tsel.v128f32(
    i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>)
declare void @llvm.linx.blk.tstore.v128f32(
    i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="128" }
