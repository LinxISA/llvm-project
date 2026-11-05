; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <64 x i32> @llvm.linx.experimental.element.view.v64i32(
    <64 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @step_two_uses_native_mul_add() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !1
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(
      <64 x i32> <i32 0, i32 2, i32 4, i32 6, i32 8, i32 10, i32 12, i32 14,
                  i32 16, i32 18, i32 20, i32 22, i32 24, i32 26, i32 28, i32 30,
                  i32 32, i32 34, i32 36, i32 38, i32 40, i32 42, i32 44, i32 46,
                  i32 48, i32 50, i32 52, i32 54, i32 56, i32 58, i32 60, i32 62,
                  i32 64, i32 66, i32 68, i32 70, i32 72, i32 74, i32 76, i32 78,
                  i32 80, i32 82, i32 84, i32 86, i32 88, i32 90, i32 92, i32 94,
                  i32 96, i32 98, i32 100, i32 102, i32 104, i32 106, i32 108, i32 110,
                  i32 112, i32 114, i32 116, i32 118, i32 120, i32 122, i32 124, i32 126>,
      i64 1, i64 1, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29),
      !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x i32> %published)
  ret void
}

; CHECK-LABEL: define void @step_two_uses_native_mul_add
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 0, i64 4294967296)
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 0, i64 1)
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v64i32(i64 32, i64 2, i64 25, i64 29, i64 2,
; CHECK: ret void

define void @row16_column1_grid() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !1
  %published = call <64 x i32> @llvm.linx.experimental.element.view.v64i32(
      <64 x i32> <i32 0, i32 1, i32 16, i32 17, i32 32, i32 33, i32 48, i32 49,
                  i32 64, i32 65, i32 80, i32 81, i32 96, i32 97, i32 112, i32 113,
                  i32 128, i32 129, i32 144, i32 145, i32 160, i32 161, i32 176, i32 177,
                  i32 192, i32 193, i32 208, i32 209, i32 224, i32 225, i32 240, i32 241,
                  i32 256, i32 257, i32 272, i32 273, i32 288, i32 289, i32 304, i32 305,
                  i32 320, i32 321, i32 336, i32 337, i32 352, i32 353, i32 368, i32 369,
                  i32 384, i32 385, i32 400, i32 401, i32 416, i32 417, i32 432, i32 433,
                  i32 448, i32 449, i32 464, i32 465, i32 480, i32 481, i32 496, i32 497>,
      i64 2, i64 2, i64 0, i64 256, i64 25, i64 32, i64 2, i64 29),
      !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x i32> %published)
  ret void
}

; CHECK-LABEL: define void @row16_column1_grid
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 16, i64 0)
; CHECK: @llvm.linx.experimental.ew.tci.v64i32(i64 32, i64 2, i64 25, i64 29, i64 1, i64 0)
; CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked.v64i32(i64 32, i64 2, i64 25, i64 29, i64 2,
; CHECK: ret void

!0 = distinct !{}
!1 = !{i32 64}
!2 = distinct !{}
