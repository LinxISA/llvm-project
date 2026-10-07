; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %s 2>&1 | FileCheck %s

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @branch_only_publication(<32 x i32> %input, i1 %take) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %output = phi <32 x i32> [ poison, %entry ], [ %output.next, %latch ]
  br i1 %take, label %then, label %else

then:
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %inserted = insertelement <32 x i32> %output, i32 %value, i32 %element
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %latch

else:
  br label %latch

latch:
  %output.next = phi <32 x i32> [ %published, %then ], [ %output, %else ]
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  ret <32 x i32> %output.next
}

; CHECK: PTO element region: publication must execute on every region iteration

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

