; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-region' -S %t/base.ll | FileCheck %s --check-prefix=POSITIVE
; RUN: sed 's/br i1 %active, label %load, label %merge/br i1 %active, label %merge, label %load/' %t/base.ll > %t/false-edge.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/false-edge.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/%active = icmp ult i32 %i, %valid/%active = icmp ult i32 %other, %valid/' %t/base.ll > %t/wrong-iv.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/wrong-iv.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/%active = icmp ult i32 %i, %valid/%active = icmp ult i32 %i, %other/' %t/base.ll > %t/varying.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/varying.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/%wide = zext i32 %index to i64/%wide = sext i32 %index to i64/' %t/base.ll > %t/sext.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/sext.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/getelementptr i32, ptr %base/getelementptr i64, ptr %base/' %t/base.ll > %t/gep64.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/gep64.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/; CLOBBER/store i32 0, ptr %base/' %t/base.ll > %t/clobber.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/clobber.ll 2>&1 | FileCheck %s --check-prefix=P1B
; RUN: sed 's/; EXTRA/%extra = load i32, ptr %base, align 4/' %t/base.ll > %t/extra.ll
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-region' -disable-output %t/extra.ll 2>&1 | FileCheck %s --check-prefix=P1B

; POSITIVE: llvm.linx.experimental.ew.mgather.gpr.masked
; POSITIVE-NOT: extractelement
; POSITIVE-NOT: insertelement
; P1B: PTO element region: {{P1b requires one proved zero-inactive gather diamond|unpromoted or unproved memory access}}

;--- base.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define <32 x i32> @gather_cfg(<32 x i32> %indices, ptr %base, i32 %valid) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [ 0, %entry ], [ %next, %merge ]
  %out = phi <32 x i32> [ poison, %entry ], [ %published, %merge ]
  %other = add i32 %i, 1
  ; CLOBBER
  ; EXTRA
  %active = icmp ult i32 %i, %valid
  br i1 %active, label %load, label %merge
load:
  %index.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %indices, i64 1, i64 1, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %index = extractelement <32 x i32> %index.view, i32 %i
  %wide = zext i32 %index to i64
  %address = getelementptr i32, ptr %base, i64 %wide
  %loaded = load i32, ptr %address, align 4
  br label %merge
merge:
  %value = phi i32 [ %loaded, %load ], [ 0, %loop ]
  %inserted = insertelement <32 x i32> %out, i32 %value, i32 %i
  %published = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(<32 x i32> %inserted, i64 2, i64 2, i64 0, i64 128, i64 25, i64 32, i64 1, i64 29)
  %next = add nuw nsw i32 %i, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret <32 x i32> %published
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
