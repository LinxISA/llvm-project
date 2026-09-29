; ModuleID = 'tmp/simt_break_test.cpp'
source_filename = "tmp/simt_break_test.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; Function Attrs: argmemonly mustprogress nofree noinline nounwind
define dso_local void @simt_break_test(ptr noundef %in, ptr nocapture noundef writeonly %out, i32 noundef signext %max_probe) local_unnamed_addr #0 {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %cmp13 = icmp sgt i32 %max_probe, 0
  br i1 %cmp13, label %for.body.lr.ph, label %cleanup3

for.body.lr.ph:                                   ; preds = %entry
  %1 = and i16 %0, 1
  %tobool = icmp ne i16 %1, 0
  %2 = zext i16 %0 to i64
  %wide.trip.count = zext i32 %max_probe to i64
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.inc
  %indvars.iv = phi i64 [ 0, %for.body.lr.ph ], [ %indvars.iv.next, %for.inc ]
  %acc.015 = phi i32 [ 0, %for.body.lr.ph ], [ %add2, %for.inc ]
  %3 = add nuw nsw i64 %indvars.iv, %2
  %arrayidx = getelementptr inbounds i32, ptr %in, i64 %3
  %4 = load volatile i32, ptr %arrayidx, align 4, !tbaa !6
  %cmp1 = icmp eq i64 %indvars.iv, 3
  %or.cond = select i1 %tobool, i1 %cmp1, i1 false
  br i1 %or.cond, label %cleanup3, label %for.inc

for.inc:                                          ; preds = %for.body
  %add2 = add nsw i32 %4, %acc.015
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %cleanup3, label %for.body, !llvm.loop !10

cleanup3:                                         ; preds = %for.body, %for.inc, %entry
  %acc.2 = phi i32 [ 0, %entry ], [ %4, %for.body ], [ %add2, %for.inc ]
  %idxprom4 = zext i16 %0 to i64
  %arrayidx5 = getelementptr inbounds i32, ptr %out, i64 %idxprom4
  store i32 %acc.2, ptr %arrayidx5, align 4, !tbaa !6
  ret void
}

; Function Attrs: nofree nosync nounwind readnone
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
!5 = !{!"clang version 15.0.4 (linx64v5-musl-local f25aa63f7aa291eff97acefb9fa16fe7a9d6580c)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.mustprogress"}
