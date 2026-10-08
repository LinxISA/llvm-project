; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %s 2>&1 | FileCheck %s

target triple = "linx64v5"
declare void @unknown_effect()
declare i32 @__gxx_personality_v0(...)
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @invoke_in_region(<32 x i32> %input)
    personality ptr @__gxx_personality_v0 {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %latch ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %latch ]
  invoke void @unknown_effect() to label %work unwind label %handler
handler:
  %landing = landingpad { ptr, i32 } cleanup
  br label %work
work:
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %i
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %latch
latch:
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %published
}

; CHECK: PTO element region: exceptional control flow is not supported in PTO element regions

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
