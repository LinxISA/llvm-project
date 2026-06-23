; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true -stop-after=virtregrewriter -o - | FileCheck %s --check-prefix=VRR

target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; Source:
;
; void __mtc__ simt_break_test(int *in, int *out, int max_probe) {
;   int tid = blkv_get_index_x();
;   int acc = 0;
;   for (int i = 0; i < max_probe; ++i) {
;     int v = in[tid + i];
;     if ((tid & 1) && i == 3) {
;       acc = v;
;       break;
;     }
;     acc += v;
;   }
;   out[tid] = acc;
; }
;
; The if-break lowering temporarily writes P with the per-lane exit mask.
; It must restore the old active mask before llvm.blkv.loop reads P, otherwise
; the loop update becomes exit_mask & ~broken instead of old_active & ~broken.

; VRR-LABEL: name: simt_break_test
; VRR-LABEL: bb.4.Flow:
; VRR: %[[OLD_ACTIVE:[0-9]+]]:simtcgsl = LinxV5PseudoCopyFromP implicit $simt_p
; VRR: SIMT_CMP_NEI_P {{.*}} implicit-def $simt_p, implicit $simt_p
; VRR: %[[EXIT_MASK:[0-9]+]]:simtcgsl = LinxV5PseudoCopyFromP implicit $simt_p
; VRR: LinxV5PseudoCopy2P %[[OLD_ACTIVE]], implicit-def dead $simt_p, implicit $simt_p
; VRR: %[[BROKEN:[0-9]+]]:simtcgsl = SIMT_OR_SCAR {{.*}} %[[EXIT_MASK]], {{.*}}
; VRR: %[[NOT_BROKEN:[0-9]+]]:simtcgsl = SIMT_XORI_SCAR {{.*}} %[[BROKEN]], {{.*}} -1, implicit $simt_p
; VRR: %[[LOOP_ACTIVE:[0-9]+]]:simtcgsl = LinxV5PseudoCopyFromP implicit $simt_p
; VRR: %[[NEXT_ACTIVE:[0-9]+]]:simtcgsl = SIMT_AND_SCAR {{.*}} %[[NOT_BROKEN]], {{.*}} %[[LOOP_ACTIVE]], {{.*}}
; VRR: LinxV5PseudoCopy2PTerm %[[NEXT_ACTIVE]], implicit-def dead $simt_p, implicit $simt_p

define dso_local void @simt_break_test(ptr noundef %in, ptr nocapture noundef writeonly %out, i32 noundef signext %max_probe) local_unnamed_addr #0 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %cmp13 = icmp sgt i32 %max_probe, 0
  br i1 %cmp13, label %for.body.lr.ph, label %cleanup3

for.body.lr.ph:
  %1 = and i16 %0, 1
  %tobool = icmp ne i16 %1, 0
  %2 = zext i16 %0 to i64
  %wide.trip.count = zext i32 %max_probe to i64
  br label %for.body

for.body:
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.inc ]
  %acc.015 = phi i32 [ 0, %for.body.lr.ph ], [ %add2, %for.inc ]
  %3 = add nuw nsw i64 %indvars.iv, %2
  %arrayidx = getelementptr inbounds i32, ptr %in, i64 %3
  %4 = load volatile i32, ptr %arrayidx, align 4, !tbaa !6
  %cmp1 = icmp eq i64 %indvars.iv, 3
  %or.cond = select i1 %tobool, i1 %cmp1, i1 false
  br i1 %or.cond, label %cleanup3, label %for.inc

for.inc:
  %add2 = add nsw i32 %4, %acc.015
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %cleanup3, label %for.body, !llvm.loop !10

cleanup3:
  %acc.2 = phi i32 [ 0, %entry ], [ %4, %for.body ], [ %add2, %for.inc ]
  %idxprom4 = zext i16 %0 to i64
  %arrayidx5 = getelementptr inbounds i32, ptr %out, i64 %idxprom4
  store i32 %acc.2, ptr %arrayidx5, align 4, !tbaa !6
  ret void
}

declare i16 @llvm.blkv.get.index.x() #1

attributes #0 = { argmemonly mustprogress nofree noinline nounwind "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="0" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
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
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.mustprogress"}
