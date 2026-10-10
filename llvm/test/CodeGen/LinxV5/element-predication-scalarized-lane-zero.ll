; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
;
; InstCombine may reduce an element-zero read to a scalar load. The generic
; preparation path restores the complete local carrier, and the current-IV
; guard proves that extracting and inserting constant element zero denotes the
; current logical element. SROA must then recover the original Tile producer.

target triple = "linx64v5"

@annotation = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@source = private constant [1 x i8] c"x"

declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @guarded_lane_zero_atomic(ptr noalias %histogram, i1 %enabled) {
entry:
  %storage = alloca [32 x i32], align 32
  %annotated = call ptr @llvm.ptr.annotation.p0(
      ptr %storage, ptr @annotation, ptr @source, i32 1, ptr null)
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  store <32 x i32> %input, ptr %annotated, align 32
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header

header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %carrier = phi <32 x i32> [ poison, %entry ], [ %published, %latch ]
  br i1 %enabled, label %lane.test, label %inactive

lane.test:
  %lane.zero = icmp eq i32 %element, 0
  br i1 %lane.zero, label %effect, label %inactive

effect:
  %index = load i32, ptr %annotated, align 4
  %wide.index = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide.index
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %effect.insert = insertelement <32 x i32> %carrier, i32 %old, i64 0
  br label %merge

inactive:
  %inactive.insert = insertelement <32 x i32> %carrier, i32 0, i32 %element
  br label %merge

merge:
  %updated = phi <32 x i32> [ %effect.insert, %effect ],
                            [ %inactive.insert, %inactive ]
  br label %latch

latch:
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %updated, i64 20, i64 20, i64 0, i64 128, i64 25,
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

; CHECK-LABEL: define void @guarded_lane_zero_atomic
; CHECK: [[INPUT:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK-NOT: load
; CHECK-NOT: extractelement
; CHECK-NOT: insertelement
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: llvm.vp.gather
; CHECK-NOT: pto.element.full.carrier
; CHECK: [[OFFSET:%.*]] = call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32> [[INPUT]], i64 32)
; CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked{{.*}}<32 x i64> [[OFFSET]],
; CHECK-NOT: llvm.linx.experimental.element.region
; CHECK-NOT: llvm.linx.experimental.element.atomic.add
; CHECK: call void asm sideeffect "opaque.tile.consumer $0"
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

