; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/missing-order.ll 2>&1 | FileCheck %s --check-prefix=ORDER
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/alias.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/orphan.ll 2>&1 | FileCheck %s --check-prefix=OWNER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/missing-effect-contract.ll 2>&1 | FileCheck %s --check-prefix=EFFECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/wrong-evl.ll 2>&1 | FileCheck %s --check-prefix=EFFECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/wrong-type.ll 2>&1 | FileCheck %s --check-prefix=EFFECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/wrong-alignment.ll 2>&1 | FileCheck %s --check-prefix=EFFECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/domain32-v64.ll 2>&1 | FileCheck %s --check-prefix=DOMAIN
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/domain64-v32.ll 2>&1 | FileCheck %s --check-prefix=DOMAIN
; RUN: not opt -passes=verify -disable-output %t/wrong-mask.ll 2>&1 | FileCheck %s --check-prefix=MASK
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region-verify' -disable-output %t/residual.ll 2>&1 | FileCheck %s --check-prefix=RESIDUAL
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -disable-output %t/malformed-duplicate.ll 2>&1 | FileCheck %s --check-prefix=ORDER
; RUN: sed 's/!5 =.*/!5 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered", !"extra"}/' %t/malformed-duplicate.ll > %t/malformed-extra.ll
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -disable-output %t/malformed-extra.ll 2>&1 | FileCheck %s --check-prefix=ORDER
;
; ORDER: predication: unordered scatter requires the unordered/source loop contract
; ALIAS: predication: memory streams may overlap across elements
; OWNER: PTO element region: Tile legalization: scatter effect lacks region ownership
; EFFECT: PTO element region: Tile legalization: unordered scatter requires an exact owned 32-lane i32/f32 or 64-lane i16/f16/i32 profile, explicit GEP, natural alignment, matching i1 mask/EVL and source contract
; RESIDUAL: PTO element region: required region lowering did not complete
; MASK: Invalid user of intrinsic instruction!
; DOMAIN: PTO element region: Tile legalization: unordered scatter width must match its owning element domain

;--- missing-order.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @missing_order(ptr noalias %output, i32 %seed) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %e = phi i32 [ 0, %entry ], [ %next, %loop ]
  %index = xor i32 %e, %seed
  %destination = getelementptr float, ptr %output, i32 %index
  store float 1.000000e+00, ptr %destination, align 4
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- alias.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @alias(ptr %data, i32 %seed) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %e = phi i32 [ 0, %entry ], [ %next, %loop ]
  %source.index = xor i32 %e, %seed
  %source = getelementptr float, ptr %data, i32 %source.index
  %value = load float, ptr %source, align 4
  %destination.index = add i32 %source.index, 1
  %destination = getelementptr float, ptr %data, i32 %destination.index
  store float %value, ptr %destination, align 4
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}

;--- orphan.ll
declare void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
    <32 x float>, <32 x ptr>, <32 x i1>, i32, metadata)
define void @orphan(ptr %base) {
entry:
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison,
                         <32 x i32> zeroinitializer
  %addresses = getelementptr float, <32 x ptr> %bases,
               <32 x i32> zeroinitializer
  call void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
      <32 x float> zeroinitializer, <32 x ptr> align 4 %addresses,
      <32 x i1> zeroinitializer, i32 32, metadata !0)
  ret void
}
!0 = distinct !{}

;--- missing-effect-contract.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
    <32 x float>, <32 x ptr>, <32 x i1>, i32, metadata)
define void @missing_effect_contract(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison,
                         <32 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr float, <32 x ptr> %bases,
               <32 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
      <32 x float> zeroinitializer, <32 x ptr> align 4 %addresses,
      <32 x i1> zeroinitializer, i32 32, metadata !0),
      !linx.pto.element.vp.owner !0
  ret void
}
!0 = distinct !{}
!1 = !{i32 32}

;--- residual.ll
declare void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
    <32 x float>, <32 x ptr>, <32 x i1>, i32, metadata)
define void @residual(ptr %base) {
entry:
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison,
                         <32 x i32> zeroinitializer
  %addresses = getelementptr float, <32 x ptr> %bases,
               <32 x i32> zeroinitializer
  call void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
      <32 x float> zeroinitializer, <32 x ptr> align 4 %addresses,
      <32 x i1> zeroinitializer, i32 32, metadata !0)
  ret void
}
!0 = distinct !{}

;--- malformed-duplicate.ll
declare void @llvm.linx.experimental.element.region(metadata)
define void @malformed_duplicate(ptr noalias %output, i32 %seed) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %e = phi i32 [ 0, %entry ], [ %next, %loop ]
  %index = xor i32 %e, %seed
  %destination = getelementptr float, ptr %output, i32 %index
  store float 1.000000e+00, ptr %destination, align 4
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4, !5}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
!5 = !{!"llvm.loop.linx.pto.element.inter_element_order"}

