; RUN: llc < %s -enable-all-vector-as-tilereg=true --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK: MPAR  copyin, <M: 1, N: 1, K: 1, MR> [a0], ->t<512KB>
; CHECK: MPAR  copyin, <M: 1, N: 1, K: 1, MR> [a1], ->t<512KB>
; CHECK: VPAR  tadd, <M: 1, N: 1, K: 1, MR> t#2, t#1, ->t<512KB>
; CHECK: MPAR  copyout, <M: 1, N: 1, K: 1, MR> t#1, [a2]
define dso_local void @tile_caller(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3) local_unnamed_addr {
entry:
  %0 = tail call <65536 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v65536f64(ptr nonnull @copyin, i64 1, i64 1, i64 1, ptr %p1)
  %1 = tail call <65536 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v65536f64(ptr nonnull @copyin, i64 1, i64 1, i64 1, ptr %p2)
  %2 = tail call <65536 x double> (ptr, i64, i64, i64, <65536 x double>, <65536 x double>, ...) @llvm.linx.vcall.par.1d2u.v65536f64.v65536f64.v65536f64(ptr nonnull @tadd, i64 1, i64 1, i64 1, <65536 x double> %0, <65536 x double> %1)
  tail call void (ptr, i64, i64, i64, <65536 x double>, ...) @llvm.linx.mcall.par.0d1u.v65536f64(ptr nonnull @copyout, i64 1, i64 1, i64 1, <65536 x double> %2, ptr %p3)
  ret void
}

declare dso_local void @copyin(<65536 x double> noundef, ptr noundef)

declare <65536 x double> @llvm.linx.mcall.par.1d0u.v65536f64(ptr, i64, i64, i64, ...)

declare dso_local void @tadd(<65536 x double> noundef, <65536 x double> noundef, <65536 x double> noundef)

declare <65536 x double> @llvm.linx.vcall.par.1d2u.v65536f64.v65536f64.v65536f64(ptr, i64, i64, i64, <65536 x double>, <65536 x double>, ...)

declare dso_local void @copyout(<65536 x double> noundef, ptr noundef)

declare void @llvm.linx.mcall.par.0d1u.v65536f64(ptr, i64, i64, i64, <65536 x double>, ...)
