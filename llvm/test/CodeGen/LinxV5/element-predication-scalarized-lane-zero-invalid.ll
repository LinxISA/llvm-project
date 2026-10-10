; RUN: split-file %s %t
; RUN: sed 's/%%lane.zero = icmp eq i32 %%element, 0/%%lane.zero = icmp eq i32 %%element, %%element/' %S/element-predication-scalarized-lane-zero.ll > %t/no-guard.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/no-guard.ll 2>&1 | FileCheck %s --check-prefix=GUARD
; RUN: sed 's/%%lane.zero = icmp eq i32 %%element, 0/%%lane.zero = icmp eq i32 %%element, 1/' %S/element-predication-scalarized-lane-zero.ll > %t/wrong-guard.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/wrong-guard.ll 2>&1 | FileCheck %s --check-prefix=GUARD
; RUN: sed 's/store <32 x i32> %%input, ptr %%annotated, align 32/store i32 7, ptr %%annotated, align 4/' %S/element-predication-scalarized-lane-zero.ll > %t/partial-store.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/partial-store.ll 2>&1 | FileCheck %s --check-prefix=PARTIAL
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/mixed-use.ll 2>&1 | FileCheck %s --check-prefix=EXACT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/whole-escape.ll 2>&1 | FileCheck %s --check-prefix=GUARD
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare' -S %t/unpromoted-load.ll | FileCheck %s --check-prefix=NOMETA
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %t/unpromoted-load.ll 2>&1 | FileCheck %s --check-prefix=VECTOR
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/old-wrapper.ll 2>&1 | FileCheck %s --check-prefix=IMPORT
;
; GUARD: PTO element region: predication: scalarized view requires a proven current-element-zero guard
; EXACT: PTO element region: predication: carrier extract requires a typed view and exact element IV
; IMPORT: PTO element region: predication: typed import requires a dominating Tile producer
; PARTIAL: PTO element region: view access carrier is i32 in store i32 7
; VECTOR: PTO element region: predication: memory profile requires scalar i32 accesses
; NOMETA: %pto.element.full.carrier = load <32 x i32>, ptr %storage, align {{[0-9]+}}{{$}}
; NOMETA-NOT: %pto.element.full.carrier = load {{.*}}!range
; NOMETA-NOT: %pto.element.full.carrier = load {{.*}}!noundef

;--- mixed-use.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @guarded_and_unguarded_use(i1 %enabled) {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29), !linx.pto.element.scalarized_lane_zero !3
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %merge ]
  %unguarded = extractelement <32 x i32> %view, i32 0
  %lane.zero = icmp eq i32 %element, 0
  %active = and i1 %enabled, %lane.zero
  br i1 %active, label %effect, label %merge
effect:
  %guarded = extractelement <32 x i32> %view, i32 0
  %used = add i32 %guarded, %unguarded
  br label %merge
merge:
  %value = phi i32 [ %used, %effect ], [ 0, %loop ]
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{}

;--- whole-escape.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @whole_carrier_escape(i1 %enabled) {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29), !linx.pto.element.scalarized_lane_zero !3
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %merge ]
  %lane.zero = icmp eq i32 %element, 0
  %active = and i1 %enabled, %lane.zero
  br i1 %active, label %effect, label %merge
effect:
  %guarded = extractelement <32 x i32> %view, i32 0
  call void asm sideeffect "opaque.tile.consumer $0", "@2Tr"(
      <32 x i32> %view)
  br label %merge
merge:
  %value = phi i32 [ %guarded, %effect ], [ 0, %loop ]
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{}

;--- unpromoted-load.ll
target triple = "linx64v5"
@annotation = private constant [61 x i8] c"pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32\00"
@source = private constant [1 x i8] c"x"
declare ptr @llvm.ptr.annotation.p0(ptr, ptr, ptr, i32, ptr)
declare void @llvm.linx.experimental.element.region(metadata)

define void @unpromoted_full_load(ptr noalias %histogram, i1 %enabled) {
entry:
  %storage = alloca [32 x i32], align 32
  %annotated = call ptr @llvm.ptr.annotation.p0(
      ptr %storage, ptr @annotation, ptr @source, i32 1, ptr null)
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  store <32 x i32> %input, ptr %annotated, align 32
  call void asm sideeffect "", "r"(ptr %storage)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %merge ]
  %lane.zero = icmp eq i32 %element, 0
  %active = and i1 %enabled, %lane.zero
  br i1 %active, label %effect, label %merge
effect:
  %index = load i32, ptr %annotated, align 4, !range !3, !noundef !4
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %loop ]
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{i32 0, i32 16}
!4 = !{}

;--- old-wrapper.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
    i64, i64, i64, i64, i64, i64)

define void @old_scalar_tci_wrapper(ptr %scalar.pointer, ptr %histogram,
                                    i1 %enabled) {
entry:
  %scalar = load i32, ptr %scalar.pointer, align 4
  %wide.scalar = zext i32 %scalar to i64
  %carrier = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 %wide.scalar, i64 0)
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %carrier, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29), !linx.pto.element.scalarized_lane_zero !3
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i32 [ 0, %entry ], [ %next, %merge ]
  %lane.zero = icmp eq i32 %element, 0
  %active = and i1 %enabled, %lane.zero
  br i1 %active, label %effect, label %merge
effect:
  %index = extractelement <32 x i32> %view, i32 0
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %loop ]
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{}

