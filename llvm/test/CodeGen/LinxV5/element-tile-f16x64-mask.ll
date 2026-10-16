; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | not grep -E 'call .*element\.(region|view)|call .*llvm.vp.'

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x half> @llvm.linx.experimental.element.view.v64f16(
    <64 x half>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x i16> @llvm.linx.experimental.element.view.v64i16(
    <64 x i16>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x i32> @llvm.linx.experimental.element.view.v64i32(
    <64 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x half> @llvm.vp.fadd.v64f16(
    <64 x half>, <64 x half>, <64 x i1>, i32)

define void @even_logical_lanes_map_to_low_predicate_word() {
entry:
  %a = call <64 x half> asm sideeffect "", "=@2Tr"()
  %b = call <64 x half> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !99
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  %sum = call <64 x half> @llvm.vp.fadd.v64f16(
      <64 x half> %av, <64 x half> %bv,
      <64 x i1> <i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false,
                  i1 true, i1 false, i1 true, i1 false>, i32 64),
      !linx.pto.element.vp.owner !0
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(
      <64 x half> %sum, i64 3, i64 3, i64 0, i64 128, i64 4,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}

; CHECK-LABEL: define void @even_logical_lanes_map_to_low_predicate_word
; CHECK: call <64 x half> @llvm.linx.experimental.ew.tbinary.gpr.masked.v64f16(i64 32, i64 2, i64 4, i64 29, i64 0, <64 x half> %a, <64 x half> %b, i64 4294967295, i64 0, i64 0, i64 1)
; CHECK: ret void

define void @descending_logical_indices() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !99
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(
      <64 x i32> <i32 63, i32 62, i32 61, i32 60, i32 59, i32 58, i32 57, i32 56,
                  i32 55, i32 54, i32 53, i32 52, i32 51, i32 50, i32 49, i32 48,
                  i32 47, i32 46, i32 45, i32 44, i32 43, i32 42, i32 41, i32 40,
                  i32 39, i32 38, i32 37, i32 36, i32 35, i32 34, i32 33, i32 32,
                  i32 31, i32 30, i32 29, i32 28, i32 27, i32 26, i32 25, i32 24,
                  i32 23, i32 22, i32 21, i32 20, i32 19, i32 18, i32 17, i32 16,
                  i32 15, i32 14, i32 13, i32 12, i32 11, i32 10, i32 9, i32 8,
                  i32 7, i32 6, i32 5, i32 4, i32 3, i32 2, i32 1, i32 0>,
      i64 7, i64 7, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29),
      !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x i32> %published)
  ret void
}

; CHECK-LABEL: define void @descending_logical_indices
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 0, i64 4294967296)
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 0, i64 1)
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 4294967294, i64 0)
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 4294967295, i64 0)
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v64i32(i64 32, i64 2, i64 25, i64 29, i64 2,
; CHECK: ret void

define void @u16_logical_select() {
entry:
  %a = call <64 x i16> asm sideeffect "", "=@2Tr"()
  %b = call <64 x i16> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !1), !linx.pto.element.vp.domain !99
  %av = call <64 x i16> @llvm.linx.experimental.element.view.v64i16(
      <64 x i16> %a, i64 4, i64 4, i64 0, i64 128, i64 26,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !1
  %bv = call <64 x i16> @llvm.linx.experimental.element.view.v64i16(
      <64 x i16> %b, i64 5, i64 5, i64 0, i64 128, i64 26,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !1
  %choice = select <64 x i1>
      <i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false,
       i1 true, i1 false, i1 true, i1 false>,
      <64 x i16> %av, <64 x i16> %bv, !linx.pto.element.vp.owner !1
  %published = call <64 x i16> @llvm.linx.experimental.element.view.v64i16(
      <64 x i16> %choice, i64 6, i64 6, i64 0, i64 128, i64 26,
      i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !1
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x i16> %published)
  ret void
}

; CHECK-LABEL: define void @u16_logical_select
; CHECK: call <64 x i16> @llvm.linx.experimental.ew.tsel.gpr.v64i16(i64 32, i64 2, i64 26, i64 29, i64 4294967295, i64 0, <64 x i16> %a, <64 x i16> %b)
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{}
!2 = distinct !{}
!99 = !{i32 64}
