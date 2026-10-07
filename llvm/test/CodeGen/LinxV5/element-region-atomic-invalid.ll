; RUN: split-file %s %t
; RUN: sed 's/ i32 1 monotonic/ i32 1 seq_cst/' %t/base.ll > %t/order.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/order.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/ i32 1 monotonic/ i32 2 monotonic/' %t/base.ll > %t/value.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/value.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/ i32 1 monotonic/ i32 1 syncscope("singlethread") monotonic/' %t/base.ll > %t/scope.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/scope.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/\[ 0, %loop \]/[ 7, %loop ]/' %t/base.ll > %t/nonzero.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/nonzero.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/%wide = zext/%wide = sext/' %t/base.ll > %t/sext.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/sext.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/br i1 %active, label %effect, label %merge/br i1 %active, label %merge, label %effect/' %t/base.ll > %t/polarity.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/polarity.ll 2>&1 | FileCheck %s --check-prefix=ATOMIC
; RUN: sed 's/; EXTRA/store i32 0, ptr %histogram/' %t/base.ll > %t/extra-memory.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extra-memory.ll 2>&1 | FileCheck %s --check-prefix=MEMORY
; RUN: sed 's/; LEAK/%leak = add i32 %old, 0/' %t/base.ll > %t/extra-use.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extra-use.ll 2>&1 | FileCheck %s --check-prefix=USE
; RUN: sed 's/; OTHER/%other = add i32 %i, 1/' %t/base.ll | sed 's/%index.view, i32 %i/%index.view, i32 %other/' > %t/wrong-iv.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/wrong-iv.ll 2>&1 | FileCheck %s --check-prefix=INDEX
; RUN: sed 's/; EXTRA/%extra = load i32, ptr %histogram, align 4/' %t/base.ll > %t/extra-load.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extra-load.ll 2>&1 | FileCheck %s --check-prefix=MEMORY
; RUN: sed 's/; SECOND/%%second = atomicrmw add ptr %address, i32 1 monotonic, align 4/' %t/base.ll > %t/multiple.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/multiple.ll 2>&1 | FileCheck %s --check-prefix=MULTIPLE
; RUN: sed 's/; ALIAS/store <32 x i32> %%published, ptr %histogram, align 4/' %t/base.ll > %t/alias.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/alias.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/mismatched-publication.ll 2>&1 | FileCheck %s --check-prefix=PUBINDEX
; RUN: sed 's/; PHIUSER/store <32 x i32> %carrier, ptr %histogram, align 4/' %t/mismatched-publication.ll > %t/extra-phi-user.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extra-phi-user.ll 2>&1 | FileCheck %s --check-prefix=MEMORY
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/scalarized-pointer.ll 2>&1 | FileCheck %s --check-prefix=HOIST

; ATOMIC: PTO element region: {{requires non-volatile system-scope monotonic atomic add of exact i32 value one|inactive atomic lanes must publish exact zero|atomic address requires exact zext i32 element index|atomic active path is not a canonical CFG conjunction|atomic effect block must branch directly to its result merge}}
; MEMORY: PTO element region: {{unpromoted or unproved memory access|P1b requires one proved zero-inactive gather diamond}}
; USE: PTO element region: atomic old value must feed only the element result merge
; INDEX: PTO element region: atomic index view is not selected by the current element
; MULTIPLE: PTO element region: element region requires exactly one atomic effect
; ALIAS: PTO element region: atomic target may alias its Tile publication storage
; PUBINDEX: PTO element region: constant atomic publication index does not match the proved active lane
; HOIST: PTO element region: scalarized lane-zero load is not safe to hoist

;--- base.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @invalid_atomic(<32 x i32> %indices, ptr %histogram,
                                  i32 %valid) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  ; OTHER
  ; EXTRA
  %active = icmp ult i32 %i, %valid
  br i1 %active, label %effect, label %merge
effect:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  ; SECOND
  ; LEAK
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  ; ALIAS
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %published
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- scalarized-pointer.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
declare <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
    i64, i64, i64, i64, i64, i64)

define <32 x i32> @inside_loop_pointer(ptr %histogram) {
entry:
  %storage = alloca [32 x i32], align 32
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %lane = icmp eq i32 %i, 0
  br i1 %lane, label %effect, label %merge
effect:
  %inside.pointer = getelementptr i32, ptr %storage, i32 0
  %scalar = load i32, ptr %inside.pointer, align 4
  %wide.scalar = zext i32 %scalar to i64
  %carrier = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 %wide.scalar, i64 0)
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %carrier, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29), !linx.pto.element.scalarized_lane_zero !3
  %index = extractelement <32 x i32> %index.view, i32 0
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  br label %merge
merge:
  %value = phi i32 [ %old, %effect ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %published
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{}

;--- mismatched-publication.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @mismatched_publication(<32 x i32> %indices,
                                          ptr %histogram) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %lane = icmp eq i32 %i, 31
  br i1 %lane, label %effect, label %inactive
effect:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %histogram, i64 %wide
  %old = atomicrmw add ptr %address, i32 1 monotonic, align 4
  %effect.base = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %out, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %effect.insert = insertelement <32 x i32> %effect.base, i32 %old, i32 0
  br label %merge
inactive:
  %inactive.base = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %out, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %inactive.insert = insertelement <32 x i32> %inactive.base, i32 0, i32 %i
  br label %merge
merge:
  %carrier = phi <32 x i32> [ %effect.insert, %effect ],
                                [ %inactive.insert, %inactive ]
  ; PHIUSER
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %carrier, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %published
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
