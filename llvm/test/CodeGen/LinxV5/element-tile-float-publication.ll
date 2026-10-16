; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; Floating view publications must retain FP16/FP32 dtype when materializing
; an undefined seed or a constant. No numerical add-zero identity is allowed.
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

; CHECK-LABEL: define void @half_undef
; CHECK: [[VALUE1:%.*]] = freeze half 0xH0000
; CHECK: call <64 x half> @llvm.linx.experimental.ew.texpands.v64f16.f16(i64 32, i64 2, i64 4, i64 29, half [[VALUE1]])
; CHECK-NOT: tbinary
; CHECK: ret void
define void @half_undef() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !90
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> undef, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}
!0 = distinct !{}
!90 = !{i32 64}

; CHECK-LABEL: define void @half_poison
; CHECK: [[VALUE2:%.*]] = freeze half 0xH0000
; CHECK: call <64 x half> @llvm.linx.experimental.ew.texpands.v64f16.f16(i64 32, i64 2, i64 4, i64 29, half [[VALUE2]])
; CHECK-NOT: tbinary
; CHECK: ret void
define void @half_poison() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !1), !linx.pto.element.vp.domain !91
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> poison, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !1
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}
!1 = distinct !{}
!91 = !{i32 64}

; CHECK-LABEL: define void @half_negative_zero
; CHECK: [[VALUE3:%.*]] = freeze half 0xH8000
; CHECK: call <64 x half> @llvm.linx.experimental.ew.texpands.v64f16.f16(i64 32, i64 2, i64 4, i64 29, half [[VALUE3]])
; CHECK-NOT: tbinary
; CHECK: ret void
define void @half_negative_zero() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !92
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> <half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000, half 0xH8000>, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}
!2 = distinct !{}
!92 = !{i32 64}

; CHECK-LABEL: define void @float_undef
; CHECK: [[VALUE4:%.*]] = freeze float 0.000000e+00
; CHECK: call <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[VALUE4]])
; CHECK-NOT: tbinary
; CHECK: ret void
define void @float_undef() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !3), !linx.pto.element.vp.domain !93
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> undef, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !3
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
!3 = distinct !{}
!93 = !{i32 32}
