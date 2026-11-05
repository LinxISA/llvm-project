; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=VP
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
;
; LLVM canonicalizes ordered >=/<= CFG into unordered relational complements
; with reversed arms.  Lower those exact complements through the opposite
; native ordered comparison and invert only within the inherited active mask.
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(
    <32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @nested_ult() {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %latch ]
  %view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x float> %view, i32 %element
  %outer = fcmp ogt float %value, -1.0
  br i1 %outer, label %active, label %inactive
active:
  %complement = fcmp ult float %value, 0.0
  br i1 %complement, label %eleven, label %twenty.two
eleven:
  br label %merge
twenty.two:
  br label %merge
inactive:
  br label %merge
merge:
  %result = phi float [ 11.0, %eleven ], [ 22.0, %twenty.two ],
                      [ 33.0, %inactive ]
  %updated = insertelement <32 x float> %carrier, float %result, i32 %element
  br label %latch
latch:
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %updated, i64 2, i64 2, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}

; VP-LABEL: define void @nested_ult
; VP: [[OUTER:%.*]] = call <32 x i1> @llvm.vp.fcmp.v32f32({{.*}}metadata !"ogt", <32 x i1> {{.*}}, i32 32)
; VP: [[INNER:%.*]] = call <32 x i1> @llvm.vp.fcmp.v32f32({{.*}}metadata !"ult", <32 x i1> [[OUTER]], i32 32)
; TILE-LABEL: define void @nested_ult
; TILE: [[OUTER:%.*]] = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32({{.*}}i64 3, i64 {{.*}})
; TILE: [[OUTER_PROFILE:%.*]] = and i64 [[OUTER]], 4294967295
; TILE: [[INNER_ACTIVE:%.*]] = and i64 [[OUTER_PROFILE]], 4294967295
; TILE: [[INNER:%.*]] = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32({{.*}}i64 5, i64 [[INNER_ACTIVE]])
; TILE: [[INVERSE:%.*]] = xor i64 [[INNER]], [[INNER_ACTIVE]]
; TILE: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
