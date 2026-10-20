; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/duplicate.ll 2>&1 | FileCheck %s --check-prefix=DUPLICATE
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/unowned.ll 2>&1 | FileCheck %s --check-prefix=UNOWNED
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/extract.ll 2>&1 | FileCheck %s --check-prefix=EXTRACT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-tile-legalize' -disable-output %t/dead-unknown-owner.ll 2>&1 | FileCheck %s --check-prefix=OWNER

; DUPLICATE: PTO element region: predication: live-out must be the final typed accumulator publication
; UNOWNED: PTO element region: Tile legalization: invalid external typed carrier boundary
; EXTRACT: PTO element region: Tile legalization: vector live-out escapes its typed publication boundary
; OWNER: PTO element region: Tile legalization: unknown predicated region token

;--- dead-unknown-owner.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @dead_unknown_owner() {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0), !linx.pto.element.vp.domain !2
  %dead = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> zeroinitializer, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29), !linx.pto.element.vp.owner !1
  ret void
}
!0 = distinct !{}
!1 = distinct !{}
!2 = !{i32 32}

;--- duplicate.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @duplicate_store() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %out0 = alloca <32 x i32>, align 32
  %out1 = alloca <32 x i32>, align 32
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %i
  %inserted = insertelement <32 x i32> poison, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  store <32 x i32> %published, ptr %out0, align 32
  store <32 x i32> %published, ptr %out1, align 32
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  call void asm sideeffect "", "@2Tr"(<32 x i32> %published)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- extract.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define i32 @external_extract() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %loop ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %i
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %scalar = extractelement <32 x i32> %published, i32 0
  ret i32 %scalar
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- unowned.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @unowned_marker() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
  %unowned = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> zeroinitializer, i64 9, i64 9, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %loop ]
  %input.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %input.view, i32 %i
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  %result = phi <32 x i32> [ %published, %loop ]
  call void asm sideeffect "", "@2Tr"(<32 x i32> %unowned)
  call void asm sideeffect "", "@2Tr"(<32 x i32> %result)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
