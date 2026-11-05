; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x float> @llvm.vp.fneg.v32f32(<32 x float>, <32 x i1>, i32)
define void @vp_sparse_fneg() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %choice = call <32 x float> @llvm.vp.fneg.v32f32(<32 x float> %av, <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}

; CHECK-LABEL: @vp_sparse_fneg
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 1431655765)
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary
; CHECK: ret void
define void @vp_empty_fneg() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %choice = call <32 x float> @llvm.vp.fneg.v32f32(<32 x float> %av, <32 x i1> zeroinitializer, i32 32), !linx.pto.element.vp.owner !2
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}


; CHECK-LABEL: @vp_empty_fneg
; CHECK: [[FALSE:%.*]] = freeze i1 false
; CHECK: [[EMPTY:%.*]] = select i1 [[FALSE]], i64 4294967295, i64 0
; CHECK: [[LOW:%.*]] = and i64 [[EMPTY]], 4294967295
; CHECK: [[ACTIVE:%.*]] = and i64 [[LOW]], 4294967295
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, i64 [[ACTIVE]])
; CHECK: ret void
!2 = distinct !{}
!0 = distinct !{}
!99 = !{i32 32}
