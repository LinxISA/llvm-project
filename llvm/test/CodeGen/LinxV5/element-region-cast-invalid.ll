; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/u8.ll 2>&1 | FileCheck %s --check-prefix=U8
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/u64.ll 2>&1 | FileCheck %s --check-prefix=U64
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/fp.ll 2>&1 | FileCheck %s --check-prefix=FP

; U8: PTO element region: unsupported scalar expression in P1a arithmetic region
; U64: PTO element region: unsupported scalar expression in P1a arithmetic region
; FP: PTO element region: unsupported scalar expression in P1a arithmetic region

;--- u8.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define <32 x i32> @bad_u8(<32 x i32> %input) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %out = phi <32 x i32> [ poison, %entry ], [ %pub, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %x = extractelement <32 x i32> %view, i32 %i
  %narrow = trunc i32 %x to i8
  %wide = zext i8 %narrow to i32
  %insert = insertelement <32 x i32> %out, i32 %wide, i32 %i
  %pub = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %pub
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- u64.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define <32 x i32> @bad_u64(<32 x i32> %input) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %out = phi <32 x i32> [ poison, %entry ], [ %pub, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %x = extractelement <32 x i32> %view, i32 %i
  %wide = zext i32 %x to i64
  %high = shl i64 %wide, 40
  %truncated = trunc i64 %high to i32
  %insert = insertelement <32 x i32> %out, i32 %truncated, i32 %i
  %pub = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %pub
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- fp.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define <32 x i32> @bad_fp(<32 x i32> %input) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %loop ]
  %out = phi <32 x i32> [ poison, %entry ], [ %pub, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %input, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %x = extractelement <32 x i32> %view, i32 %i
  %as.fp = uitofp i32 %x to float
  %rounded = fptoui float %as.fp to i32
  %insert = insertelement <32 x i32> %out, i32 %rounded, i32 %i
  %pub = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %insert, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %pub
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
