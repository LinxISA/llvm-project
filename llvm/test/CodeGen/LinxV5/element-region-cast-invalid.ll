; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/u8.ll | FileCheck %s --check-prefix=U8 --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %t/u64.ll | FileCheck %s --check-prefix=U64 --implicit-check-not='call {{.*}}@llvm.linx.experimental.element.' --implicit-check-not='call {{.*}}@llvm.vp.' --implicit-check-not=extractelement --implicit-check-not=insertelement
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -disable-output %t/fp.ll 2>&1 | FileCheck %s --check-prefix=FP

; U8-LABEL: define void @bad_u8
; U8: freeze i32 255
; U8: call <32 x i32> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32(i64 32, i64 1, i64 25, i64 29, i64 5,
; U8: call void asm sideeffect "", "@2Tr"
; U64-LABEL: define void @bad_u64
; U64: freeze i32 0
; U64: call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32
; U64: call void asm sideeffect "", "@2Tr"
; FP: PTO element region: predication: only integer casts are supported in this IR stage

;--- u8.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @bad_u8() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %pub)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- u64.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @bad_u64() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %pub)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- fp.ll
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)
define void @bad_fp() {
entry:
  %input = call <32 x i32> asm sideeffect "", "=@2Tr"()
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
  call void asm sideeffect "", "@2Tr"(<32 x i32> %pub)
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
