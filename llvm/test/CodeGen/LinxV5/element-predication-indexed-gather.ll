; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=IR
;
; Read-only streams need a uniform base, but their per-element indices need not
; be affine or distinct. A raw narrow GEP index is signed; only an explicit
; zext selects unsigned byte-offset generation.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @signed_indexed_gather(ptr noalias %source) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %element
  %negative.index = sub i32 0, %index
  %address = getelementptr i32, ptr %source, i32 %negative.index
  %loaded = load i32, ptr %address, align 4
  %inserted = insertelement <32 x i32> %carrier, i32 %loaded, i32 %element
  br label %latch
latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1
exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %result)
  ret void
}

; IR-LABEL: define void @signed_indexed_gather
; IR: [[SIGNED_SUB:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 1,
; IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; IR: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[SIGNED_SUB]], i64 32)
; IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}i64 16,
; IR-NOT: llvm.linx.experimental.element.region
; IR-NOT: llvm.linx.experimental.element.view
; IR-NOT: llvm.vp.gather
; IR: ret void

define void @unsigned_indexed_gather(ptr noalias %source) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 3, i64 3, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %element
  %wide.index = zext i32 %index to i64
  %address = getelementptr i32, ptr %source, i64 %wide.index
  %loaded = load i32, ptr %address, align 4
  %inserted = insertelement <32 x i32> %carrier, i32 %loaded, i32 %element
  br label %latch
latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 4, i64 4, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !4
exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %result)
  ret void
}

; IR-LABEL: define void @unsigned_indexed_gather
; IR-COUNT-1: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32> {{.*}}, i64 32)
; IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}i64 24,
; IR-NOT: llvm.linx.experimental.element.region
; IR-NOT: llvm.linx.experimental.element.view
; IR-NOT: llvm.vp.gather
; IR: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

