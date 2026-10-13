; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | FileCheck %s

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare i32 @llvm.fshl.i32(i32, i32, i32)

define void @signed_carrier_fshl(i32 %shift) {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %rotated = call i32 @llvm.fshl.i32(i32 %value, i32 %value, i32 %shift)
  %insert = insertelement <32 x i32> %carrier, i32 %rotated, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @signed_carrier_fshl(
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 8,
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 9,
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 6,
; CHECK-NOT: @llvm.fshl

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
