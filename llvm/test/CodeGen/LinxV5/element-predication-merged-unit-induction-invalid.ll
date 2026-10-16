; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/nonzero.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/plus-two.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/undef.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/bound-zero.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/bound-large.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/inclusive.ll 2>&1 | FileCheck %s --check-prefix=REJECT
; RUN: not opt -mtriple=linx64v5 -passes='linx-v5-element-predication' -disable-output %t/wrong-value.ll 2>&1 | FileCheck %s --check-prefix=REJECT

; REJECT: predication: requires a proven 1..4096 trip count and zero-based unit induction

;--- nonzero.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 1, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 1
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- plus-two.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 2
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- undef.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ undef, %right ]
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- bound-zero.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 1
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ult i32 %next, 0
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- bound-large.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 1
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ult i32 %next, 4097
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- inclusive.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 1
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ule i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- wrong-value.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
define void @bad(i1 %pick, i32 %limit) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %next, %latch ]
  br i1 %pick, label %left, label %right
left:
  %left.next = add i32 %iv, 1
  br label %latch
right:
  %right.next = add i32 %iv, 1
  br label %latch
latch:
  %next = phi i32 [ %left.next, %left ], [ %right.next, %right ]
  %more = icmp ult i32 %limit, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
