; RUN: split-file %s %t
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/header.ll 2>&1 | FileCheck %s --check-prefix=HEADER
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/freeze.ll 2>&1 | FileCheck %s --check-prefix=FREEZE

; A header branch whose two successors remain in the loop is dispatch, not a
; trip bound.  The real latch reaches iv=4, so the trap remains reachable.
; HEADER: unsupported opcode or effect in generic IR stage: call llvm.trap

; Freeze cannot be evaluated as identity when its operand can be poison.
; FREEZE: unsupported opcode or effect in generic IR stage: call llvm.trap

;--- header.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()

define void @both_header_successors_inside(ptr %out) {
entry:
  br label %outer.header

outer.header:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %outer.latch ]
  %dispatch = icmp ult i32 %iv, 2
  br i1 %dispatch, label %left, label %right

left:
  br label %inner.preheader

right:
  br label %inner.preheader

inner.preheader:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %inner.header

inner.header:
  %element = phi i32 [ 0, %inner.preheader ], [ %element.next, %inner.latch ]
  %unsafe = icmp uge i32 %iv, 2
  br i1 %unsafe, label %trap, label %store

trap:
  call void @llvm.trap()
  br label %store

store:
  %slot = getelementptr i32, ptr %out, i32 %element
  store i32 %iv, ptr %slot, align 4
  br label %inner.latch

inner.latch:
  %element.next = add nuw i32 %element, 1
  %inner.done = icmp eq i32 %element.next, 32
  br i1 %inner.done, label %outer.latch, label %inner.header, !llvm.loop !1

outer.latch:
  %iv.next = add nuw i32 %iv, 1
  %outer.done = icmp eq i32 %iv.next, 4
  br i1 %outer.done, label %exit, label %outer.header

exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- freeze.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()

define void @freeze_poison_is_unknown(ptr %out) {
entry:
  br label %outer.header

outer.header:
  %iv = phi i8 [ 0, %entry ], [ %iv.next, %outer.latch ]
  %selected = icmp eq i8 %iv, 1
  br i1 %selected, label %inner.preheader, label %outer.latch

inner.preheader:
  %poisonable = add nuw i8 %iv, -1
  %chosen = freeze i8 %poisonable
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %inner.header

inner.header:
  %element = phi i32 [ 0, %inner.preheader ], [ %element.next, %inner.latch ]
  %unsafe = icmp uge i8 %chosen, 1
  br i1 %unsafe, label %trap, label %store

trap:
  call void @llvm.trap()
  br label %store

store:
  %slot = getelementptr i32, ptr %out, i32 %element
  store i32 %element, ptr %slot, align 4
  br label %inner.latch

inner.latch:
  %element.next = add nuw i32 %element, 1
  %inner.done = icmp eq i32 %element.next, 32
  br i1 %inner.done, label %outer.latch, label %inner.header, !llvm.loop !1

outer.latch:
  %iv.next = add nuw i8 %iv, 1
  %outer.done = icmp eq i8 %iv.next, 2
  br i1 %outer.done, label %exit, label %outer.header

exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
