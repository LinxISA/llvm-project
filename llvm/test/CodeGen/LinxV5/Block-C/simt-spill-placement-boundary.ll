; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -stop-after=finalize-isel -o - | FileCheck %s --check-prefixes=TERM
; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true | FileCheck %s --check-prefixes=ASM

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; 这个用例不强行验证“真的发生了 spill”，只验证更底层的机制：
; 单边 if 的 end_cf 恢复点会被降成 LinxV5PseudoCopy2PTerm，
; 这样 regalloc / spill placement 不应该把 spill 随意跨过这个 mask 边界。
;
; 源码形态大致等价于：
; using FT = float __attribute__((matrix_type(16, 16)));
; __vec__ void simt_mask_boundary(FT __in__ TA, FT __in__ TB, FT __out__ TC) {
;   float *a = blkv_get_tile_ptr(TA);
;   float *b = blkv_get_tile_ptr(TB);
;   float *c = blkv_get_tile_ptr(TC);
;   int x = blkv_get_index_x();
;   float res = 1.0f;
;   if (((int)a[x] & 1) == 0)
;     res = b[x];
;   c[x] = res;
; }

; TERM-LABEL: name: _Z18simt_mask_boundaryu11matrix_typeILm16ELm16EfES_S_
; TERM: bb.2.if.end:
; TERM: SIMT_PSEL
; TERM-NEXT: LinxV5PseudoCopy2PTerm
; TERM-NEXT: SIMT_Continuous_SW

; ASM-LABEL: _Z18simt_mask_boundaryu11matrix_typeILm16ELm16EfES_S_:
; ASM: v.psel

define dso_local void @_Z18simt_mask_boundaryu11matrix_typeILm16ELm16EfES_S_(<256 x float> noundef %TA, <256 x float> noundef %TB, <256 x float> __out__ noundef %TC) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %idxprom = zext i16 %1 to i64
  %arrayidx = getelementptr inbounds float, ptr addrspace(6) %0, i64 %idxprom
  %2 = load float, ptr addrspace(6) %arrayidx, align 4, !tbaa !6
  %conv = fptosi float %2 to i32
  %3 = and i32 %conv, 1
  %cmp = icmp eq i32 %3, 0
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %4 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TB)
  %arrayidx3 = getelementptr inbounds float, ptr addrspace(6) %4, i64 %idxprom
  %5 = load float, ptr addrspace(6) %arrayidx3, align 4, !tbaa !6
  br label %if.end

if.end:
  %res.0 = phi float [ %5, %if.then ], [ 1.000000e+00, %entry ]
  %6 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float> %TC)
  %arrayidx5 = getelementptr inbounds float, ptr addrspace(6) %6, i64 %idxprom
  store float %res.0, ptr addrspace(6) %arrayidx5, align 4, !tbaa !6
  ret void
}

declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256f32(<256 x float>) #1
declare i16 @llvm.blkv.get.index.x() #1

attributes #0 = { mustprogress nofree noinline nosync nounwind willreturn "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4"}
!6 = !{!7, !7, i64 0}
!7 = !{!"float", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
