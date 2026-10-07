; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-region' -S %s | FileCheck %s

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @expression_chain(<32 x i32> %input, i32 %bias,
                                    i32 %multiplier) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %output = phi <32 x i32> [ poison, %entry ], [ %published, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %biased = add i32 %value, %bias
  %multiplied = mul i32 %biased, %multiplier
  %reduced = sub i32 %multiplied, 7
  %divided = udiv i32 %reduced, 5
  %remainder = urem i32 %divided, 251
  %shifted.left = shl i32 %remainder, 3
  %shifted.right = lshr i32 %shifted.left, 2
  %masked = and i32 %shifted.right, 4294967280
  %merged = or i32 %masked, 17
  %mixed = xor i32 %merged, %shifted.left
  %negated = sub i32 0, %mixed
  %inverted = xor i32 %negated, -1
  %inserted = insertelement <32 x i32> %output, i32 %inverted, i32 %element
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %latch

latch:
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  ret <32 x i32> %result
}

; CHECK-LABEL: define <32 x i32> @expression_chain
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 0,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 2,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 1,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 3,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 4,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 8,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 9,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 5,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 6,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 7,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 1,
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 7,
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement

!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.mustprogress"}
!3 = !{!"llvm.loop.unroll.disable"}
!4 = !{!"llvm.loop.linx.pto.element.region", !0}
