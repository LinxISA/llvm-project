; RUN: llc < %s -enable-all-vector-as-tilereg=true -mcpu=janus --march=linx64v5 -linxv5-enable-clock-hand-opt=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK-LABEL: _Z4testPf:
; CHECK: TLOAD.ND2ZN	<LB0: 16, LB1: 16, LB2: 16, FP32, Others>	[a0,a1], 	->t<1KB>
; CHECK: TSTORE.ND2ZN	<LB0: 16, LB1: 16, LB2: 16, FP32>, t#1,	[a0,a1]
define dso_local void @_Z4testPf(ptr %p) local_unnamed_addr {
entry:
  %0 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 16, i64 1, i64 78, i64 3, ptr %p, i64 4)
  tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <256 x float> %0)
  ret void
}

declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)

declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
