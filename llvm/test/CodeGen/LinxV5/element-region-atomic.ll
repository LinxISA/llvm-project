; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-region' -S %s | FileCheck %s --check-prefix=IR
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-region' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @histogram_atomic(ptr %out.ptr, ptr %indices.ptr,
                              ptr %histogram, i32 %valid, i1 %outer) {
entry:
  %indices = load <32 x i32>, ptr %indices.ptr
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  br i1 %outer, label %tail.test, label %merge
tail.test:
  %tail = icmp ult i32 %i, %valid
  br i1 %tail, label %effect, label %merge
effect:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %tail.test ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  store <32 x i32> %published, ptr %out.ptr
  ret void
}

define void @selected_atomic(ptr %out.ptr, ptr %indices.ptr,
                             ptr %keys.ptr, ptr %histogram,
                             i32 %valid, i32 %selected) {
entry:
  %indices = load <32 x i32>, ptr %indices.ptr
  %keys = load <32 x i32>, ptr %keys.ptr
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %tail = icmp ult i32 %i, %valid
  br i1 %tail, label %key.test, label %merge
key.test:
  %key.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %keys, i64 3, i64 3, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %key = extractelement <32 x i32> %key.view, i32 %i
  %selected.lane = icmp eq i32 %key, %selected
  br i1 %selected.lane, label %effect, label %merge
effect:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %key.test ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 5, i64 5, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !4
exit:
  store <32 x i32> %published, ptr %out.ptr
  ret void
}

define void @scalarized_lane_zero_atomic(ptr %out.ptr, ptr %indices.ptr,
                                         ptr %histogram, i1 %part.active) {
entry:
  %indices = load <32 x i32>, ptr %indices.ptr
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %lane.zero = icmp eq i32 %i, 0
  %active = and i1 %part.active, %lane.zero
  br i1 %active, label %effect, label %inactive
effect:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 6, i64 6, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 0
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %effect.base = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %out, i64 7, i64 7, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %effect.insert = insertelement <32 x i32> %effect.base, i32 %old, i32 0
  br label %merge
inactive:
  %inactive.base = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %out, i64 7, i64 7, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %inactive.insert = insertelement <32 x i32> %inactive.base, i32 0, i32 %i
  br label %merge
merge:
  %carrier = phi <32 x i32> [ %effect.insert, %effect ],
                                [ %inactive.insert, %inactive ]
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %carrier, i64 7, i64 7, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !7
exit:
  store <32 x i32> %published, ptr %out.ptr
  ret void
}

; IR-LABEL: define void @histogram_atomic
; IR-NOT: llvm.linx.experimental.element.region
; IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; IR: select i1 %outer, i64 %pto.atomic.tail.mask, i64 0
; IR: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32
; IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; IR-NOT: atomicrmw
; IR-LABEL: define void @selected_atomic
; IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; IR: and i64
; IR: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32
; IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; IR-NOT: atomicrmw
; IR-LABEL: define void @scalarized_lane_zero_atomic
; IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr.v32i32
; IR: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32
; IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; IR-NOT: atomicrmw

; OBJ-LABEL: <histogram_atomic>:
; OBJ: BSTART.TEPL TCI, U32
; OBJ: BSTART.TEPL TCMPS, U32
; OBJ: BSTART.TEPL TLEA, U32
; OBJ: BSTART.TLSU MGATHER.ADD, U32
; OBJ: B.DATR CUBE_M32
; OBJ: ExecMaskPresent
; OBJ-LABEL: <selected_atomic>:
; OBJ: BSTART.TEPL TCI, U32
; OBJ: BSTART.TEPL TCMPS, U32
; OBJ: BSTART.TEPL TCMPS, U32
; OBJ: and
; OBJ: BSTART.TEPL TLEA, U32
; OBJ: BSTART.TLSU MGATHER.ADD, U32
; OBJ: B.DATR CUBE_M32
; OBJ: ExecMaskPresent
; OBJ-LABEL: <scalarized_lane_zero_atomic>:
; OBJ: BSTART.TEPL TCMPS, U32
; OBJ: BSTART.TEPL TLEA, U32
; OBJ: BSTART.TLSU MGATHER.ADD, U32
; OBJ: ExecMaskPresent

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}
