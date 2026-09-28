; ModuleID = 'element-if-demo/elementwise_if_for.cpp'
source_filename = "element-if-demo/elementwise_if_for.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5"

; Function Attrs: mustprogress noinline nounwind optnone
define dso_local void @_Z18elementwise_if_forRDv128_fRKS_S2_(ptr noundef nonnull align 32 dereferenceable(512) %out, ptr noundef nonnull align 32 dereferenceable(512) %lhs, ptr noundef nonnull align 32 dereferenceable(512) %rhs) #0 {
entry:
  %out.addr = alloca ptr, align 8
  %lhs.addr = alloca ptr, align 8
  %rhs.addr = alloca ptr, align 8
  store ptr %out, ptr %out.addr, align 8
  store ptr %lhs, ptr %lhs.addr, align 8
  store ptr %rhs, ptr %rhs.addr, align 8
  %0 = load ptr, ptr %out.addr, align 8
  %1 = load ptr, ptr %lhs.addr, align 8
  %2 = load <128 x float>, ptr %1, align 32
  %3 = load ptr, ptr %rhs.addr, align 8
  %4 = load <128 x float>, ptr %3, align 32
  %linx.elementwise.zero = call <128 x float> @llvm.linx.experimental.ew.texpands.v128f32.f32(i64 16, i64 8, i64 1, i64 31, float 0.000000e+00)
  %linx.elementwise.predicate = call <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %2, <128 x float> %linx.elementwise.zero, i64 3)
  %linx.elementwise.then = call <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %2, <128 x float> %4, i64 -1, i64 -1, i64 0, i64 1)
  %linx.elementwise.else = call <128 x float> @llvm.linx.experimental.ew.tsub.masked.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %2, <128 x float> %4, i64 -1, i64 -1, i64 0, i64 1)
  %linx.elementwise.if = call <128 x float> @llvm.linx.experimental.ew.tsel.v128f32.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %linx.elementwise.predicate, <128 x float> %linx.elementwise.then, <128 x float> %linx.elementwise.else)
  store <128 x float> %linx.elementwise.if, ptr %0, align 32
  ret void
}

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.texpands.v128f32.f32(i64, i64, i64, i64, float) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, i64, i64, i64, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tsub.masked.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, i64, i64, i64, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tsel.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>) #1

attributes #0 = { mustprogress noinline nounwind optnone "frame-pointer"="all" "linx.elementwise" "linx.elementwise.lanes"="128" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3}
!llvm.ident = !{!4}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{i32 1, !"SmallDataLimit", i32 8}
!4 = !{!"clang version 15.0.4 (git@github.com:LinxISA/llvm-project.git 3454566c26d132f2537b7a02640a5b47797dabc2)"}
