; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/positive.ll | FileCheck %s --check-prefix=POSITIVE
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-call-context,function(linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -disable-output %t/positive.ll
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/alias.ll | FileCheck %s --check-prefix=ALIAS
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/escape.ll | FileCheck %s --check-prefix=ESCAPE
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/ambiguous.ll | FileCheck %s --check-prefix=AMBIGUOUS
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/callers.ll | FileCheck %s --check-prefix=CALLERS
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/callee-write.ll | FileCheck %s --check-prefix=CALLEEWRITE
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/field-loads.ll | FileCheck %s --check-prefix=FIELDLOADS
; RUN: opt -mtriple=linx64v5 -passes=linx-v5-element-call-context -S %t/call-shapes.ll | FileCheck %s --check-prefix=CALLSHAPES

; POSITIVE-LABEL: define void @caller
; POSITIVE: call void @helper.pto.context.0(ptr %closure, ptr %source)
; POSITIVE-LABEL: define internal void @helper.pto.context.0
; POSITIVE: %value = load i32, ptr %source.element, align 4, !noalias ![[SCOPES:[0-9]+]]
; POSITIVE: store i32 %value, ptr %destination.element, align 4, !alias.scope ![[SCOPES]]
; POSITIVE: ![[SCOPES]] = !{![[SCOPE:[0-9]+]]}
; POSITIVE: ![[SCOPE]] = distinct !{![[SCOPE]], ![[DOMAIN:[0-9]+]], !"linx.pto.element.private.destination"}
; POSITIVE: ![[DOMAIN]] = distinct !{![[DOMAIN]], !"linx.pto.element.call.context"}

; ALIAS-LABEL: define internal void @helper(
; ALIAS-NOT: pto.context
; ALIAS: call void @helper(ptr %closure, ptr %buffer)

; ESCAPE-LABEL: define internal void @helper(
; ESCAPE-NOT: pto.context
; ESCAPE: call void @helper(ptr %closure, ptr %source)

; AMBIGUOUS-LABEL: define internal void @helper(
; AMBIGUOUS-NOT: pto.context
; AMBIGUOUS: call void @helper(ptr %closure, ptr %source)

; CALLERS-LABEL: define internal void @helper(
; CALLERS-LABEL: define void @safe_caller
; CALLERS: call void @helper.pto.context.{{[0-9]+}}(ptr %closure, ptr %source)
; CALLERS-LABEL: define void @partial_overlap_caller
; CALLERS: call void @helper(ptr %closure, ptr %partial)
; CALLERS-LABEL: define void @conditional_store_caller
; CALLERS: call void @helper(ptr %closure, ptr %source)
; CALLERS-LABEL: define void @non_alloca_caller
; CALLERS: call void @helper(ptr %closure, ptr %source)
; CALLERS-LABEL: define void @mixed_caller
; CALLERS: call void @helper.pto.context.{{[0-9]+}}(ptr %safe.closure, ptr %source)
; CALLERS: call void @helper(ptr %unsafe.closure, ptr %unsafe.buffer)

; CALLEEWRITE-NOT: pto.context
; CALLEEWRITE: call void @helper(ptr %closure, ptr %source)

; FIELDLOADS-LABEL: define void @volatile_caller
; FIELDLOADS: call void @volatile_helper(ptr %closure, ptr %source)
; FIELDLOADS-LABEL: define void @atomic_caller
; FIELDLOADS: call void @atomic_helper(ptr %closure, ptr %source)
; FIELDLOADS-NOT: pto.context

; CALLSHAPES-NOT: pto.context
; CALLSHAPES: call void @recursive_helper(ptr %closure, ptr %source)
; CALLSHAPES: call void @bundle_helper(ptr %closure, ptr %source) [ "deopt"(i32 0) ]
; CALLSHAPES: %target = load ptr, ptr @indirect_slot
; CALLSHAPES: call void %target(ptr %closure, ptr %source)

;--- positive.ll
declare void @llvm.linx.experimental.element.region(metadata)

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

define void @caller(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- callers.ll
@global_buffer = internal global [32 x i32] zeroinitializer, align 128
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define void @safe_caller(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @partial_overlap_caller() {
entry:
  %buffer = alloca [64 x i32], align 128
  %partial = getelementptr inbounds [64 x i32], ptr %buffer, i32 0, i32 32
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %partial)
  ret void
}

define void @conditional_store_caller(ptr %source, i1 %choose) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  br i1 %choose, label %store, label %call
store:
  store ptr %buffer, ptr %closure, align 8
  br label %call
call:
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @non_alloca_caller(ptr %source) {
entry:
  %closure = alloca ptr, align 8
  store ptr @global_buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

define void @mixed_caller(ptr %source) {
entry:
  %safe.buffer = alloca [32 x i32], align 128
  %safe.closure = alloca ptr, align 8
  store ptr %safe.buffer, ptr %safe.closure, align 8
  call void @helper(ptr %safe.closure, ptr %source)
  %unsafe.buffer = alloca [32 x i32], align 128
  %unsafe.closure = alloca ptr, align 8
  store ptr %unsafe.buffer, ptr %unsafe.closure, align 8
  call void @helper(ptr %unsafe.closure, ptr %unsafe.buffer)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- callee-write.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  store ptr %source, ptr %closure, align 8
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define void @caller(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- field-loads.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @volatile_helper(ptr %closure, ptr %source) {
entry:
  %destination = load volatile ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define internal void @atomic_helper(ptr %closure, ptr %source) {
entry:
  %destination = load atomic ptr, ptr %closure monotonic, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !3)
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !4
exit:
  ret void
}

define void @volatile_caller(ptr %source) {
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @volatile_helper(ptr %closure, ptr %source)
  ret void
}

define void @atomic_caller(ptr %source) {
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @atomic_helper(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

;--- call-shapes.ll
@indirect_slot = internal global ptr @indirect_helper
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @recursive_helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  call void @recursive_helper(ptr %closure, ptr %source)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define internal void @bundle_helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !3)
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !4
exit:
  ret void
}

define internal void @indirect_helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !6)
  %source.element = getelementptr inbounds i32, ptr %source, i32 %element
  %value = load i32, ptr %source.element, align 4
  %destination.element = getelementptr inbounds i32, ptr %destination, i32 %element
  store i32 %value, ptr %destination.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !7
exit:
  ret void
}

define void @caller(ptr %source) {
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @recursive_helper(ptr %closure, ptr %source)
  call void @bundle_helper(ptr %closure, ptr %source) [ "deopt"(i32 0) ]
  %target = load ptr, ptr @indirect_slot
  call void %target(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}

;--- alias.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define void @caller() {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  call void @helper(ptr %closure, ptr %buffer)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- escape.ll
@escaped = global ptr null
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define void @caller(ptr %source) {
entry:
  %buffer = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %buffer, ptr %closure, align 8
  store ptr %closure, ptr @escaped, align 8
  call void @helper(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- ambiguous.ll
declare void @llvm.linx.experimental.element.region(metadata)

define internal void @helper(ptr %closure, ptr %source) {
entry:
  %destination = load ptr, ptr %closure, align 8
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  call void @llvm.linx.experimental.element.region(metadata !0)
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

define void @caller(ptr %source, i1 %choose) {
entry:
  %first = alloca [32 x i32], align 128
  %second = alloca [32 x i32], align 128
  %closure = alloca ptr, align 8
  store ptr %first, ptr %closure, align 8
  br i1 %choose, label %replace, label %call
replace:
  store ptr %second, ptr %closure, align 8
  br label %call
call:
  call void @helper(ptr %closure, ptr %source)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
