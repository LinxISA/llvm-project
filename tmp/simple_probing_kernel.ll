; ModuleID = 'tmp/simple_probing_kernel.cpp'
source_filename = "tmp/simple_probing_kernel.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

$_Z6lookupILi256ELi1EEvPhPlPiPjjiii = comdat any

; Function Attrs: mustprogress noinline nounwind
define weak_odr dso_local void @_Z6lookupILi256ELi1EEvPhPlPiPjjiii(ptr noalias noundef %slot, ptr noalias noundef %keys, ptr noalias noundef %values_output, ptr noalias noundef %hashes_output, i32 noundef signext %capacity, i32 noundef signext %num, i32 noundef %entry_size, i32 noundef %max_probe) local_unnamed_addr #0 comdat {
entry:
  %0 = tail call i16 @llvm.blkv.get.index.x()
  %conv = zext i16 %0 to i32
  %1 = tail call i16 @llvm.blkv.get.index.y()
  %conv1 = zext i16 %1 to i32
  %mul = shl nuw nsw i32 %conv1, 8
  %add = add nuw nsw i32 %mul, %conv
  %cmp = icmp slt i32 %add, %num
  br i1 %cmp, label %if.then, label %if.end19

if.then:                                          ; preds = %entry
  %idxprom = zext i32 %add to i64
  %arrayidx = getelementptr inbounds i64, ptr %keys, i64 %idxprom
  %2 = load i64, ptr %arrayidx, align 8, !tbaa !6
  %.tr = trunc i64 %2 to i32
  %conv3 = shl i32 %.tr, 3
  %arrayidx5 = getelementptr inbounds i32, ptr %hashes_output, i64 %idxprom
  store i32 %conv3, ptr %arrayidx5, align 4, !tbaa !10
  %cmp633 = icmp sgt i32 %max_probe, 0
  br i1 %cmp633, label %for.body.lr.ph, label %if.end19

for.body.lr.ph:                                   ; preds = %if.then
  %arrayidx16 = getelementptr inbounds i32, ptr %values_output, i64 %idxprom
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %if.end
  %conv3.pn = phi i32 [ %conv3, %for.body.lr.ph ], [ %add17, %if.end ]
  %probe_cnt.034 = phi i32 [ 0, %for.body.lr.ph ], [ %inc, %if.end ]
  %curr_slot.035 = urem i32 %conv3.pn, %capacity
  %mul7 = mul i32 %curr_slot.035, %entry_size
  %idx.ext = zext i32 %mul7 to i64
  %add.ptr = getelementptr inbounds i8, ptr %slot, i64 %idx.ext
  %3 = load i64, ptr %add.ptr, align 8, !tbaa !6
  %cmp13 = icmp eq i64 %3, %2
  br i1 %cmp13, label %if.then14, label %if.end

if.then14:                                        ; preds = %for.body
  %add.ptr11 = getelementptr inbounds i8, ptr %add.ptr, i64 8
  %4 = load i32, ptr %add.ptr11, align 4, !tbaa !10
  store i32 %4, ptr %arrayidx16, align 4, !tbaa !10
  br label %if.end

if.end:                                           ; preds = %if.then14, %for.body
  %probe_cnt.1 = phi i32 [ %max_probe, %if.then14 ], [ %probe_cnt.034, %for.body ]
  %add17 = add nuw i32 %curr_slot.035, 1
  %inc = add nsw i32 %probe_cnt.1, 1
  %cmp6 = icmp slt i32 %inc, %max_probe
  br i1 %cmp6, label %for.body, label %if.end19, !llvm.loop !12

if.end19:                                         ; preds = %if.end, %if.then, %entry
  ret void
}

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.y() #1

attributes #0 = { mustprogress noinline nounwind "__mtc__" "frame-pointer"="none" "min-legal-vector-width"="0" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }

!llvm.linker.options = !{}
!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (linx64v5-musl-local f25aa63f7aa291eff97acefb9fa16fe7a9d6580c)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"long", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = !{!11, !11, i64 0}
!11 = !{!"int", !8, i64 0}
!12 = distinct !{!12, !13}
!13 = !{!"llvm.loop.mustprogress"}
