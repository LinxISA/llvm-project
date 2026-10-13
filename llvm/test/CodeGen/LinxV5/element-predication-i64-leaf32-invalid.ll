; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
;
; Native i64 element Tiles are deliberately limited to the logical32 leaf.
; A v64i64 CUBE_M32 carrier must not silently become a two-column profile.

; CHECK: PTO element region: predication: requires scalar integer or pointer results, f32 or typed carrier results

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x i64> @llvm.linx.experimental.element.view.v64i64(
    <64 x i64>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @reject_dense_i64(ptr %output) {
entry:
  %input = call <64 x i64> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.view = call <64 x i64> @llvm.linx.experimental.element.view.v64i64(
      <64 x i64> %input, i64 1, i64 1, i64 0, i64 512, i64 24,
      i64 32, i64 2, i64 29)
  %value = extractelement <64 x i64> %input.view, i32 %element
  %address = getelementptr i64, ptr %output, i32 %element
  store i64 %value, ptr %address, align 8
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 64
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
