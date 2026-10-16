; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/wrapped.ll 2>&1 | FileCheck %s --check-prefix=WRAPPED
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/f32.ll 2>&1 | FileCheck %s --check-prefix=F32
; A malicious descriptor must not wrap rows/columns through unsigned narrowing.

;--- wrapped.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x half> @llvm.linx.experimental.element.view.v64f16(
    <64 x half>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @wrapped_shape_is_not_32x2() {
entry:
  %input = call <64 x half> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %bad = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> %input, i64 1, i64 1, i64 0, i64 128, i64 4,
      i64 4294967328, i64 4294967298, i64 29),
      !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %bad)
  ret void
}

; WRAPPED: PTO element region: Tile legalization:

!0 = distinct !{}
!1 = !{i32 64}

;--- f32.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x float> @llvm.linx.experimental.element.view.v64f32(
    <64 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @forged_f32_logical64() {
entry:
  %input = call <64 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %bad = call <64 x float> @llvm.linx.experimental.element.view.v64f32(
      <64 x float> %input, i64 1, i64 1, i64 0, i64 256, i64 1,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x float> %bad)
  ret void
}

; F32: PTO element region: Tile legalization:

!0 = distinct !{}
!1 = !{i32 64}
