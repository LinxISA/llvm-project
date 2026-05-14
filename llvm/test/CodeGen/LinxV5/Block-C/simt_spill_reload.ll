; RUN: llc < %s -O2 -enable-all-vector-as-tilereg=true 2>&1 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; Source to generate the IR below
; using FT = int __attribute__((matrix_type(16, 16)));
; __vec__ void test_branch(FT __in__ TA, int *output, int size) {
;     int* array = blkv_get_tile_ptr(TA);
;     int x = blkv_get_index_x();
;
;     int sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
;     int prod1 = 1, prod2 = 1, prod3 = 1, prod4 = 1;
;     int diff1 = 0, diff2 = 0, diff3 = 0, diff4 = 0;
;     int temp1, temp2, temp3, temp4;
;
;     for (int i = 0; i < x; i++) {
;         // 在循环中使用所有这些变量
;         temp1 = array[i] * i;
;         temp2 = array[i] + i;
;         temp3 = array[i] - i;
;         temp4 = array[i] / (i + 1);
;
;         sum1 += temp1;
;         sum2 += temp2;
;         sum3 += temp3;
;         sum4 += temp4;
;
;         prod1 *= (temp1 + 1);
;         prod2 *= (temp2 + 1);
;         prod3 *= (temp3 + 1);
;         prod4 *= (temp4 + 1);
;
;         diff1 -= temp1;
;         diff2 -= temp2;
;         diff3 -= temp3;
;         diff4 -= temp4;
;
;         // 更多的中间计算
;         int intermediate1 = sum1 * prod1;
;         int intermediate2 = sum2 * prod2;
;         int intermediate3 = sum3 * prod3;
;         int intermediate4 = sum4 * prod4;
;
;         diff1 += intermediate1;
;         diff2 += intermediate2;
;         diff3 += intermediate3;
;         diff4 += intermediate4;
;     }
;
;     output[0] = diff1;
;     output[1] = diff2;
;     output[2] = diff3;
;     output[3] = diff4;
; }

; ModuleID = '../src/linx-llvm/llvm/test/CodeGen/LinxV5/Block-C/simt_spill_reload.cpp'
source_filename = "../src/linx-llvm/llvm/test/CodeGen/LinxV5/Block-C/simt_spill_reload.cpp"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"
target triple = "linx64v5-unknown-linux-musl"

; CHECK: addi	zero, 2568, ->t
; CHECK-NEXT: v.sw.u.local    vn#1.reuse.uw, [to1, lc0<<2, t#1.ud]
; CHECK: 	.section .stack_size,"a",@progbits
; CHECK-NEXT: .globl	_Z11test_branchu11matrix_typeILm16ELm16EiEPii_stack_size
; CHECK-NEXT: _Z11test_branchu11matrix_typeILm16ELm16EiEPii_stack_size:
; CHECK-NEXT: .word	3336
; Function Attrs: mustprogress nofree noinline nosync nounwind
define dso_local void @_Z11test_branchu11matrix_typeILm16ELm16EiEPii(<256 x i32> noundef %TA, ptr nocapture noundef writeonly %output, i32 noundef signext %size) local_unnamed_addr #0 {
entry:
  %0 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256i32(<256 x i32> %TA)
  %1 = tail call i16 @llvm.blkv.get.index.x()
  %cmp75.not = icmp eq i16 %1, 0
  br i1 %cmp75.not, label %for.cond.cleanup, label %for.body.preheader

for.body.preheader:                               ; preds = %entry
  %wide.trip.count = zext i16 %1 to i64
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body, %entry
  %diff1.0.lcssa = phi i32 [ 0, %entry ], [ %add28, %for.body ]
  %diff2.0.lcssa = phi i32 [ 0, %entry ], [ %add29, %for.body ]
  %diff3.0.lcssa = phi i32 [ 0, %entry ], [ %add30, %for.body ]
  %diff4.0.lcssa = phi i32 [ 0, %entry ], [ %add31, %for.body ]
  store i32 %diff1.0.lcssa, ptr %output, align 4, !tbaa !6
  %arrayidx33 = getelementptr inbounds i32, ptr %output, i64 1
  store i32 %diff2.0.lcssa, ptr %arrayidx33, align 4, !tbaa !6
  %arrayidx34 = getelementptr inbounds i32, ptr %output, i64 2
  store i32 %diff3.0.lcssa, ptr %arrayidx34, align 4, !tbaa !6
  %arrayidx35 = getelementptr inbounds i32, ptr %output, i64 3
  store i32 %diff4.0.lcssa, ptr %arrayidx35, align 4, !tbaa !6
  ret void

