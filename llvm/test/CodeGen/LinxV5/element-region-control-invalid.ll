; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @branch_only_publication(i1 %take) {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %output = phi <32 x i32> [ %initial, %entry ], [ %output.next, %latch ]
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %output.next)
  ret void
}

; CHECK-LABEL: define void @branch_only_publication
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr.v32i32{{.*}}<32 x i32> %initial
; CHECK: call void asm sideeffect "", "@2Tr"
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
