; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=INTERMEDIATE
;
; Element indices stay B32. A raw i32 GEP requests S32 interpretation at TLEA,
; while the TLEA result is the B64 byte offset. Fresh sign-neutral producers
; may publish S32 directly; signedness-sensitive producers retain the dtype
; required by their LLVM opcode and are retagged only at the address boundary.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @fresh_signed_add_index(ptr noalias %histogram) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %view, i32 %element
  %adjusted = add i32 %index, -7
  %address = getelementptr i32, ptr %histogram, i32 %adjusted
  %unused = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}

; CHECK-LABEL: define void @fresh_signed_add_index
; CHECK: [[SIGNED_ADD:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: [[ADD_OFFSET:%.*]] = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[SIGNED_ADD]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}<32 x i64> [[ADD_OFFSET]],
; CHECK-NOT: llvm.linx.experimental.element.
; CHECK-NOT: llvm.vp.
; CHECK: ret void

define void @shared_readonly_offsets(ptr noalias %left,
                                     ptr noalias %right) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %carrier = phi <32 x i32> [ poison, %entry ], [ %published, %loop ]
  %left.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %left.index = extractelement <32 x i32> %left.view, i32 %element
  %left.address = getelementptr i32, ptr %left, i32 %left.index
  %left.value = load i32, ptr %left.address, align 4
  %right.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %right.index = extractelement <32 x i32> %right.view, i32 %element
  %right.address = getelementptr i32, ptr %right, i32 %right.index
  %right.value = load i32, ptr %right.address, align 4
  %sum = add i32 %left.value, %right.value
  %inserted = insertelement <32 x i32> %carrier, i32 %sum, i32 %element
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 3, i64 3, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !4
exit:
  %result = phi <32 x i32> [ %published, %loop ]
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %result)
  ret void
}

; CHECK-LABEL: define void @shared_readonly_offsets
; CHECK: [[READ_OFFSET:%.*]] = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> %indices, i64 32)
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}ptr {{.*}}, <32 x i64> [[READ_OFFSET]],
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked{{.*}}ptr {{.*}}, <32 x i64> [[READ_OFFSET]],
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: ret void
; INTERMEDIATE-LABEL: define void @shared_readonly_offsets
; INTERMEDIATE: [[READ_VIEW0:%.*]] = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 2, i64 2, i64 0, i64 128, i64 17,
; INTERMEDIATE: sext <32 x i32> [[READ_VIEW0]] to <32 x i64>
; INTERMEDIATE: [[READ_VIEW1:%.*]] = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 2, i64 2, i64 0, i64 128, i64 17,
; INTERMEDIATE: sext <32 x i32> [[READ_VIEW1]] to <32 x i64>

define void @shared_atomic_offsets(ptr %left, ptr %right) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %left.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %left.index = extractelement <32 x i32> %left.view, i32 %element
  %left.address = getelementptr i32, ptr %left, i32 %left.index
  %left.old = atomicrmw add ptr %left.address, i32 1 monotonic, align 4
  %right.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %right.index = extractelement <32 x i32> %right.view, i32 %element
  %right.address = getelementptr i32, ptr %right, i32 %right.index
  %right.old = atomicrmw add ptr %right.address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !7
exit:
  ret void
}

; CHECK-LABEL: define void @shared_atomic_offsets
; CHECK: [[ATOMIC_OFFSET:%.*]] = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> %indices, i64 32)
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}ptr {{.*}}, <32 x i64> [[ATOMIC_OFFSET]],
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}ptr {{.*}}, <32 x i64> [[ATOMIC_OFFSET]],
; CHECK-NOT: @llvm.linx.experimental.ew.tlea
; CHECK: ret void
; INTERMEDIATE-LABEL: define void @shared_atomic_offsets
; INTERMEDIATE: [[ATOMIC_VIEW0:%.*]] = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 17,
; INTERMEDIATE: sext <32 x i32> [[ATOMIC_VIEW0]] to <32 x i64>
; INTERMEDIATE: [[ATOMIC_VIEW1:%.*]] = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 17,
; INTERMEDIATE: sext <32 x i32> [[ATOMIC_VIEW1]] to <32 x i64>

define void @signed_div_index(ptr noalias %histogram) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !9)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 5, i64 5, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %view, i32 %element
  %quotient = sdiv i32 %index, 3
  %address = getelementptr i32, ptr %histogram, i32 %quotient
  %unused = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !10
exit:
  ret void
}

; CHECK-LABEL: define void @signed_div_index
; CHECK: [[SDIV:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 3,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[SDIV]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; CHECK: ret void

define void @shift_signedness(ptr %signed.base, ptr %unsigned.base,
                              i32 %amount) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !15)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 7, i64 7, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %view, i32 %element
  %signed.index = ashr i32 %index, %amount
  %signed.address = getelementptr i32, ptr %signed.base, i32 %signed.index
  %signed.old = atomicrmw add ptr %signed.address, i32 1 monotonic, align 4
  %unsigned.index = lshr i32 %index, %amount
  %unsigned.address = getelementptr i32, ptr %unsigned.base,
                                    i32 %unsigned.index
  %unsigned.old = atomicrmw add ptr %unsigned.address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !16
exit:
  ret void
}

; CHECK-LABEL: define void @shift_signedness
; CHECK: [[ASHR:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 9,
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[ASHR]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; CHECK: [[LSHR:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 9,
; CHECK: [[LSHR_RETAG:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0, <32 x i32> [[LSHR]],
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[LSHR_RETAG]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; CHECK: ret void

define void @unsigned_div_signed_address(ptr noalias %histogram,
                                         i32 %divisor) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !12)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 6, i64 6, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %view, i32 %element
  %quotient = udiv i32 %index, %divisor
  %address = getelementptr i32, ptr %histogram, i32 %quotient
  %unused = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !13
exit:
  ret void
}

; CHECK-LABEL: define void @unsigned_div_signed_address
; CHECK: [[UDIV:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 3,
; CHECK: [[UDIV_RETAG:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 17, i64 29, i64 0, <32 x i32> [[UDIV]],
; CHECK-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked
; CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, <32 x i32> [[UDIV_RETAG]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}
!9 = distinct !{}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.linx.pto.element.region", !9}
!12 = distinct !{}
!13 = distinct !{!13, !14}
!14 = !{!"llvm.loop.linx.pto.element.region", !12}
!15 = distinct !{}
!16 = distinct !{!16, !17}
!17 = !{!"llvm.loop.linx.pto.element.region", !15}

