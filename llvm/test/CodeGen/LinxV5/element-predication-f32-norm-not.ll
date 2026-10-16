; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
;
; FP32 NORM predicate inversion is packed predicate algebra, not numeric FMUL.
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <512 x float> @llvm.linx.experimental.element.view.v512f32(
    <512 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @f32_norm_predicate_not() {
entry:
  %input = call <512 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <512 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <512 x float> [ %initial, %entry ], [ %published, %loop ]
  %input.view = call <512 x float> @llvm.linx.experimental.element.view.v512f32(
      <512 x float> %input, i64 1, i64 1, i64 0, i64 2048, i64 1,
      i64 16, i64 32, i64 0)
  %value = extractelement <512 x float> %input.view, i32 %element
  %positive = fcmp ogt float %value, 0.0
  %not = xor i1 %positive, true
  %result = select i1 %not, float 11.0, float 22.0
  %inserted = insertelement <512 x float> %carrier, float %result, i32 %element
  %published = call <512 x float> @llvm.linx.experimental.element.view.v512f32(
      <512 x float> %inserted, i64 2, i64 2, i64 0, i64 2048, i64 1,
      i64 16, i64 32, i64 0)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 512
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  %final = phi <512 x float> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<512 x float> %final)
  ret void
}

; CHECK-LABEL: define void @f32_norm_predicate_not
; CHECK: @llvm.linx.experimental.ew.tcmp.row
; CHECK: @llvm.linx.experimental.ew.tsel.row
; CHECK: @llvm.linx.experimental.ew.tcmp.row
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary
; CHECK: @llvm.linx.experimental.ew.tsel.row
; CHECK: call void asm sideeffect "opaque.tile.consumer $0"

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
