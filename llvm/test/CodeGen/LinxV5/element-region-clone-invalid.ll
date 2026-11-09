; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-element-predication -disable-output %s 2>&1 | FileCheck %s
; CHECK: predication: requires a dominating mandatory region sentinel

declare void @llvm.linx.experimental.element.region(metadata)

define void @ambiguous_clone() {
entry:
  br label %first
first:
  %i = phi i32 [0, %entry], [%inext, %first]
  %inext = add i32 %i, 1
  %imore = icmp ult i32 %inext, 32
  br i1 %imore, label %first, label %between, !llvm.loop !1
between:
  br label %second
second:
  %j = phi i32 [0, %between], [%jnext, %second]
  %jnext = add i32 %j, 1
  %jmore = icmp ult i32 %jnext, 32
  br i1 %jmore, label %second, label %exit, !llvm.loop !3
exit:
  call void @llvm.linx.experimental.element.region(metadata !0)
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{!3, !2}
