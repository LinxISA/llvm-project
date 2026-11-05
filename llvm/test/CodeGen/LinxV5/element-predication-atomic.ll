; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=PRED
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=FINAL
;
; Ordinary monotonic i32 atomic adds remain ordered effects in the predicated
; CFG. Equal indices are deliberately legal: different elements may update the
; same histogram bin, and each later atomic observes the earlier atomic's old
; value in source order.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @nested_duplicate_atomic(ptr noalias %histogram, i32 %limit,
                                     i1 %enabled) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %deltas = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %initial = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ %initial, %entry ], [ %published, %latch ]
  br i1 %enabled, label %index.block, label %latch

index.block:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %element
  %in.range = icmp ult i32 %index, %limit
  br i1 %in.range, label %atomic.block, label %latch

atomic.block:
  %delta.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %deltas, i64 2, i64 2, i64 0, i64 128, i64 17,
      i64 32, i64 1, i64 29)
  %delta = extractelement <32 x i32> %delta.view, i32 %element
  %wide.index = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide.index
  %first.old = atomicrmw add ptr %address, i32 %delta monotonic, align 4
  %second.old = atomicrmw add ptr %address, i32 %first.old monotonic, align 4
  %inserted = insertelement <32 x i32> %carrier, i32 %second.old,
                            i32 %element
  br label %latch

latch:
  %updated = phi <32 x i32> [ %inserted, %atomic.block ],
                            [ %carrier, %index.block ],
                            [ %carrier, %header ]
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %updated, i64 3, i64 3, i64 0, i64 128, i64 25,
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

; PRED-LABEL: define void @nested_duplicate_atomic
; PRED: [[FIRST:%.*]] = call <32 x i32> @llvm.linx.experimental.element.atomic.add{{.*}}(<32 x ptr> {{.*}}, <32 x i32> {{.*}}, <32 x i1> {{.*}}, i32 32)
; PRED: [[SECOND:%.*]] = call <32 x i32> @llvm.linx.experimental.element.atomic.add{{.*}}(<32 x ptr> {{.*}}, <32 x i32> [[FIRST]], <32 x i1> {{.*}}, i32 32)
; PRED-NOT: atomicrmw
; PRED: call void asm sideeffect "opaque.tile.consumer $0"
;
; FINAL-LABEL: define void @nested_duplicate_atomic
; FINAL: [[OFFSET:%.*]] = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32> {{.*}}, i64 32)
; FINAL: [[RETAG:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 0,
; FINAL: [[FIRST_TILE:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}{{\(.*}}<32 x i64> [[OFFSET]], <32 x i32> [[RETAG]],
; FINAL: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}{{\(.*}}<32 x i64> [[OFFSET]], <32 x i32> [[FIRST_TILE]],
; FINAL-NOT: llvm.linx.experimental.element.atomic.add
; FINAL-NOT: llvm.linx.experimental.element.region
; FINAL-NOT: llvm.linx.experimental.element.view
; FINAL-NOT: atomicrmw
; FINAL: call void asm sideeffect "opaque.tile.consumer $0"

define void @unused_atomic_result(ptr noalias %histogram) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 4, i64 4, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %element
  %wide.index = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide.index
  %unused = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %latch

latch:
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %header, label %exit, !llvm.loop !4

exit:
  ret void
}

; PRED-LABEL: define void @unused_atomic_result
; PRED: call <32 x i32> @llvm.linx.experimental.element.atomic.add
; PRED-NOT: atomicrmw
; PRED: ret void
; FINAL-LABEL: define void @unused_atomic_result
; FINAL: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32{{.*}}i64 32)
; FINAL: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; FINAL-NOT: llvm.linx.experimental.element.atomic.add
; FINAL-NOT: atomicrmw
; FINAL: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}

