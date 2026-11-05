; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -S %t/bounded.ll | FileCheck %s --check-prefix=PRE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -S %t/bounded.ll | FileCheck %s --check-prefix=BOUNDED
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize' -disable-output %t/unbounded.ll 2>&1 | FileCheck %s --check-prefix=UNBOUNDED

; PRE: %pto.b32.gep = zext <32 x i32> {{.*}} to <32 x i64>
; PRE: getelementptr i32, <32 x ptr> {{.*}}, <32 x i64> %pto.b32.gep

; BOUNDED-LABEL: define void @branch_local_bounded
; BOUNDED: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
; BOUNDED: @llvm.linx.experimental.ew.tlea.v32i64.v32i32
; BOUNDED-NOT: @llvm.vp.{{.*}}v32i64

; UNBOUNDED: Tile legalization: i64 VP address arithmetic requires a constant affine result or a fully active, address-only uniform offset with signed-B32 lane offsets

;--- bounded.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)

define void @branch_local_bounded(ptr noalias %input, ptr noalias %output,
                                  i1 %choose) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %latch ]
  br i1 %choose, label %left, label %right

left:
  %l = add nuw nsw i64 %iv, 1
  br label %merge

right:
  %r = add nuw nsw i64 %iv, 2
  br label %merge

merge:
  %joined = phi i64 [ %l, %left ], [ %r, %right ]
  %active = icmp ult i64 %joined, 32
  br i1 %active, label %copy, label %latch

copy:
  %src = getelementptr i32, ptr %input, i64 %joined
  %value = load i32, ptr %src, align 4
  %dst = getelementptr i32, ptr %output, i64 %iv
  store i32 %value, ptr %dst, align 4
  br label %latch

latch:
  %next = add nuw nsw i64 %iv, 1
  %done = icmp eq i64 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1

exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}

;--- unbounded.ll
target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)

define void @branch_local_unbounded(ptr noalias %input, ptr noalias %output,
                                    i64 %wide, i1 %choose) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %iv = phi i64 [ 0, %entry ], [ %next, %latch ]
  br i1 %choose, label %left, label %right

left:
  %l = add nuw nsw i64 %iv, 1
  br label %merge

right:
  %r = add i64 %wide, 2
  br label %merge

merge:
  %joined = phi i64 [ %l, %left ], [ %r, %right ]
  %active = icmp ult i64 %joined, 32
  br i1 %active, label %copy, label %latch

copy:
  %src = getelementptr i32, ptr %input, i64 %joined
  %value = load i32, ptr %src, align 4
  %dst = getelementptr i32, ptr %output, i64 %iv
  store i32 %value, ptr %dst, align 4
  br label %latch

latch:
  %next = add nuw nsw i64 %iv, 1
  %done = icmp eq i64 %next, 32
  br i1 %done, label %exit, label %loop, !llvm.loop !1

exit:
  ret void
}

!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
