; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=SSA
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @nested_break_continue(ptr noalias %output, i32 %stop,
                                   i32 %skip, i32 %divisor) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %boundary = add i32 %element, %stop
  %break.at = and i32 %boundary, 3
  br label %inner
inner:
  %iteration = phi i32 [ 0, %header ], [ %increment, %inner.latch ]
  %sum = phi i32 [ 0, %header ], [ %updated, %inner.latch ]
  %break = icmp eq i32 %iteration, %break.at
  br i1 %break, label %inner.exit, label %test.continue
test.continue:
  %continue = icmp eq i32 %iteration, %skip
  br i1 %continue, label %inner.latch, label %body
body:
  %numerator = add i32 %element, %iteration
  %quotient = udiv i32 %numerator, %divisor
  %addition = add i32 %sum, %quotient
  br label %inner.latch
inner.latch:
  %updated = phi i32 [ %sum, %test.continue ], [ %addition, %body ]
  %increment = add nuw nsw i32 %iteration, 1
  %more = icmp ult i32 %increment, 4
  br i1 %more, label %inner, label %inner.exit, !llvm.loop !3
inner.exit:
  %result = phi i32 [ %sum, %inner ], [ %updated, %inner.latch ]
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %result, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}

; SSA-LABEL: define void @nested_break_continue
; SSA-NOT: phi i32
; SSA: call <32 x i32> @llvm.vp.udiv
; SSA: call void @llvm.vp.scatter
; SSA-NOT: phi i32
; SSA: ret void
; TILE-LABEL: define void @nested_break_continue
; TILE: call <32 x i32> asm sideeffect "BSTART.TEPL TDIV, U32
; TILE: call void asm sideeffect "BSTART.TLSU MSCATTER, U32
; TILE-NOT: llvm.vp.
; TILE-NOT: llvm.linx.experimental.element.
; TILE: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{!3, !4}
!4 = !{!"llvm.loop.unroll.disable"}
