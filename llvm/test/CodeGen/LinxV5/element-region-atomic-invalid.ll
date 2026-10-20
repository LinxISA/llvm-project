; RUN: split-file %s %t
; RUN: sed 's/ i32 1 monotonic/ i32 1 seq_cst/' %t/base.ll > %t/order.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/order.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/ i32 1 monotonic/ i32 2 monotonic/' %t/base.ll > %t/value.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/value.ll | FileCheck %s --check-prefix=VALUE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/ i32 1 monotonic/ i32 1 syncscope("singlethread") monotonic/' %t/base.ll > %t/scope.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/scope.ll 2>&1 | FileCheck %s --check-prefix=PROFILE
; RUN: sed 's/\[ 0, %loop \]/[ 7, %loop ]/' %t/base.ll > %t/nonzero.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/nonzero.ll | FileCheck %s --check-prefix=NONZERO --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/%wide = zext/%wide = sext/' %t/base.ll > %t/sext.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/sext.ll | FileCheck %s --check-prefix=SIGNED --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/br i1 %active, label %effect, label %merge/br i1 %active, label %merge, label %effect/' %t/base.ll > %t/polarity.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/polarity.ll | FileCheck %s --check-prefix=POLARITY --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/; EXTRA/store i32 0, ptr %histogram/' %t/base.ll > %t/extra-memory.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/extra-memory.ll 2>&1 | FileCheck %s --check-prefix=MEMORY
; RUN: sed 's/; LEAK/%leak = add i32 %old, 3/; s/\[ %old, %effect \]/[ %leak, %effect ]/' %t/base.ll > %t/extra-use.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/extra-use.ll | FileCheck %s --check-prefix=USE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/; OTHER/%other = add i32 %i, 1/' %t/base.ll | sed 's/%index.view, i32 %i/%index.view, i32 %other/' > %t/wrong-iv.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/wrong-iv.ll 2>&1 | FileCheck %s --check-prefix=INDEX
; RUN: sed 's/; EXTRA/%extra = load i32, ptr %histogram, align 4/' %t/base.ll > %t/extra-load.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/extra-load.ll | FileCheck %s --check-prefix=LOAD --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/; SECOND/%%second = atomicrmw add ptr %address, i32 1 monotonic, align 4/' %t/base.ll > %t/multiple.ll
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/multiple.ll | FileCheck %s --check-prefix=MULTIPLE --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: sed 's/; ALIAS/store <32 x i32> %%published, ptr %histogram, align 4/' %t/base.ll > %t/alias.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/alias.ll 2>&1 | FileCheck %s --check-prefix=ALIAS
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/mismatched-publication.ll 2>&1 | FileCheck %s --check-prefix=PUBINDEX
; RUN: sed 's/; PHIUSER/store <32 x i32> %carrier, ptr %histogram, align 4/' %t/mismatched-publication.ll > %t/extra-phi-user.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/extra-phi-user.ll 2>&1 | FileCheck %s --check-prefix=PUBINDEX
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/scalarized-pointer.ll 2>&1 | FileCheck %s --check-prefix=HOIST

; PROFILE: PTO element region: predication: atomic profile requires aligned nonvolatile system-scope monotonic i32 add
; MEMORY: PTO element region: predication: unordered scatter requires the unordered/source loop contract
; INDEX: PTO element region: predication: carrier extract requires a typed view and exact element IV
; ALIAS: PTO element region: predication: memory profile requires scalar i64/U16/i32/f16/f32 accesses
; PUBINDEX: PTO element region: predication: carrier insert requires a typed view and exact element IV
; HOIST: PTO element region: predication: unsupported opcode or effect in generic IR stage: call llvm.linx.experimental.ew.tci
; VALUE: freeze i32 2
; VALUE: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; VALUE: call void asm sideeffect "", "@2Tr"
; NONZERO: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; NONZERO: freeze i32 7
; NONZERO: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr
; SIGNED: call <32 x i64> @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, {{.*}}i64 32)
; SIGNED: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked.v32i32.v32i64.v32i32(i64 32, i64 1, i64 25, i64 0, i64 29,
; POLARITY: xor i64
; POLARITY: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; USE: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; USE: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 0,
; LOAD: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; LOAD-NOT: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; LOAD: ret void
; MULTIPLE: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; MULTIPLE: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
; MULTIPLE: call void asm sideeffect "", "@2Tr"

;--- base.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @invalid_atomic(ptr %histogram,
                                  i32 %valid) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
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

define void @inside_loop_pointer(ptr %histogram) {
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
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

define void @mismatched_publication(ptr %histogram) {
entry:
  %indices = call <32 x i32> asm sideeffect "", "=@2Tr"()
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
