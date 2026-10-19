; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -disable-output %t/equal.ll
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/different.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/nonzero.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/under-aligned.ll 2>&1 | FileCheck %s
;
; Two naturally aligned, same-typed unit-stride stores of the same constant
; commute even when their whole objects may alias.  A different stored value
; keeps the normal overlap rejection.
; CHECK: predication: memory streams may overlap across elements

;--- equal.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @equal(ptr %first, ptr %second) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %first.element = getelementptr inbounds i32, ptr %first, i32 %element
  store i32 0, ptr %first.element, align 4
  %second.element = getelementptr inbounds i32, ptr %second, i32 %element
  store i32 0, ptr %second.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- nonzero.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @nonzero(ptr %first, ptr %second) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %first.element = getelementptr inbounds i32, ptr %first, i32 %element
  store i32 7, ptr %first.element, align 4
  %second.element = getelementptr inbounds i32, ptr %second, i32 %element
  store i32 7, ptr %second.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- under-aligned.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @under_aligned(ptr %first, ptr %second) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %first.element = getelementptr inbounds i64, ptr %first, i32 %element
  store i64 0, ptr %first.element, align 4
  %second.element = getelementptr inbounds i64, ptr %second, i32 %element
  store i64 0, ptr %second.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- different.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @different(ptr %first, ptr %second) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %first.element = getelementptr inbounds i32, ptr %first, i32 %element
  store i32 0, ptr %first.element, align 4
  %second.element = getelementptr inbounds i32, ptr %second, i32 %element
  store i32 1, ptr %second.element, align 4
  %next = add nuw nsw i32 %element, 1
  %done = icmp eq i32 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
