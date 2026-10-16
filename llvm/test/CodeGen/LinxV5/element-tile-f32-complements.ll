; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
;
; Direct VP consumers cover every exact unordered relational complement.  The
; alternating active mask proves inversion is limited to active lanes.
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float>, <32 x float>, metadata, <32 x i1>, i32)
declare <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <64 x i1> @llvm.vp.fcmp.v64f16(<64 x half>, <64 x half>, metadata, <64 x i1>, i32)

define void @predicate_ult() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"ult", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !0
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !0
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_ult
; CHECK: [[CMP:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32({{.*}}i64 5, i64 1431655765)
; CHECK: [[INV:%.*]] = xor i64 [[CMP]], 1431655765
; CHECK: [[INV_PROFILE:%.*]] = and i64 [[INV]], 4294967295
; CHECK: call <32 x float> @llvm.linx.experimental.ew.tsel.gpr.v32f32({{.*}}i64 [[INV_PROFILE]]

define void @predicate_ugt() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !1), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !1
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !1
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"ugt", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !1
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !1
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !1
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_ugt
; CHECK: [[CMP:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32({{.*}}i64 4, i64 1431655765)
; CHECK: [[INV:%.*]] = xor i64 [[CMP]], 1431655765

define void @predicate_ule() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !2), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"ule", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !2
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !2
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !2
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_ule
; CHECK: [[CMP:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32({{.*}}i64 3, i64 1431655765)
; CHECK: [[INV:%.*]] = xor i64 [[CMP]], 1431655765

define void @predicate_uge() {
entry:
  %a = call <32 x float> asm sideeffect "", "=@2Tr"()
  %b = call <32 x float> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3), !linx.pto.element.vp.domain !99
  %av = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %a, i64 1, i64 1, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !3
  %bv = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %b, i64 2, i64 2, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !3
  %cmp = call <32 x i1> @llvm.vp.fcmp.v32f32(<32 x float> %av, <32 x float> %bv, metadata !"uge", <32 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 32), !linx.pto.element.vp.owner !3
  %choice = select <32 x i1> %cmp, <32 x float> %av, <32 x float> %bv, !linx.pto.element.vp.owner !3
  %published = call <32 x float> @llvm.linx.experimental.element.view.v32f32(<32 x float> %choice, i64 3, i64 3, i64 0, i64 128, i64 1, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !3
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x float> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_uge
; CHECK: [[CMP:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32({{.*}}i64 2, i64 1431655765)
; CHECK: [[INV:%.*]] = xor i64 [[CMP]], 1431655765

define void @predicate_f16x64_ult_partial() {
entry:
  %a = call <64 x half> asm sideeffect "", "=@2Tr"()
  %b = call <64 x half> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !4), !linx.pto.element.vp.domain !100
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !4
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !4
  %cmp = call <64 x i1> @llvm.vp.fcmp.v64f16(<64 x half> %av, <64 x half> %bv, metadata !"ult", <64 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 37), !linx.pto.element.vp.owner !4
  %full = call <64 x i1> @llvm.vp.fcmp.v64f16(<64 x half> %av, <64 x half> %bv, metadata !"ugt", <64 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, i32 64), !linx.pto.element.vp.owner !4
  %combined = xor <64 x i1> %cmp, %full, !linx.pto.element.vp.owner !4
  %choice = select <64 x i1> %combined, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !4
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %choice, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !4
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_f16x64_ult_partial
; CHECK: [[CMP:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v64f16({{.*}}i64 5, i64 524287)
; CHECK: [[INV:%.*]] = xor i64 [[CMP]], 524287
; CHECK: [[INV_PROFILE:%.*]] = and i64 [[INV]], -1
; CHECK: [[FULL:%.*]] = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v64f16({{.*}}i64 4, i64 4294967295)
; CHECK: [[FULL_INV:%.*]] = xor i64 [[FULL]], 4294967295
; CHECK: [[FULL_PROFILE:%.*]] = and i64 [[FULL_INV]], -1
; CHECK: [[COMBINED:%.*]] = xor i64 [[INV_PROFILE]], [[FULL_PROFILE]]
; CHECK: [[COMBINED_PROFILE:%.*]] = and i64 [[COMBINED]], -1
; CHECK: call <64 x half> @llvm.linx.experimental.ew.tsel.gpr.v64f16({{.*}}i64 [[COMBINED_PROFILE]]

define void @predicate_f16x64_full_then_partial() {
entry:
  %a = call <64 x half> asm sideeffect "", "=@2Tr"()
  %b = call <64 x half> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !5), !linx.pto.element.vp.domain !100
  %av = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %a, i64 1, i64 1, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !5
  %bv = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %b, i64 2, i64 2, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !5
  %mask = icmp eq <64 x i32> zeroinitializer, zeroinitializer, !linx.pto.element.vp.owner !5
  %full = call <64 x i1> @llvm.vp.fcmp.v64f16(<64 x half> %av, <64 x half> %bv, metadata !"ugt", <64 x i1> %mask, i32 64), !linx.pto.element.vp.owner !5
  %partial = call <64 x i1> @llvm.vp.fcmp.v64f16(<64 x half> %av, <64 x half> %bv, metadata !"ult", <64 x i1> %mask, i32 37), !linx.pto.element.vp.owner !5
  %combined = xor <64 x i1> %full, %partial, !linx.pto.element.vp.owner !5
  %choice = select <64 x i1> %combined, <64 x half> %av, <64 x half> %bv, !linx.pto.element.vp.owner !5
  %published = call <64 x half> @llvm.linx.experimental.element.view.v64f16(<64 x half> %choice, i64 3, i64 3, i64 0, i64 128, i64 4, i64 32, i64 2, i64 29), !linx.pto.element.vp.owner !5
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<64 x half> %published)
  ret void
}
; CHECK-LABEL: define void @predicate_f16x64_full_then_partial
; CHECK: [[MASK:%.*]] = call i64 @llvm.linx.experimental.ew.tcmps.gpr.v64i32
; CHECK: [[MASK_PROFILE:%.*]] = and i64 [[MASK]], -1
; CHECK: [[FULL_ACTIVE:%.*]] = and i64 [[MASK_PROFILE]], -1
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v64f16({{.*}}i64 4, i64 [[FULL_ACTIVE]])
; CHECK: [[PARTIAL_ACTIVE:%.*]] = and i64 [[MASK_PROFILE]], 1125895612399615
; CHECK: call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v64f16({{.*}}i64 5, i64 [[PARTIAL_ACTIVE]])
; CHECK: call <64 x half> @llvm.linx.experimental.ew.tsel.gpr.v64f16

!0 = distinct !{}
!1 = distinct !{}
!2 = distinct !{}
!3 = distinct !{}
!4 = distinct !{}
!5 = distinct !{}
!99 = !{i32 32}
!100 = !{i32 64}
