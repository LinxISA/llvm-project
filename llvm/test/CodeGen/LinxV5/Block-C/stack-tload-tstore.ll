
; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK: TLOAD.NORM	<LB0: 128, LB1: 1, LB2: 128, S64, Zero>	[a1,zero], 	->t<1KB>
define dso_local void @_Z4testPfRu11matrix_typeILm1ELm256EfE(ptr  %p, ptr %TO) local_unnamed_addr {
entry:
  %0 = load <256 x float>, ptr %TO
  tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <256 x float> %0)
  ret void
}

declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>) #1
