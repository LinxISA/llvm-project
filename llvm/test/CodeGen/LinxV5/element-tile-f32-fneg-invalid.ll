; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/evl.ll 2>&1 | FileCheck %s --check-prefix=EVL
; EVL: requires a 32-bit mask and constant EVL=32
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/fmf.ll 2>&1 | FileCheck %s --check-prefix=FMF
; FMF: floating instructions require no fast-math

;--- evl.ll
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
  %choice = call <32 x float> @llvm.vp.fneg.v32f32(<32 x float> %av, <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 17), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
!0 = distinct !{}
!99 = !{i32 32}

;--- fmf.ll
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
  %choice = call nnan <32 x float> @llvm.vp.fneg.v32f32(<32 x float> %av, <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
!0 = distinct !{}
!99 = !{i32 32}
