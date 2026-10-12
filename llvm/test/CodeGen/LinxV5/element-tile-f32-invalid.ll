; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/vp-fast.ll 2>&1 | FileCheck %s --check-prefix=VPFAST
; VPFAST: floating instructions require no fast-math
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/vp-one.ll 2>&1 | FileCheck %s --check-prefix=VPONE
; VPONE: floating comparison predicate/type has no exact native mapping
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/unmasked.ll 2>&1 | FileCheck %s --check-prefix=UNMASKED
; UNMASKED: floating comparison requires token-owned llvm.vp.fcmp
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/vp-strict.ll 2>&1 | FileCheck %s --check-prefix=VPSTRICT
; VPSTRICT: strictfp/fenv requires constrained floating-point lowering

;--- vp-fast.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float>, <32 x float>, metadata, <32 x i1>, i32)
declare <32 x float> @llvm.vp.fadd.v32f32(<32 x float>, <32 x float>, <32 x i1>, i32)
define void @predicate_oeq() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"oeq", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %choice = call nnan <32 x float> @llvm.vp.fadd.v32f32(<32 x float> %av, <32 x float> %bv, <32 x i1> <i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true, i1 true>, i32 32), !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}

!0 = distinct !{}
!99 = !{i32 32}

;--- vp-one.ll
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
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"one", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}

!0 = distinct !{}
!99 = !{i32 32}

;--- unmasked.ll
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
  %cmp = fcmp oeq <32 x float> %av, %bv, !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}

!0 = distinct !{}
!99 = !{i32 32}

;--- vp-strict.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float>, <32 x float>, metadata, <32 x i1>, i32)
define void @predicate_oeq() strictfp {
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

!0 = distinct !{}
!99 = !{i32 32}
