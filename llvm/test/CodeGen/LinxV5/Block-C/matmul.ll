; RUN: llc < %s -enable-all-vector-as-tilereg=true -stop-after=finalize-isel --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; should not be %x:tile_src = COPY %y
; CHECK: tile_abs = COPY
define void @foo(ptr %p) {
  %a = tail call <1024 x float> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v1024f32(ptr @vcopyin, i64 1, i64 1, i64 1)
  %b = tail call <1024 x float> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v1024f32(ptr @vcopyin, i64 1, i64 1, i64 2)
  %m = call <1024 x float> @llvm.linx.blk.matmul.v1024f32.v1024f32.v1024f32(i64 32, i64 32, i64 32, i64 1, i64 1, <1024 x float> %a, <1024 x float> %b)
  tail call void (ptr, i64, i64, i64, <1024 x float>, ...) @llvm.linx.mcall.par.0d1u.v1024f32(ptr @vcopyout, i64 1, i64 1, i64 1, <1024 x float> %m, ptr %p)
  ret void
}

declare void @vcopyin(<1024 x float>)

declare <1024 x float> @llvm.linx.vcall.par.1d0u.v1024f32(ptr, i64, i64, i64, ...)

declare void @vcopyout(ptr noundef, <1024 x float> noundef)

declare void @llvm.linx.mcall.par.0d1u.v1024f32(ptr, i64, i64, i64, <1024 x float>, ...)

declare <1024 x float> @llvm.linx.blk.matmul.v1024f32.v1024f32.v1024f32(i64, i64, i64, i64, i64, <1024 x float>, <1024 x float>)
