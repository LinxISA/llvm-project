; RUN: opt -passes='loop-simplify,loop-idiom' -S %s | FileCheck %s
; Required target regions use the standard hint to retain their loop bodies.
target triple = "x86_64-unknown-linux-gnu"
define void @preserved(ptr noalias %src, ptr noalias %dst) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %sp = getelementptr i8, ptr %src, i64 %i
  %dp = getelementptr i8, ptr %dst, i64 %i
  %v = load i8, ptr %sp, align 1
  store i8 %v, ptr %dp, align 1
  %next = add nuw nsw i64 %i, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !0
exit:
  ret void
}
; CHECK-LABEL: define void @preserved(
; CHECK-NOT: call void @llvm.memcpy
; CHECK: load i8
; CHECK: store i8
; CHECK: !llvm.loop
; CHECK: ret void

define void @ordinary(ptr noalias %src, ptr noalias %dst) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %sp = getelementptr i8, ptr %src, i64 %i
  %dp = getelementptr i8, ptr %dst, i64 %i
  %v = load i8, ptr %sp, align 1
  store i8 %v, ptr %dp, align 1
  %next = add nuw nsw i64 %i, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %loop, label %exit
exit:
  ret void
}
; CHECK-LABEL: define void @ordinary(
; CHECK: call void @llvm.memcpy
; CHECK: ret void
!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.disable_nonforced"}
