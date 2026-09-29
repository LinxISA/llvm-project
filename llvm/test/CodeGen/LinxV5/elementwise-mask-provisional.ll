; RUN: llc -mtriple=linx64v5-unknown-linux-gnu -mcpu=janus -O0 < %s | FileCheck %s
;
; Provisional element-wise control-flow lowering. The logical active mask is
; represented in LLVM SSA and is not yet bound to PTO hardware operands.

; CHECK-LABEL: elementwise_mask_provisional:
; CHECK-NOT: llvm.linx.experimental.ew.mask
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

define i1 @elementwise_mask_provisional(i1 %condition) #0 {
entry:
  %active = call <4 x i1> @llvm.linx.experimental.ew.mask.splat.v4i1(i1 true)
  %predicate = call <4 x i1> @llvm.linx.experimental.ew.mask.splat.v4i1(i1 %condition)
  %then = call <4 x i1> @llvm.linx.experimental.ew.mask.and.v4i1(
      <4 x i1> %active, <4 x i1> %predicate)
  %else = call <4 x i1> @llvm.linx.experimental.ew.mask.andnot.v4i1(
      <4 x i1> %active, <4 x i1> %predicate)
  %merge = call <4 x i1> @llvm.linx.experimental.ew.mask.or.v4i1(
      <4 x i1> %then, <4 x i1> %else)
  %selected = call <4 x i1> @llvm.linx.experimental.ew.mask.select.v4i1(
      <4 x i1> %then, <4 x i1> %else, <4 x i1> %predicate)
  %any = call i1 @llvm.linx.experimental.ew.mask.any.v4i1(<4 x i1> %merge)
  %all = call i1 @llvm.linx.experimental.ew.mask.all.v4i1(<4 x i1> %selected)
  %result = and i1 %any, %all
  ret i1 %result
}

declare <4 x i1> @llvm.linx.experimental.ew.mask.splat.v4i1(i1)
declare <4 x i1> @llvm.linx.experimental.ew.mask.and.v4i1(<4 x i1>, <4 x i1>)
declare <4 x i1> @llvm.linx.experimental.ew.mask.andnot.v4i1(<4 x i1>, <4 x i1>)
declare <4 x i1> @llvm.linx.experimental.ew.mask.or.v4i1(<4 x i1>, <4 x i1>)
declare <4 x i1> @llvm.linx.experimental.ew.mask.select.v4i1(<4 x i1>, <4 x i1>, <4 x i1>)
declare i1 @llvm.linx.experimental.ew.mask.any.v4i1(<4 x i1>)
declare i1 @llvm.linx.experimental.ew.mask.all.v4i1(<4 x i1>)

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="4" }