for.body:                                         ; preds = %for.body.preheader, %for.body
  %indvars.iv = phi i64 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %sum1.088 = phi i32 [ 0, %for.body.preheader ], [ %add8, %for.body ]
  %sum2.087 = phi i32 [ 0, %for.body.preheader ], [ %add9, %for.body ]
  %sum3.086 = phi i32 [ 0, %for.body.preheader ], [ %add10, %for.body ]
  %sum4.085 = phi i32 [ 0, %for.body.preheader ], [ %add11, %for.body ]
  %prod1.083 = phi i32 [ 1, %for.body.preheader ], [ %mul13, %for.body ]
  %prod2.082 = phi i32 [ 1, %for.body.preheader ], [ %mul15, %for.body ]
  %prod3.081 = phi i32 [ 1, %for.body.preheader ], [ %mul17, %for.body ]
  %prod4.080 = phi i32 [ 1, %for.body.preheader ], [ %mul19, %for.body ]
  %diff4.079 = phi i32 [ 0, %for.body.preheader ], [ %add31, %for.body ]
  %diff3.078 = phi i32 [ 0, %for.body.preheader ], [ %add30, %for.body ]
  %diff2.077 = phi i32 [ 0, %for.body.preheader ], [ %add29, %for.body ]
  %diff1.076 = phi i32 [ 0, %for.body.preheader ], [ %add28, %for.body ]
  %arrayidx = getelementptr inbounds i32, ptr addrspace(6) %0, i64 %indvars.iv
  %2 = load i32, ptr addrspace(6) %arrayidx, align 4, !tbaa !6
  %3 = trunc i64 %indvars.iv to i32
  %mul = mul nsw i32 %2, %3
  %add = add nsw i32 %2, %3
  %sub = sub nsw i32 %2, %3
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %4 = trunc i64 %indvars.iv.next to i32
  %div = sdiv i32 %2, %4
  %add8 = add nsw i32 %mul, %sum1.088
  %add9 = add nsw i32 %add, %sum2.087
  %add10 = add nsw i32 %sub, %sum3.086
  %add11 = add nsw i32 %div, %sum4.085
  %add12 = add nsw i32 %mul, 1
  %mul13 = mul nsw i32 %add12, %prod1.083
  %add14 = add nsw i32 %add, 1
  %mul15 = mul nsw i32 %add14, %prod2.082
  %add16 = add nsw i32 %sub, 1
  %mul17 = mul nsw i32 %add16, %prod3.081
  %add18 = add nsw i32 %div, 1
  %mul19 = mul nsw i32 %add18, %prod4.080
  %sub20 = sub i32 %diff1.076, %mul
  %sub21 = sub i32 %diff2.077, %add
  %sub22 = sub i32 %diff3.078, %sub
  %sub23 = sub i32 %diff4.079, %div
  %mul24 = mul nsw i32 %mul13, %add8
  %mul25 = mul nsw i32 %mul15, %add9
  %mul26 = mul nsw i32 %mul17, %add10
  %mul27 = mul nsw i32 %mul19, %add11
  %add28 = add nsw i32 %sub20, %mul24
  %add29 = add nsw i32 %sub21, %mul25
  %add30 = add nsw i32 %sub22, %mul26
  %add31 = add nsw i32 %sub23, %mul27
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body, !llvm.loop !10
}

; Function Attrs: nofree nosync nounwind readnone
declare ptr addrspace(6) @llvm.blkv.get.tile.ptr.v256i32(<256 x i32>) #1

; Function Attrs: nofree nosync nounwind readnone
declare i16 @llvm.blkv.get.index.x() #1

attributes #0 = { mustprogress nofree noinline nosync nounwind "__vec__" "frame-pointer"="none" "min-legal-vector-width"="8192" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+relax" }
attributes #1 = { nofree nosync nounwind readnone }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"lp64"}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"PIE Level", i32 2}
!4 = !{i32 1, !"SmallDataLimit", i32 8}
!5 = !{!"clang version 15.0.4 (Linx Base Software 103.0.0.B0xx 24xxxx V0.36 961ddc0bff29575a3850b04d4e03ece8b5eceaf3)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !8, i64 0}
!8 = !{!"omnipotent char", !9, i64 0}
!9 = !{!"Simple C++ TBAA"}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.mustprogress"}
