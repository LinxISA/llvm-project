; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=VP
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | not grep -E 'call .*element\.(region|view)|call .*llvm.vp.|extractelement|insertelement|fadd |fsub |fneg |fcmp '
;
; Ordinary scalar CFG: the inner sNaN-capable comparison inherits its source
; block mask. The output is a three-way PHI, with one shared freeze feeding
; add/sub and a native raw-bit publication (no numeric identity conversion).
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(
    <32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @nested_f32_fneg() {
entry:
  %input = call <32 x float> asm sideeffect "", "=@2Tr"()
  %other = call <32 x float> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x float> [ %initial, %entry ], [ %published, %latch ]
  %a.view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %input, i64 1, i64 1, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %b.view = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %other, i64 2, i64 2, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %a = extractelement <32 x float> %a.view, i32 %element
  %b = extractelement <32 x float> %b.view, i32 %element
  %positive = fcmp ogt float %a, 0.0
  br i1 %positive, label %positive.arm, label %negative.arm
positive.arm:
  %stable = freeze float %b
  %negated = fneg float %stable
  %equal = fcmp oeq float %a, %stable
  br i1 %equal, label %equal.arm, label %different.arm
equal.arm:
  %sum = fadd float %a, %stable
  br label %merge
different.arm:
  %difference = fadd float %a, %negated
  br label %merge
negative.arm:
  %negative.zero = select i1 true, float -0.0, float 0.0
  br label %merge
merge:
  %result = phi float [ %sum, %equal.arm ], [ %difference, %different.arm ],
                       [ %negative.zero, %negative.arm ]
  %updated = insertelement <32 x float> %carrier, float %result, i32 %element
  br label %latch
latch:
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(
      <32 x float> %updated, i64 3, i64 3, i64 0, i64 128, i64 1,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1
exit:
  %final = phi <32 x float> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %final)
  ret void
}

; VP-LABEL: define void @nested_f32_fneg
; VP: [[OUTER:%.*]] = call <32 x i1> @llvm.vp.fcmp.v32f32({{.*}}metadata !"ogt", <32 x i1> {{.*}}, i32 32)
; VP: [[FROZEN:%.*]] = freeze <32 x float>
; VP: [[NEGATED:%.*]] = call <32 x float> @llvm.vp.fneg.v32f32(<32 x float> [[FROZEN]], <32 x i1> [[OUTER]], i32 32)
; VP: [[INNER:%.*]] = call <32 x i1> @llvm.vp.fcmp.v32f32({{.*}}<32 x float> [[FROZEN]], metadata !"oeq", <32 x i1> [[OUTER]], i32 32)
; VP: call <32 x float> @llvm.vp.fadd.v32f32({{.*}}<32 x float> [[FROZEN]], <32 x i1>
; VP: call <32 x float> @llvm.vp.fadd.v32f32({{.*}}<32 x float> [[NEGATED]], <32 x i1>
; VP: select <32 x i1>
; VP: ret void
; TILE-LABEL: define void @nested_f32_fneg
; TILE: [[A:%.*]] = call <32 x float> asm sideeffect "", "=@2Tr"()
; TILE: [[B:%.*]] = call <32 x float> asm sideeffect "", "=@2Tr"()
; TILE-DAG: [[TRUE:%.*]] = freeze i1 true
; TILE-DAG: [[FULL:%.*]] = select i1 [[TRUE]], i64 4294967295, i64 0
; TILE-DAG: [[FULL_LOW:%.*]] = and i64 [[FULL]], 4294967295
; TILE-DAG: [[FULL_ACTIVE:%.*]] = and i64 [[FULL_LOW]], 4294967295
; TILE-DAG: [[OUTER_MASK:%.*]] = call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> [[A]], i64 {{.*}}, i64 3, i64 [[FULL_ACTIVE]])
; TILE-DAG: [[LOW_OUTER:%.*]] = and i64 [[OUTER_MASK]], 4294967295
; TILE-DAG: [[ACTIVE:%.*]] = and i64 [[LOW_OUTER]], 4294967295
; TILE-DAG: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> [[A]], <32 x float> [[B]], i64 0, i64 [[ACTIVE]])
; TILE-DAG: call <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, i64 0, <32 x float> [[A]], <32 x float> [[B]], i64 {{.*}}, i64 0, i64 0, i64 1)
; TILE-DAG: [[NEG_ACTIVE:%.*]] = and i64 [[LOW_OUTER]], 4294967295
; TILE-DAG: [[NEGTILE:%.*]] = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> [[B]], i64 [[NEG_ACTIVE]])
; TILE-DAG: call <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, i64 0, <32 x float> [[A]], <32 x float> [[NEGTILE]], i64 {{.*}}, i64 0, i64 0, i64 1)
; TILE-DAG: [[NEGZERO:%.*]] = freeze float -0.000000e+00
; TILE-DAG: call <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[NEGZERO]])
; TILE-NOT: pto.index.typed
; TILE: call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float>
; TILE: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
