; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
;
; A native logical32 i64 arithmetic result may either remain address-only or
; publish through an exact typed Tile.  Mixing the two use classes must remain
; fail-closed rather than silently admitting arbitrary wide logical indices.
;
; CHECK: PTO element region: Tile legalization: i64 VP address arithmetic requires a constant affine result or a fully active, address-only uniform offset with signed-B32 lane offsets

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i64> @llvm.linx.experimental.element.view.v32i64(
    <32 x i64>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @reject_mixed_address_and_typed_value(ptr %base) {
entry:
  %input = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i64> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x i64> [ %initial, %entry ], [ %published, %loop ]
  %input.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %input, i64 1, i64 1, i64 0, i64 256, i64 24,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i64> %input.view, i32 %element
  %sum = add i64 %value, 1
  %address = getelementptr i64, ptr %base, i64 %sum
  %loaded = load i64, ptr %address, align 8
  %mixed = add i64 %sum, %loaded
  %inserted = insertelement <32 x i64> %carrier, i64 %mixed, i32 %element
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %inserted, i64 2, i64 2, i64 0, i64 256, i64 24,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i64> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i64> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
