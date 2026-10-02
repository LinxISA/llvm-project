; ModuleID = 'element-if-demo/elementwise_if_for.cpp'
source_filename = "element-if-demo/elementwise_if_for.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; Function Attrs: mustprogress nounwind
define dso_local void @_Z18elementwise_if_forRDv128_fRKS_S2_(ptr nocapture noundef nonnull writeonly align 32 dereferenceable(512) %out, ptr nocapture noundef nonnull readonly align 32 dereferenceable(512) %lhs, ptr nocapture noundef nonnull readonly align 32 dereferenceable(512) %rhs) local_unnamed_addr #0 {
entry:
  %0 = load <128 x float>, ptr %lhs, align 32, !tbaa !6
  %1 = load <128 x float>, ptr %rhs, align 32, !tbaa !6
  %linx.elementwise.zero = tail call <128 x float> @llvm.linx.experimental.ew.texpands.v128f32.f32(i64 16, i64 8, i64 1, i64 31, float 0.000000e+00)
  %linx.elementwise.predicate = tail call <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %0, <128 x float> %linx.elementwise.zero, i64 3)
  %linx.elementwise.then = tail call <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %0, <128 x float> %1, <128 x float> %linx.elementwise.predicate, i64 0, i64 1)
  %linx.elementwise.else = tail call <128 x float> @llvm.linx.experimental.ew.tsub.masked.v128f32.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %0, <128 x float> %1, <128 x float> %linx.elementwise.predicate, i64 1, i64 1)
  %linx.elementwise.if = tail call <128 x float> @llvm.linx.experimental.ew.tsel.v128f32.v128f32.v128f32.v128f32(i64 16, i64 8, i64 1, i64 31, <128 x float> %linx.elementwise.predicate, <128 x float> %linx.elementwise.then, <128 x float> %linx.elementwise.else)
  store <128 x float> %linx.elementwise.if, ptr %out, align 32, !tbaa !6
  ret void
}

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.texpands.v128f32.f32(i64, i64, i64, i64, float) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tcmp.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>, i64, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tsub.masked.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>, i64, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tsel.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>) #1

attributes #0 = { mustprogress nounwind "frame-pointer"="none" "linx.elementwise" "linx.elementwise.lanes"="128" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nounwind }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (https://github.com/LinxISA/llvm-project.git f49d5e2026dd15e0910672178110143d8a2062be)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
