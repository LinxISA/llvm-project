; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %s 2>&1 | FileCheck %s
; CHECK: predication: memory streams may overlap across elements

declare void @llvm.linx.experimental.element.region(metadata)
define void @overlap(ptr %data) {
entry:
 call void @llvm.linx.experimental.element.region(metadata !0)
 br label %loop
loop:
 %i = phi i64 [0, %entry], [%next, %loop]
 %p = getelementptr i32, ptr %data, i64 %i
 %x = load i32, ptr %p
 %next = add nuw i64 %i, 1
 %q = getelementptr i32, ptr %data, i64 %next
 store i32 %x, ptr %q
 %more = icmp ult i64 %next, 4
 br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
 ret void
}
!0 = distinct !{}
!1 = distinct !{!1,!2}
!2 = !{!"llvm.loop.linx.pto.element.region",!0}
