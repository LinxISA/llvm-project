; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -print-after=linx-annotate-control-flow 2>&1 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK
; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true 2>&1 | FileCheck %s --dump-input always -vv --check-prefixes=ASM

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; Source to generate the IR below
; using FT = float __attribute__((matrix_type(16, 16)));
; __vec__ void test_branch(FT __in__ TA, FT __in__ TB, FT __out__ TC, int num) {
;     float* ptrA = blkv_get_tile_ptr(TA);
;     float* ptrB = blkv_get_tile_ptr(TB);
;     float* ptrC = blkv_get_tile_ptr(TC);
;
;     int x = blkv_get_index_x();
;     if ((int)ptrA[x] % 2 == 0) {
;         ptrC[x] = ptrA[x] * ptrA[x-1];
;     } else {
;         ptrC[x] = ptrB[x];
;     }
; }
; using FT = float __attribute__((matrix_type(16, 16)));
; __vec__ void test_branch_2(FT __in__ TA, FT __in__ TB, FT __out__ TC, int num) {
;     float* ptrA = blkv_get_tile_ptr(TA);
;     float* ptrB = blkv_get_tile_ptr(TB);
;     float* ptrC = blkv_get_tile_ptr(TC);
;
;     int x = blkv_get_index_x();
;     float result = 0;
;     if ((int)ptrA[x] % 2 == 0) {
;         result += ptrA[x] * ptrA[x-1];
;     } else {
;         result += ptrB[x];
;     }
;
;     ptrC[x] = result;
; }

; CHECK: @_Z11test_branchu11matrix_typeILm16ELm16EfES_S_i
; CHECK: entry:
; CHECK: call { i1, i64 } @llvm.blkv.if.i64
; CHECK: Flow:
; CHECK: call { i1, i64 } @llvm.blkv.flow.i64.i64
; CHECK: if.end:
; CHECK: %merge = call float @llvm.blkv.merge.cf.f32
; CHECK: call void @llvm.blkv.end.cf.i64
; ASM: v.psel  p, vn#1.sw, vn#2.sw, ->vt.w
; Function Attrs: mustprogress nofree noinline nosync nounwind willreturn
define dso_local void @_Z11test_branchu11matrix_typeILm16ELm16EfES_S_i(<256 x float> noundef %TA, <256 x float> noundef %TB, <256 x float> __out__ noundef %TC, i32 noundef signext %num) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %idxprom = zext i16 %1 to i64
  %arrayidx = getelementptr inbounds float, ptr %0, i64 %idxprom
  %2 = load float, ptr %arrayidx, align 4, !tbaa !6
  %conv1 = fptosi float %2 to i32
  %3 = and i32 %conv1, 1
  %cmp = icmp eq i32 %3, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %sub = add nsw i64 %idxprom, -1
  %arrayidx5 = getelementptr inbounds float, ptr %0, i64 %sub
  %4 = load float, ptr %arrayidx5, align 4, !tbaa !6
  %mul = fmul float %2, %4
  br label %if.end

if.else:                                          ; preds = %entry
  %5 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TB)
  %arrayidx9 = getelementptr inbounds float, ptr %5, i64 %idxprom
  %6 = load float, ptr %arrayidx9, align 4, !tbaa !6
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %.sink = phi float [ %mul, %if.then ], [ %6, %if.else ]
  %7 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %8 = getelementptr inbounds float, ptr %7, i64 %idxprom
  store float %.sink, ptr %8, align 4
  ret void
}

; Function Attrs: nofree nosync nounwind readnone
declare ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float>) #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #1

; CHECK: @_Z13test_branch_2u11matrix_typeILm16ELm16EfES_S_i
; CHECK: entry:
; CHECK: call { i1, i64 } @llvm.blkv.if.i64
; CHECK: Flow:
; CHECK: call { i1, i64 } @llvm.blkv.flow.i64.i64
; CHECK: if.end:
; CHECK: %merge = call float @llvm.blkv.merge.cf.f32
; CHECK: call void @llvm.blkv.end.cf.i64
; ASM: v.psel  p, vn#1.sw, vn#2.sw, ->vt.w
; Function Attrs: mustprogress nofree noinline nosync nounwind willreturn
define dso_local void @_Z13test_branch_2u11matrix_typeILm16ELm16EfES_S_i(<256 x float> noundef %TA, <256 x float> noundef %TB, <256 x float> __out__ noundef %TC, i32 noundef signext %num) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %idxprom = zext i16 %1 to i64
  %arrayidx = getelementptr inbounds float, ptr %0, i64 %idxprom
  %2 = load float, ptr %arrayidx, align 4, !tbaa !6
  %conv1 = fptosi float %2 to i32
  %3 = and i32 %conv1, 1
  %cmp = icmp eq i32 %3, 0
  br i1 %cmp, label %if.then, label %if.else

if.then:                                          ; preds = %entry
  %sub = add nsw i64 %idxprom, -1
  %arrayidx5 = getelementptr inbounds float, ptr %0, i64 %sub
  %4 = load float, ptr %arrayidx5, align 4, !tbaa !6
  %5 = tail call float @llvm.fmuladd.f32(float %2, float %4, float 0.000000e+00)
  br label %if.end

if.else:                                          ; preds = %entry
  %6 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TB)
  %arrayidx7 = getelementptr inbounds float, ptr %6, i64 %idxprom
  %7 = load float, ptr %arrayidx7, align 4, !tbaa !6
  %add = fadd float %7, 0.000000e+00
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %result.0 = phi float [ %5, %if.then ], [ %add, %if.else ]
  %8 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %arrayidx9 = getelementptr inbounds float, ptr %8, i64 %idxprom
  store float %result.0, ptr %arrayidx9, align 4, !tbaa !6
  ret void
}

; Function Attrs: mustprogress nocallback nofree nosync nounwind readnone speculatable willreturn
declare float @llvm.fmuladd.f32(float, float, float) #2

attributes #0 = { mustprogress nofree noinline nosync nounwind willreturn "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }
attributes #2 = { mustprogress nocallback nofree nosync nounwind readnone speculatable willreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (Linx Base Software 103.0.0.B0xx 24xxxx V0.36 2553b92f93accde2fd698ca81bcf343a96539182)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"float", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
