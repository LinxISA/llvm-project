; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %s 2>&1 | FileCheck %s
; CHECK: predication: unsupported opcode or effect
; Unsupported, valid LLVM input must diagnose rather than assert in widening.
declare void @llvm.linx.experimental.element.region(metadata)
define void @unsupported(ptr %p) {
entry:
 call void @llvm.linx.experimental.element.region(metadata !0)
 br label %loop
loop:
 %i = phi i64 [0, %entry], [%next, %loop]
 %v = atomicrmw add ptr %p, i32 1 monotonic
 %next = add nuw i64 %i, 1
 %more = icmp ult i64 %next, 4
 br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
 ret void
}
!0 = distinct !{}
!1 = distinct !{!1,!2}
!2 = !{!"llvm.loop.linx.pto.element.region",!0}
