; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %s 2>&1 | FileCheck %s
;
; The exact same-width i32 carrier bridge is limited to an existing Tr Tile
; producer. An ordinary vector/GPR inline-asm result must remain fail-closed.
;
; CHECK: PTO element region: Tile legalization: NORM carrier bitcast requires an external Tile producer

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <512 x float> @llvm.linx.experimental.element.view.v512f32(
    <512 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @reject_non_tile_raw_carrier() {
entry:
  %raw = call <512 x i32> asm sideeffect "", "=r"()
  %initial = call <512 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <512 x float> [ %initial, %entry ], [ %published, %loop ]
  %bits = bitcast <512 x i32> %raw to <512 x float>
  %view = call <512 x float> @llvm.linx.experimental.element.view.v512f32(
      <512 x float> %bits, i64 1, i64 1, i64 0, i64 2048, i64 1,
      i64 16, i64 32, i64 0)
  %value = extractelement <512 x float> %view, i32 %element
  %inserted = insertelement <512 x float> %carrier, float %value, i32 %element
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

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
