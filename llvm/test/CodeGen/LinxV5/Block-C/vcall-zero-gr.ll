; RUN: llc < %s -enable-all-vector-as-tilereg=true -march=linx64 -O3 | FileCheck %s --dump-input always -vv

; CHECK: VPAR vfoo, <M: 1, N: 1, K: 1, MR>  [a1], ->t<4KB>
define void @foo(ptr noundef %p) {
  %v = tail call <1024 x float> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v1024f32(ptr @vfoo, i64 1, i64 1, i64 1, i32 0)
  tail call void (ptr, i64, i64, i64, <1024 x float>, ...) @llvm.linx.mcall.par.0d1u.v1024f32(ptr @vcopyout, i64 1, i64 1, i64 1, <1024 x float> %v, ptr %p)
  ret void
}

declare void @vfoo(<1024 x float> noundef, i32 noundef signext)

declare <1024 x float> @llvm.linx.vcall.par.1d0u.v1024f32(ptr, i64, i64, i64, ...)

declare void @vcopyout(ptr noundef, <1024 x float> noundef)

declare void @llvm.linx.mcall.par.0d1u.v1024f32(ptr, i64, i64, i64, <1024 x float>, ...)