;--- wrong-evl.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
    <64 x half>, <64 x ptr>, <64 x i1>, i32, metadata)
define void @wrong_evl(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <64 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <64 x ptr> %one, <64 x ptr> poison,
                         <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr half, <64 x ptr> %bases,
               <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
      <64 x half> zeroinitializer, <64 x ptr> align 2 %addresses,
      <64 x i1> zeroinitializer, i32 32, metadata !0),
      !linx.pto.element.vp.owner !0,
      !llvm.loop.linx.pto.element.inter_element_order !2,
      !llvm.loop.linx.pto.element.intra_element_order !3
  ret void
}
!0 = distinct !{}
!1 = !{i32 64}
!2 = !{!"unordered"}
!3 = !{!"source"}

;--- wrong-type.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v64f32.v64p0(
    <64 x float>, <64 x ptr>, <64 x i1>, i32, metadata)
define void @wrong_type(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <64 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <64 x ptr> %one, <64 x ptr> poison,
                         <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr float, <64 x ptr> %bases,
               <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v64f32.v64p0(
      <64 x float> zeroinitializer, <64 x ptr> align 4 %addresses,
      <64 x i1> zeroinitializer, i32 64, metadata !0),
      !linx.pto.element.vp.owner !0,
      !llvm.loop.linx.pto.element.inter_element_order !2,
      !llvm.loop.linx.pto.element.intra_element_order !3
  ret void
}
!0 = distinct !{}
!1 = !{i32 64}
!2 = !{!"unordered"}
!3 = !{!"source"}

;--- wrong-alignment.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
    <64 x half>, <64 x ptr>, <64 x i1>, i32, metadata)
define void @wrong_alignment(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <64 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <64 x ptr> %one, <64 x ptr> poison,
                         <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr half, <64 x ptr> %bases,
               <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
      <64 x half> zeroinitializer, <64 x ptr> align 1 %addresses,
      <64 x i1> zeroinitializer, i32 64, metadata !0),
      !linx.pto.element.vp.owner !0,
      !llvm.loop.linx.pto.element.inter_element_order !2,
      !llvm.loop.linx.pto.element.intra_element_order !3
  ret void
}
!0 = distinct !{}
!1 = !{i32 64}
!2 = !{!"unordered"}
!3 = !{!"source"}

;--- wrong-mask.ll
declare void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
    <64 x half>, <64 x ptr>, <64 x i1>, i32, metadata)
define void @wrong_mask(<64 x ptr> %addresses) {
entry:
  call void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
      <64 x half> zeroinitializer, <64 x ptr> align 2 %addresses,
      <32 x i1> zeroinitializer, i32 64, metadata !0)
  ret void
}
!0 = distinct !{}

;--- domain32-v64.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
    <64 x half>, <64 x ptr>, <64 x i1>, i32, metadata)
define void @domain32_v64(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <64 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <64 x ptr> %one, <64 x ptr> poison,
                         <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr half, <64 x ptr> %bases,
               <64 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v64f16.v64p0(
      <64 x half> zeroinitializer, <64 x ptr> align 2 %addresses,
      <64 x i1> zeroinitializer, i32 64, metadata !0),
      !linx.pto.element.vp.owner !0,
      !llvm.loop.linx.pto.element.inter_element_order !2,
      !llvm.loop.linx.pto.element.intra_element_order !3
  ret void
}
!0 = distinct !{}
!1 = !{i32 32}
!2 = !{!"unordered"}
!3 = !{!"source"}

;--- domain64-v32.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
    <32 x float>, <32 x ptr>, <32 x i1>, i32, metadata)
define void @domain64_v32(ptr %base) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0),
      !linx.pto.element.vp.domain !1
  %one = insertelement <32 x ptr> poison, ptr %base, i32 0,
      !linx.pto.element.vp.owner !0
  %bases = shufflevector <32 x ptr> %one, <32 x ptr> poison,
                         <32 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  %addresses = getelementptr float, <32 x ptr> %bases,
               <32 x i32> zeroinitializer,
      !linx.pto.element.vp.owner !0
  call void @llvm.linx.experimental.element.scatter.v32f32.v32p0(
      <32 x float> zeroinitializer, <32 x ptr> align 4 %addresses,
      <32 x i1> zeroinitializer, i32 32, metadata !0),
      !linx.pto.element.vp.owner !0,
      !llvm.loop.linx.pto.element.inter_element_order !2,
      !llvm.loop.linx.pto.element.intra_element_order !3
  ret void
}
!0 = distinct !{}
!1 = !{i32 64}
!2 = !{!"unordered"}
!3 = !{!"source"}
