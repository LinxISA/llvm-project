; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/duplicate.ll 2>&1 | FileCheck %s --check-prefix=DUPLICATE
; RUN: not opt -mtriple=linx64v5 -passes='function(linx-v5-element-region,linx-v5-element-region-verify)' -disable-output %t/unowned.ll 2>&1 | FileCheck %s --check-prefix=UNOWNED
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extract.ll 2>&1 | FileCheck %s --check-prefix=EXTRACT

; DUPLICATE: PTO element region: P1a requires exactly one proved Tile publication store
; UNOWNED: PTO element region: required region lowering did not complete
; EXTRACT: PTO element region: unsupported external scalarized carrier consumer

;--- duplicate.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @duplicate_store(<32 x i32> %input) {
entry:
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
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- extract.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define i32 @external_extract(<32 x i32> %input) {
entry:
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
define <32 x i32> @unowned_marker(<32 x i32> %input) {
entry:
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
  ret <32 x i32> %result
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
