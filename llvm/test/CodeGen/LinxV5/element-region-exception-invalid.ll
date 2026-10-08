; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %s 2>&1 | FileCheck %s

target triple = "linx64v5"

declare void @may_throw()
declare i32 @__gxx_personality_v0(...)
declare void @llvm.linx.experimental.element.region(metadata)

define void @exceptional_region() personality ptr @__gxx_personality_v0 {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  invoke void @may_throw()
          to label %latch unwind label %unwind

latch:
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

unwind:
  %exception = landingpad { ptr, i32 }
          cleanup
  br label %latch

exit:
  ret void
}

; CHECK: PTO element region: exceptional control flow is not supported in PTO element regions

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
