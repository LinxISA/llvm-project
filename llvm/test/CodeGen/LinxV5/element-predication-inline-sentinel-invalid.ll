; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %s 2>&1 | FileCheck %s
; CHECK: PTO element region: predication: requires a dominating mandatory region sentinel

declare void @llvm.linx.experimental.element.region(metadata)

define void @missing_dominating_instance(ptr %data, i1 %flag) {
entry:
  br i1 %flag, label %left, label %right
left:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %first
first:
  %element = phi i64 [ 0, %left ], [ %next, %first ]
  %address = getelementptr i32, ptr %data, i64 %element
  store i32 1, ptr %address, align 4
  %next = add nuw i64 %element, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %first, label %exit, !llvm.loop !1
right:
  br label %second
second:
  %other = phi i64 [ 0, %right ], [ %next.other, %second ]
  %other.address = getelementptr i32, ptr %data, i64 %other
  store i32 2, ptr %other.address, align 4
  %next.other = add nuw i64 %other, 1
  %more.other = icmp ult i64 %next.other, 32
  br i1 %more.other, label %second, label %exit, !llvm.loop !1
exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
