; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK-LABEL: foo
; CHECK: TLOAD.ND2NZ <LB0: 16, LB1: 16, LB2: 1, FP32, Null> [a0,a3], ->t<1KB>
; CHECK: TLOAD.ND2ZN <LB0: 16, LB1: 16, LB2: 1, FP32, Null> [a1,a3], ->t<1KB>
; CHECK: MAMULB <M: 16, N: 16, K: 16, FP32> t#2, t#1, ->acc<1KB>
; CHECK: ACCCVT NZ2ND.canon, <Row: 16, Col: 16, FP32, FP64> acc#1, ->t<1KB>
; CHECK: TSTORE.NORM <LB0: 16, LB1: 16, LB2: 1, FP32>, t#1, [a2,a3]
define void @foo(ptr %pa, ptr %pb, ptr %pc) {
entry:
  %0 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %pa, i64 16)
  %1 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %pb, i64 16)
  %2 = tail call <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, <256 x float> %0, <256 x float> %1)
  %3 = tail call <256 x float> @llvm.linx.blk.acccvt.v256f32.v256f32(i64 16, i64 16, i64 1, i64 0, i64 27, i64 1, <256 x float> %2)
  tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %pc, i64 16, <256 x float> %3)
  ret void
}

declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64, i64, i64, i64, <256 x float>, <256 x float>)
declare <256 x float> @llvm.linx.blk.acccvt.v256f32.v256f32(i64, i64, i64, i64, i64, i64, <256 x float>)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
