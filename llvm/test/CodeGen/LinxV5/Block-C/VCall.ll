; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK


; CHECK: VPAR  copyin,   <M: 4, N: 4, K: 1, MR>   [a0],        ->t<512B>
; CHECK: VPAR  copyin,   <M: 4, N: 4, K: 1, MR>   [a1],        ->t<512B>
; CHECK: VPAR  tadd,     <M: 4, N: 4, K: 1, MR>   t#2, t#1,     ->t<512B>
; CHECK: VPAR  copyout,  <M: 4, N: 4, K: 1, MR>   t#1, [a2]

define dso_local void @tile_caller(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3) local_unnamed_addr  {
entry:
  %0 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr nonnull @tadd, i64 4, i64 4, i64 1, <16 x double> %0, <16 x double> %1)
  tail call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr nonnull @copyout, i64 4, i64 4, i64 1, <16 x double> %2, ptr %p3)
  ret void
}

declare void @copyin(<16 x double> noundef, ptr noundef)

declare <16 x double> @llvm.linx.vcall.par.1d0u.v16f64(ptr, i64, i64, i64, ...)

declare void @tadd(<16 x double> noundef, <16 x double> noundef, <16 x double> noundef)

declare <16 x double> @llvm.linx.vcall.par.1d2u.v16f64(ptr, i64, i64, i64, <16 x double>, <16 x double>, ...)

declare void @copyout(<16 x double> noundef, ptr noundef)

declare void @llvm.linx.vcall.par.0d1u.v16f64(ptr, i64, i64, i64, <16 x double>, ...)
