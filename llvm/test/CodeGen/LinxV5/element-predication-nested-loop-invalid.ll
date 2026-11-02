; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/unbounded.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/large.ll 2>&1 | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/marked.ll 2>&1 | FileCheck %s --check-prefix=MARKED
; CHECK: predication: dynamic nested control requires scalar i32 data and i32 loop-carried values
; MARKED: predication: nested marked element regions are not supported

;--- unbounded.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @unbounded(ptr %output, i32 %limit) {
entry:
  %wide.limit = zext i32 %limit to i64
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  br label %inner
inner:
  %iteration = phi i64 [ 0, %header ], [ %increment, %inner ]
  %increment = add i64 %iteration, 1
  %more = icmp ult i64 %increment, %wide.limit
  br i1 %more, label %inner, label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- marked.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @marked() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %inner
inner:
  %iteration = phi i32 [ 0, %header ], [ %increment, %inner ]
  %increment = add nuw nsw i32 %iteration, 1
  %more = icmp ult i32 %increment, 4
  br i1 %more, label %inner, label %latch, !llvm.loop !4
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

;--- large.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @large(ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  br label %inner
inner:
  %iteration = phi i64 [ 0, %header ], [ %increment, %inner ]
  %increment = add nuw nsw i64 %iteration, 1
  %more = icmp ult i64 %increment, 65
  br i1 %more, label %inner, label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
