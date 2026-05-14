; RUN: llc < %s -linxv5-reuse-mark=true -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK
; RUN: llc < %s -linxv5-reuse-mark=false -enable-all-vector-as-tilereg=true -march=linx64v5be -O3 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK1


; CHECK:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<128B>
; CHECK:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<128B>
; CHECK:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#2.reuse, t#1, ->t<128B>
; CHECK:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#3, t#1, ->t<128B>
; CHECK:VPAR copyout, <M: 4, N: 4, K: 1, MR> t#1, [a2]

; CHECK1:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a0], ->t<128B>
; CHECK1:VPAR copyin, <M: 4, N: 4, K: 1, MR> [a1], ->t<128B>
; CHECK1:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#2.reuse, t#1.reuse, ->t<128B>
; CHECK1:VPAR tadd,   <M: 4, N: 4, K: 1, MR> t#3.reuse, t#1.reuse, ->t<128B>
; CHECK1:VPAR copyout, <M: 4, N: 4, K: 1, MR> t#1.reuse, [a2]
; Function Attrs: nounwind
define dso_local void @tile_caller(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3) local_unnamed_addr #0 {
entry:
  %0 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p1)
  %1 = tail call <16 x double> (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.1d0u.v16f64(ptr nonnull @copyin, i64 4, i64 4, i64 1, ptr %p2)
  %2 = tail call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr nonnull @tadd, i64 4, i64 4, i64 1, <16 x double> %0, <16 x double> %1)
  %3 = tail call <16 x double> (ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) @llvm.linx.vcall.par.1d2u.v16f64(ptr nonnull @tadd, i64 4, i64 4, i64 1, <16 x double> %0, <16 x double> %2)
  tail call void (ptr, i64, i64, i64, <16 x double>, ...) @llvm.linx.vcall.par.0d1u.v16f64(ptr nonnull @copyout, i64 4, i64 4, i64 1, <16 x double> %3, ptr %p3)
  ret void
}

declare void @copyin(<16 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare <16 x double> @llvm.linx.vcall.par.1d0u.v16f64(ptr, i64, i64, i64, ...) #2

declare void @tadd(<16 x double> noundef, <16 x double> noundef, <16 x double> noundef) #1

; Function Attrs: nounwind
declare <16 x double> @llvm.linx.vcall.par.1d2u.v16f64(ptr, i64, i64, i64, <16 x double>, <16 x double>, ...) #2

declare void @copyout(<16 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare void @llvm.linx.vcall.par.0d1u.v16f64(ptr, i64, i64, i64, <16 x double>, ...) #2
