; RUN: llc -mtriple=linx64v5-unknown-linux-gnu -mcpu=janus -O0 < %s | FileCheck %s
;
; The logical mask algebra is lowered before ISel. This test intentionally
; checks only that no intrinsic or SIMT register path leaks into the output.

; CHECK-LABEL: elementwise_mask_intrinsics:
; CHECK-NOT: llvm.linx.experimental.ew.mask
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

define i1 @elementwise_mask_intrinsics(i1 %seed, <4 x i1> %other) #0 {
entry:
  %s = call <4 x i1> @llvm.linx.experimental.ew.mask.splat.v4i1(i1 %seed)
  %a = call <4 x i1> @llvm.linx.experimental.ew.mask.and.v4i1(<4 x i1> %s, <4 x i1> %other)
  %n = call <4 x i1> @llvm.linx.experimental.ew.mask.andnot.v4i1(<4 x i1> %a, <4 x i1> zeroinitializer)
  %o = call <4 x i1> @llvm.linx.experimental.ew.mask.or.v4i1(<4 x i1> %n, <4 x i1> %other)
  %any = call i1 @llvm.linx.experimental.ew.mask.any.v4i1(<4 x i1> %o)
  %all = call i1 @llvm.linx.experimental.ew.mask.all.v4i1(<4 x i1> %n)
  %result = and i1 %any, %all
  ret i1 %result
}

declare <4 x i1> @llvm.linx.experimental.ew.mask.splat.v4i1(i1)
declare <4 x i1> @llvm.linx.experimental.ew.mask.and.v4i1(<4 x i1>, <4 x i1>)
declare <4 x i1> @llvm.linx.experimental.ew.mask.andnot.v4i1(<4 x i1>, <4 x i1>)
declare <4 x i1> @llvm.linx.experimental.ew.mask.or.v4i1(<4 x i1>, <4 x i1>)
declare i1 @llvm.linx.experimental.ew.mask.any.v4i1(<4 x i1>)
declare i1 @llvm.linx.experimental.ew.mask.all.v4i1(<4 x i1>)

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="4" }
