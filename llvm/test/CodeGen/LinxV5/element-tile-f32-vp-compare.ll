; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | not grep -E 'call .*element\.(region|view)|call .*llvm.vp.|fcmp '
;
; This direct native mask-flow gate makes no source fenv/order claim.
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float>, <32 x float>, metadata, <32 x i1>, i32)

define void @predicate_oeq() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"oeq", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_oeq
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 0, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @predicate_une() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"une", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !2
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !2
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_une
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 1, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @predicate_olt() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !4), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !4
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !4
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"olt", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !4
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !4
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !4
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_olt
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 2, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @predicate_ogt() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !6), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !6
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !6
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"ogt", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !6
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !6
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !6
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_ogt
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 3, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @predicate_ole() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !8), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !8
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !8
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"ole", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !8
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !8
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !8
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_ole
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 4, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @predicate_oge() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !10), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !10
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !10
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"oge", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !10
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !10
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !10
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_oge
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 5, i64 1431655765)
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: pto.index.typed
; CHECK: ret void

define void @scalar_left_raw_select() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !12), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !12
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> <float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0>, <32 x float> %av, metadata !"olt", <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !12
  %choice = select <32 x i1> %cmp, <32 x float> <float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0, float -0.0>, <32 x float> <float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000, float 0x7FF8000020000000>, !linx.pto.element.vp.owner !12
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !12
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @scalar_left_raw_select
; CHECK: [[SCALAR:%.*]] = freeze float -0.000000e+00
; CHECK: [[BITS:%.*]] = bitcast float [[SCALAR]] to i32
; CHECK: [[RAW:%.*]] = zext i32 [[BITS]] to i64
; CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 [[RAW]], i64 3, i64 4294967295)
; CHECK-DAG: [[NEGZERO:%.*]] = freeze float -0.000000e+00
; CHECK-DAG: [[QNAN:%.*]] = freeze float 0x7FF8000020000000
; CHECK-DAG: @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[NEGZERO]])
; CHECK-DAG: @llvm.linx.experimental.ew.texpands.v32f32.f32(i64 32, i64 1, i64 1, i64 29, float [[QNAN]])
; CHECK: @llvm.linx.experimental.ew.tsel.gpr.v32f32(i64 32, i64 1, i64 1, i64 29,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary
; CHECK: ret void

!0 = distinct !{}
!2 = distinct !{}
!4 = distinct !{}
!6 = distinct !{}
!8 = distinct !{}
!10 = distinct !{}
!99 = !{i32 32}
!12 = distinct !{}
