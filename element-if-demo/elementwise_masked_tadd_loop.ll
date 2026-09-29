; ModuleID = 'element-if-demo/elementwise_masked_tadd_loop.cpp'
source_filename = "element-if-demo/elementwise_masked_tadd_loop.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5"

; Function Attrs: mustprogress nounwind
define dso_local void @_Z28elementwise_masked_tadd_loopPfS_S_mmi(ptr noundef %lhs_ptr, ptr noundef %rhs_ptr, ptr noundef %out_ptr, i64 noundef %mask_low, i64 noundef %mask_high, i32 noundef signext %iterations) local_unnamed_addr #0 {
entry:
  %cmp11 = icmp sgt i32 %iterations, 0
  br i1 %cmp11, label %for.body.preheader, label %for.cond.cleanup

for.body.preheader:                               ; preds = %entry
  %wide.trip.count = zext i32 %iterations to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %mul = shl i64 %indvars.iv, 4
  %idx.ext = and i64 %mul, 4294967280
  %add.ptr = getelementptr inbounds float, ptr %lhs_ptr, i64 %idx.ext
  %add.ptr3 = getelementptr inbounds float, ptr %rhs_ptr, i64 %idx.ext
  %add.ptr6 = getelementptr inbounds float, ptr %out_ptr, i64 %idx.ext
  %0 = tail call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %add.ptr, i64 16)
  %1 = tail call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %add.ptr3, i64 16)
  %2 = tail call <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32(i64 16, i64 16, i64 1, i64 31, <128 x float> %0, <128 x float> %1, i64 %mask_low, i64 %mask_high, i64 0, i64 1)
  tail call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 1, i64 1, i64 1, i64 3, ptr %add.ptr6, i64 16, <128 x float> %2)
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !llvm.loop !4
}

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64) #1

; Function Attrs: nounwind
declare <128 x float> @llvm.linx.experimental.ew.tadd.masked.v128f32.v128f32.v128f32(i64, i64, i64, i64, <128 x float>, <128 x float>, i64, i64, i64, i64) #1

; Function Attrs: nounwind
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>) #1

attributes #0 = { mustprogress nounwind "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 1, !"SmallDataLimit", i32 8}
!3 = !{!"clang version 15.0.4 (git@github.com:LinxISA/llvm-project.git af743c28be634956da3df24ac2d6cccc2c3c520e)"}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.mustprogress"}
