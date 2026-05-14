; RUN: llc < %s -enable-all-vector-as-tilereg=true -mcpu=janus --march=linx64 -linxv5-enable-clock-hand-opt=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; ModuleID = 'test.cpp'
source_filename = "test.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64-unknown-linux-musl"

; CHECK-LABEL: _Z11tile_callerPdS_S_:
; CHECK: TCOPY t#8, ->t<8KB>

; Function Attrs: mustprogress nounwind
define dso_local void @_Z11tile_callerPdS_S_(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3) local_unnamed_addr #0 {
entry:
  %0 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %1 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %2 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %3 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %4 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %5 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %6 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %7 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %8 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %9 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %10 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %11 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %12 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %13 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %14 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %15 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %16 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p2)
  %17 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, ptr %p1)
  %18 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %0, <1024 x double> %1)
  %19 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %2, <1024 x double> %3)
  %20 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %4, <1024 x double> %5)
  %21 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %6, <1024 x double> %7)
  %22 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %8, <1024 x double> %9)
  %23 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %10, <1024 x double> %11)
  %24 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %12, <1024 x double> %13)
  %25 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %14, <1024 x double> %15)
  %26 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %16, <1024 x double> %17)
  %27 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %18, <1024 x double> %19)


  %28 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %20, <1024 x double> %21)
  %29 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %22, <1024 x double> %23)
  %30 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %24, <1024 x double> %25)
  %31 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %26, <1024 x double> %27)
  %32 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %28, <1024 x double> %29)
  %33 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %30, <1024 x double> %31)
  %34 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %32, <1024 x double> %33)
  %35 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %34, <1024 x double> %34)
  %36 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %35, <1024 x double> %35)
  %37 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %36, <1024 x double> %36)
  %38 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %37, <1024 x double> %37)
  %39 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %38, <1024 x double> %38)
  %40 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %39, <1024 x double> %21)
  %41 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_, i64 1, i64 1, i64 1, <1024 x double> %40, <1024 x double> %28)
  tail call void (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.mcall.par.0d1u.v1024f64(ptr nonnull @_Z7copyoutu11matrix_typeILm1ELm1024EdEPd, i64 1, i64 1, i64 1, <1024 x double> %41, ptr %p3)
  ret void
}

declare void @_Z6copyinu11matrix_typeILm1ELm1024EdEPd(<1024 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare <1024 x double> @llvm.linx.mcall.par.1d0u.v1024f64(ptr, i64, i64, i64, ...) #2

declare void @_Z4taddILi1EEvu11matrix_typeILm1ELm1024EdES0_S0_(<1024 x double> noundef, <1024 x double> noundef, <1024 x double> noundef) #3

; Function Attrs: nounwind
declare <1024 x double> @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) #2

declare void @_Z7copyoutu11matrix_typeILm1ELm1024EdEPd(<1024 x double> noundef, ptr noundef) #1

; Function Attrs: nounwind
declare void @llvm.linx.mcall.par.0d1u.v1024f64(ptr, i64, i64, i64, <1024 x double>, ...) #2

attributes #0 = { mustprogress nounwind "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { "__mtc__" "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #2 = { nounwind }
attributes #3 = { "__vec__" "frame-pointer"="none" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (ssh://git@codehub-dg-y.huawei.com:2222/linx/ISA-Codesign/BlockISA/linx-llvm.git d82a7c69d7ead45f0a00c9312a0d4525e102197f)"}
