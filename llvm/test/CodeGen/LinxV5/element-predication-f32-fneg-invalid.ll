; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %s 2>&1 | FileCheck %s
; CHECK: floating unary requires f32 fneg without fast-math
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(
    <32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @nested_f32_fneg() {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %other = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %latch ]
  %a.view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %b.view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %other, i64 2, i64 2, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %a = extractelement <32 x float> %a.view, i32 %element
  %b = extractelement <32 x float> %b.view, i32 %element
  %positive = fcmp ogt float %a, 0.0
  br i1 %positive, label %positive.arm, label %negative.arm
positive.arm:
  %stable = freeze float %b
  %negated = fneg nnan float %stable
  %equal = fcmp oeq float %a, %stable
  br i1 %equal, label %equal.arm, label %different.arm
equal.arm:
  %sum = fadd float %a, %stable
  br label %merge
different.arm:
  %difference = fadd float %a, %negated
  br label %merge
negative.arm:
  %negative.zero = select i1 true, float -0.0, float 0.0
  br label %merge
merge:
  %result = phi float [ %sum, %equal.arm ], [ %difference, %different.arm ],
                       [ %negative.zero, %negative.arm ]
  %updated = insertelement <32 x float> %carrier, float %result, i32 %element
  br label %latch
latch:
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %updated, i64 3, i64 3, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
