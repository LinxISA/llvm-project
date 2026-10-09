; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/cross-element.ll 2>&1 | FileCheck %s --check-prefix=CROSS
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/header-snapshot.ll 2>&1 | FileCheck %s --check-prefix=SNAPSHOT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/argument-import.ll 2>&1 | FileCheck %s --check-prefix=IMPORT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/gm-load-import.ll 2>&1 | FileCheck %s --check-prefix=IMPORT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/fake-consumer.ll 2>&1 | FileCheck %s --check-prefix=CONSUMER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/i8-memory.ll 2>&1 | FileCheck %s --check-prefix=MEMORY
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/wrapper-storage.ll 2>&1 | FileCheck %s --check-prefix=WRAPPER
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/wrapper-dtype.ll 2>&1 | FileCheck %s --check-prefix=WRAPPER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -disable-output %t/wrapper-storage.ll 2>&1 | FileCheck %s --check-prefix=PRODWRAPPER
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -disable-output %t/wrapper-dtype.ll 2>&1 | FileCheck %s --check-prefix=PRODWRAPPER
;
; CROSS: PTO element region: predication: carrier extract requires a typed view and exact element IV
; SNAPSHOT: PTO element region: predication: live-out must be the final typed accumulator publication
; IMPORT: PTO element region: predication: typed import requires a dominating Tile producer
; CONSUMER: PTO element region: Tile legalization: vector live-out escapes its typed publication boundary
; MEMORY: PTO element region: predication: memory profile requires scalar i32 accesses
; WRAPPER: PTO element region: predication: final publication changes its typed storage identity
; PRODWRAPPER: PTO element region: Tile legalization: final publication changes its typed storage identity

;--- cross-element.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @cross_element_read() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %other.element = add i32 %element, 1
  %value = extractelement <32 x i32> %view, i32 %other.element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- wrapper-storage.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @wrapper_changes_storage() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %initial, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial.view, %entry ], [ %insert, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %typed.final = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 9, i64 9, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %typed.final, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- wrapper-dtype.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @wrapper_changes_dtype() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %initial, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial.view, %entry ], [ %insert, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %typed.final = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %typed.final, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- i8-memory.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @i8_memory_is_not_a_32bit_tile_stream(ptr noalias %source) {
entry:
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %address = getelementptr i8, ptr %source, i32 %element
  %byte = load i8, ptr %address, align 1
  %value = zext i8 %byte to i32
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- argument-import.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @argument_import(<32 x i32> %input) {
entry:
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %value = extractelement <32 x i32> %input.view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- gm-load-import.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @gm_load_import(ptr %source) {
entry:
  %input = load <32 x i32>, ptr %source, align 32
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %value = extractelement <32 x i32> %input.view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- fake-consumer.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @fake_consumer() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "X"(<32 x i32> %result)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- header-snapshot.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @header_snapshot_liveout() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %insert = insertelement <32 x i32> %carrier, i32 %value, i32 %element
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !1

exit:
  %result = phi <32 x i32> [ %published, %latch ]
  %snapshot = phi <32 x i32> [ %carrier, %latch ]
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %result)
  call void asm sideeffect "BSTART.TEPL 32, $0", "@2Tr"(<32 x i32> %snapshot)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
