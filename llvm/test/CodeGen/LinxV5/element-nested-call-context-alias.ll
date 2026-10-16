; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/positive.ll | FileCheck %s --check-prefix=POS
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-call-context,function(linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -disable-output %t/positive.ll
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/alias.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/receiver-escape.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/receiver-mutate.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/callee-write.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/child-escape.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/conditional.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/other-user.ll | FileCheck %s --check-prefix=REJECT

; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/two-calls.ll | FileCheck %s --check-prefix=MULTI
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-call-context,function(linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -disable-output %t/two-calls.ll
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/whole-record.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/wide-overwrite.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/partial-overlap.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/mixed-alias.ll | FileCheck %s --check-prefix=MIXED
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/two-calls-alias.ll | FileCheck %s --check-prefix=REJECT

; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/secondary-alias.ll | FileCheck %s --check-prefix=REJECT
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/self-chain.ll | FileCheck %s --check-prefix=REJECT

; MULTI-LABEL: define void @root
; MULTI-COUNT-2: call void @middle.pto.context.caller.
; MULTI-COUNT-4: define internal void @helper.pto.context.
; MIXED-LABEL: define void @root
; MIXED: call void @middle.pto.context.caller.
; MIXED: call void @middle(ptr %receiver, ptr %buffer)

; POS-LABEL: define void @root
; POS-COUNT-2: call void @middle.pto.context.caller.
; POS-LABEL: define internal void @helper.pto.context.1
; POS: load i32{{.*}} !noalias
; POS: store i32{{.*}} !alias.scope
; POS-LABEL: define internal void @helper.pto.context.3
; POS: load i32{{.*}} !noalias
; POS: store i32{{.*}} !alias.scope
; POS-NOT: define internal void @middle(
; POS-NOT: define internal void @helper(
;
; REJECT-NOT: .pto.context.
;
; Immutable private captures can cross an outlined caller without forcing it
; inline. Context facts belong to a pair of local clones, never to the original
; helper or caller. Every use of the records must remain read-only/private.

;--- positive.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- alias.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %buffer)
  call void @middle(ptr %receiver, ptr %buffer)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- receiver-escape.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  store ptr %receiver, ptr @escaped
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- receiver-mutate.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- callee-write.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  store ptr %source, ptr %receiver, align 8
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- child-escape.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  store ptr %closure, ptr @escaped
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- conditional.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  br i1 %condition, label %init, label %join
init:
  store ptr %closure, ptr %receiver, align 8
  br label %join
join:
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- other-user.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null
declare void @unknown(ptr)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @unknown(ptr %receiver)
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- two-calls.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- whole-record.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %record = load {i64, ptr}, ptr %receiver, align 8
  %closure = extractvalue {i64, ptr} %record, 1
  store ptr %source, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca {i64, ptr}, align 8
  store ptr %buffer, ptr %closure, align 8
  %slot = getelementptr {i64, ptr}, ptr %receiver, i32 0, i32 1
  store ptr %closure, ptr %slot, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- wide-overwrite.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %slot = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 1
  %closure = load ptr, ptr %slot, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca {ptr, ptr}, align 8
  store ptr %buffer, ptr %closure, align 8
  %slot = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 1
  store ptr %closure, ptr %slot, align 8
  store i128 0, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- partial-overlap.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %slot = getelementptr i8, ptr %receiver, i64 4
  %closure = load ptr, ptr %slot, align 4
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca [16 x i8], align 8
  store ptr %buffer, ptr %closure, align 8
  %slot = getelementptr i8, ptr %receiver, i64 4
  store ptr %closure, ptr %slot, align 4
  store ptr %source, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- mixed-alias.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %buffer)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- two-calls-alias.ll
declare void @llvm.linx.experimental.element.region(metadata)
@escaped = global ptr null

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %closure = load ptr, ptr %receiver, align 8
  call void @helper(ptr %closure, ptr %source)
  %destination = load ptr, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %destination)
  ret void
}

define void @root(ptr %source, ptr %other, i1 %condition) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr %receiver, align 8
  call void @middle(ptr %receiver, ptr %source)
  call void @middle(ptr %receiver, ptr %other)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}


;--- secondary-alias.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %s = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %s, align 4
  %d = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %d, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %slot0 = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 0
  %slot1 = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 1
  %closure = load ptr, ptr %slot0, align 8
  %alias = load ptr, ptr %slot1, align 8
  store ptr %source, ptr %alias, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  %receiver = alloca {ptr, ptr}, align 8
  store ptr %buffer, ptr %closure, align 8
  %slot0 = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 0
  %slot1 = getelementptr {ptr, ptr}, ptr %receiver, i32 0, i32 1
  store ptr %closure, ptr %slot0, align 8
  store ptr %closure, ptr %slot1, align 8
  call void @middle(ptr %receiver, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- self-chain.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %s = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %s, align 4
  %d = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %d, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}

define internal void @middle(ptr %receiver, ptr %source) {
entry:
  %self.slot = getelementptr {ptr, ptr, ptr}, ptr %receiver, i32 0, i32 1
  %bad.slot = getelementptr {ptr, ptr, ptr}, ptr %receiver, i32 0, i32 2
  %self = load ptr, ptr %self.slot, align 8
  %bad = load ptr, ptr %bad.slot, align 8
  %target.slot = getelementptr {ptr, ptr, ptr}, ptr %self, i32 0, i32 0
  store ptr %bad, ptr %target.slot, align 8
  %closure = load ptr, ptr %target.slot, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @root(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %good = alloca ptr, align 8
  %bad = alloca ptr, align 8
  %receiver = alloca {ptr, ptr, ptr}, align 8
  store ptr %buffer, ptr %good, align 8
  store ptr %source, ptr %bad, align 8
  %slot0 = getelementptr {ptr, ptr, ptr}, ptr %receiver, i32 0, i32 0
  %slot1 = getelementptr {ptr, ptr, ptr}, ptr %receiver, i32 0, i32 1
  %slot2 = getelementptr {ptr, ptr, ptr}, ptr %receiver, i32 0, i32 2
  store ptr %good, ptr %slot0, align 8
  store ptr %receiver, ptr %slot1, align 8
  store ptr %bad, ptr %slot2, align 8
  call void @middle(ptr %receiver, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
