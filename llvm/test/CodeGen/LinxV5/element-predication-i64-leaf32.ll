; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=IR
;
; Native i64 element Tiles are one 32-lane, 256-byte CUBE_M32 leaf. Preserve
; typed imports, empty imported carriers, freeze, and comparison signedness
; without widening the logical shape to v64i64.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i64> @llvm.linx.experimental.element.view.v32i64(
    <32 x i64>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @signed_compare_freeze() {
entry:
  %left = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %right = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %initial.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %initial, i64 1, i64 1, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i64> [ %initial.view, %entry ], [ %published, %latch ]
  %left.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %left, i64 2, i64 2, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  %right.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %right, i64 3, i64 3, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  %lhs.raw = extractelement <32 x i64> %left.view, i32 %element
  %lhs = freeze i64 %lhs.raw
  %rhs = extractelement <32 x i64> %right.view, i32 %element
  %equal = icmp eq i64 %lhs, %rhs
  %less = icmp slt i64 %lhs, %rhs
  %less.choice = select i1 %less, i64 %lhs, i64 17
  %selected = select i1 %equal, i64 %rhs, i64 %less.choice
  %inserted = insertelement <32 x i64> %carrier, i64 %selected, i32 %element
  br label %latch

latch:
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %inserted, i64 1, i64 1, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i64> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i64> %result)
  ret void
}

; IR-LABEL: define void @signed_compare_freeze()
; IR: [[LEFT:%.*]] = call <32 x i64> asm sideeffect "", "=@2Tr"()
; IR: [[RIGHT:%.*]] = call <32 x i64> asm sideeffect "", "=@2Tr"()
; IR: call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 16, i64 29, <32 x i64> [[LEFT]], <32 x i64> [[RIGHT]], i64 0)
; IR: call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 16, i64 29, <32 x i64> [[LEFT]], <32 x i64> [[RIGHT]], i64 2)
; IR: call <32 x i64> @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 16,
; IR-NOT: freeze <32 x i64>
; IR-NOT: <64 x i64>
; IR-NOT: llvm.vp.
; IR-NOT: llvm.linx.experimental.element.

define void @unsigned_compare() {
entry:
  %left = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %right = call <32 x i64> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i64> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i64> [ %initial, %entry ], [ %published, %latch ]
  %left.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %left, i64 4, i64 4, i64 0, i64 256, i64 24,
      i64 32, i64 1, i64 29)
  %right.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %right, i64 5, i64 5, i64 0, i64 256, i64 24,
      i64 32, i64 1, i64 29)
  %lhs = extractelement <32 x i64> %left.view, i32 %element
  %rhs = extractelement <32 x i64> %right.view, i32 %element
  %less = icmp ult i64 %lhs, %rhs
  %selected = select i1 %less, i64 %lhs, i64 23
  %inserted = insertelement <32 x i64> %carrier, i64 %selected, i32 %element
  br label %latch

latch:
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %inserted, i64 6, i64 6, i64 0, i64 256, i64 24,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !4

exit:
  %result = phi <32 x i64> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i64> %result)
  ret void
}

; IR-LABEL: define void @unsigned_compare()
; IR: call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 24,
; IR: call <32 x i64> @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 24,
; IR-NOT: <64 x i64>

define void @signed_empty_import(i32 %choice) {
entry:
  %is.actual = icmp eq i32 %choice, 2
  br i1 %is.actual, label %actual.path, label %empty.test

empty.test:
  %is.undef = icmp eq i32 %choice, 0
  br i1 %is.undef, label %undef.path, label %poison.path

actual.path:
  %actual = call <32 x i64> asm sideeffect "", "=@2Tr"()
  br label %preheader

undef.path:
  br label %preheader

poison.path:
  br label %preheader

preheader:
  %import = phi <32 x i64> [ %actual, %actual.path ],
                            [ undef, %undef.path ],
                            [ poison, %poison.path ]
  %initial = call <32 x i64> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %loop

loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %carrier = phi <32 x i64> [ %initial, %preheader ], [ %published, %loop ]
  %import.view = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %import, i64 7, i64 7, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i64> %import.view, i32 %element
  %inserted = insertelement <32 x i64> %carrier, i64 %value, i32 %element
  %published = call <32 x i64> @llvm.linx.experimental.element.view.v32i64(
      <32 x i64> %inserted, i64 8, i64 8, i64 0, i64 256, i64 16,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !7

exit:
  %result = phi <32 x i64> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i64> %result)
  ret void
}

; IR-LABEL: define void @signed_empty_import
; IR: undef.path:
; IR: call <32 x i64> @llvm.linx.experimental.ew.tci.v32i64(i64 32, i64 1, i64 16, i64 29,
; IR: poison.path:
; IR: call <32 x i64> @llvm.linx.experimental.ew.tci.v32i64(i64 32, i64 1, i64 16, i64 29,
; IR: %import = phi <32 x i64>
; IR-NOT: phi <32 x i64> [ undef
; IR-NOT: phi <32 x i64> [ poison
; IR-NOT: <64 x i64>

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}
