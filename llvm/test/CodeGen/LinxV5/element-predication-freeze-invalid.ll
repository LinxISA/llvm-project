; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication' -disable-output %s 2>&1 | FileCheck %s
; CHECK: freeze requires scalar i32 data

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @pointer_freeze(ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %header ]
  %destination = getelementptr i32, ptr %output, i32 %element
  %chosen = freeze ptr %destination
  store i32 %element, ptr %chosen, align 4
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
