; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/positive.ll | FileCheck %s --check-prefix=POS
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-call-context,function(linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -disable-output %t/positive.ll
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/two-level.ll | FileCheck %s --check-prefix=TWO
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/argument-global.ll | FileCheck %s --check-prefix=GLOBAL
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-call-context,function(linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -disable-output %t/argument-global.ll
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/multiple-callers.ll | FileCheck %s --check-prefix=MULTI
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/alias.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/argument-global-alias.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/mixed.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/exported.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/address-taken.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/recursive.ll | FileCheck %s --check-prefix=REJECT

; POS-LABEL: define internal void @atomic_helper
; POS: load i32, ptr %input.element{{.*}}!alias.scope{{.*}}!noalias
; POS: atomicrmw add ptr %histogram.element, i32 1 monotonic{{.*}}!alias.scope{{.*}}!noalias
; POS-LABEL: define internal void @copy_helper
; POS: load i32, ptr %input.element{{.*}}!alias.scope{{.*}}!noalias
; POS: store i32 %value, ptr %output.element{{.*}}!alias.scope{{.*}}!noalias

; TWO-LABEL: define internal void @atomic_helper
; TWO: load i32, ptr %input.element{{.*}}!alias.scope{{.*}}!noalias
; TWO: atomicrmw add ptr %histogram.element, i32 1 monotonic{{.*}}!alias.scope{{.*}}!noalias

; GLOBAL-LABEL: define internal void @atomic_helper
; GLOBAL: load i32, ptr %input.element{{.*}}!alias.scope{{.*}}!noalias
; GLOBAL: atomicrmw add ptr %histogram.element, i32 1 monotonic{{.*}}!alias.scope{{.*}}!noalias

; MULTI-LABEL: define internal void @atomic_helper
; MULTI: load i32, ptr %input.element{{.*}}!alias.scope{{.*}}!noalias
; MULTI: atomicrmw add ptr %histogram.element, i32 1 monotonic{{.*}}!alias.scope{{.*}}!noalias

; REJECT-LABEL: define {{.*}}void @atomic_helper
; REJECT: load i32, ptr %input.element, align 4
; REJECT: atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
; REJECT-NOT: linx.pto.element.ordinary.call.context

;--- positive.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
@output = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @copy_helper(ptr %input, ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !2)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %value = load i32, ptr %input.element, align 4
  %output.element = getelementptr inbounds i32, ptr %output, i32 %element
  store i32 %value, ptr %output.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !3
exit:
  ret void
}

define void @root() {
  call void @atomic_helper(ptr @input, ptr @histogram)
  call void @copy_helper(ptr @input, ptr @output)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !4}
!2 = distinct !{}
!3 = distinct !{!3, !5}
!4 = !{!"llvm.loop.linx.pto.element.region", !0}
!5 = !{!"llvm.loop.linx.pto.element.region", !2}

;--- argument-global.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @atomic_helper(ptr %input) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr @histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define void @root() {
  call void @atomic_helper(ptr @input)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- argument-global-alias.ll
target datalayout = "e-p:64:64"
@histogram = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @atomic_helper(ptr %input) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr @histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define void @root() {
  call void @atomic_helper(ptr @histogram)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- two-level.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %forwarded.input, ptr %forwarded.histogram) {
  call void @atomic_helper(ptr %forwarded.input, ptr %forwarded.histogram)
  ret void
}

define void @root() {
  call void @middle(ptr @input, ptr @histogram)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- multiple-callers.ll
target datalayout = "e-p:64:64"
@input0 = internal global [32 x i32] zeroinitializer, align 128
@histogram0 = internal global [32 x i32] zeroinitializer, align 128
@input1 = internal global [32 x i32] zeroinitializer, align 128
@histogram1 = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define void @root0() {
  call void @atomic_helper(ptr @input0, ptr @histogram0)
  ret void
}
define void @root1() {
  call void @atomic_helper(ptr @input1, ptr @histogram1)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- alias.ll
target datalayout = "e-p:64:64"
@buffer = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)
define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
define void @root() {
  call void @atomic_helper(ptr @buffer, ptr @buffer)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- mixed.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
@buffer = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)
define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
define void @safe() {
  call void @atomic_helper(ptr @input, ptr @histogram)
  ret void
}
define void @unsafe() {
  call void @atomic_helper(ptr @buffer, ptr @buffer)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- exported.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)
define void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
define void @root() {
  call void @atomic_helper(ptr @input, ptr @histogram)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- address-taken.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
@slot = internal global ptr @atomic_helper
declare void @llvm.linx.experimental.element.region(metadata)
define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
define void @root() {
  call void @atomic_helper(ptr @input, ptr @histogram)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- recursive.ll
target datalayout = "e-p:64:64"
@input = internal global [32 x i32] zeroinitializer, align 128
@histogram = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)
define internal void @atomic_helper(ptr %input, ptr %histogram) {
entry:
  call void @atomic_helper(ptr %input, ptr %histogram)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.element = getelementptr inbounds i32, ptr %input, i32 %element
  %expert = load i32, ptr %input.element, align 4
  %histogram.element = getelementptr inbounds i32, ptr %histogram, i32 %expert
  %old = atomicrmw add ptr %histogram.element, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
define void @root() {
  call void @atomic_helper(ptr @input, ptr @histogram)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
