; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -linxv5-enable-ldst-bridge=true 2>&1 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64-unknown-linux-musl"

; Source to generate the IR below
; using FT = float __attribute__((matrix_type(16, 16)));
; __mtc__ void test_gm_in_tr_out(FT __out__ TC, float* gm) {
;     float* ptrC = blkv_get_tile_ptr(TC);
; 
;     int x = blkv_get_index_x() * 4; // make not continuous
; 
;     ptrC[x] = gm[x];
; }
; 
; __mtc__ void test_tr_in_gm_out(FT __in__ TA, float* gm) {
;     float* ptrA = blkv_get_tile_ptr(TA);
; 
;     int x = blkv_get_index_x() * 4; // make not continuous
; 
;     gm[x] = ptrA[x];
; }

; CHECK: _Z17test_gm_in_tr_outu11matrix_typeILm16ELm16EfEPf
; CHECK: v.lw.brg.local [ri0.sd, lc0.uh<<4], ->t.w
; CHECK: v.sw.u.brg.local t#1.sw, [to, vt#1.sd]
; Function Attrs: mustprogress nofree noinline nosync nounwind willreturn
define dso_local void @_Z17test_gm_in_tr_outu11matrix_typeILm16ELm16EfEPf(<256 x float> __out__ noundef %TC, ptr nocapture noundef readonly %gm) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %ext = zext i16 %1 to i64
  %idxprom = mul i64 %ext, 4
  %arrayidx = getelementptr inbounds float, ptr %gm, i64 %idxprom
  %2 = load float, ptr %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds float, ptr %0, i64 %idxprom
  store float %2, ptr %arrayidx2, align 4, !tbaa !6
  ret void
}

; Function Attrs: nofree nosync nounwind readnone
declare ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float>) #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #1

; CHECK: _Z17test_tr_in_gm_outu11matrix_typeILm16ELm16EfEPf
; CHECK: v.lw.brg.local [ta, lc0.uh<<4], ->t.w
; CHECK: v.sw.u.brg.local t#1.sw, [ri0.sd, vt#1.sd]
; Function Attrs: mustprogress nofree noinline nosync nounwind willreturn
define dso_local void @_Z17test_tr_in_gm_outu11matrix_typeILm16ELm16EfEPf(<256 x float> noundef %TA, ptr nocapture noundef writeonly %gm) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %ext = zext i16 %1 to i64
  %idxprom = mul i64 %ext, 4
  %arrayidx = getelementptr inbounds float, ptr %0, i64 %idxprom
  %2 = load float, ptr %arrayidx, align 4, !tbaa !6
  %arrayidx2 = getelementptr inbounds float, ptr %gm, i64 %idxprom
  store float %2, ptr %arrayidx2, align 4, !tbaa !6
  ret void
}

attributes #0 = { mustprogress nofree noinline nosync nounwind willreturn "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (Linx Base Software 103.0.0.B0xx 24xxxx V0.36 ff745252621dea655c706037c00b54f67970fae0)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"float", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
