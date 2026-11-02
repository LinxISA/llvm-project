; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=SSA
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=TILE
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -S %s -o %t.ll
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -stop-after=linxv5-clockhands-postalloc %t.ll -o %t.mir
; RUN: llc -mtriple=linx64v5 -mcpu=janus -run-pass=machineverifier %t.mir -o /dev/null

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @dynamic_loop(ptr noalias %output, i32 %limit, i32 %skip) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  br label %inner
inner:
  %iteration = phi i32 [ 0, %header ], [ %increment, %inner.latch ]
  %sum = phi i32 [ 0, %header ], [ %updated, %inner.latch ]
  %less = icmp ult i32 %iteration, %limit
  br i1 %less, label %test, label %inner.exit
test:
  %continue = icmp eq i32 %iteration, %skip
  br i1 %continue, label %inner.latch, label %body
body:
  %value = add i32 %element, %iteration
  %addition = add i32 %sum, %value
  br label %inner.latch
inner.latch:
  %updated = phi i32 [ %sum, %test ], [ %addition, %body ]
  %increment = add i32 %iteration, 1
  br label %inner
inner.exit:
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %sum, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}

; SSA-LABEL: define void @dynamic_loop
; SSA: pto.loop.scheduler:
; SSA: phi <32 x i1>
; SSA: phi <32 x i32>
; SSA: call i1 @llvm.vector.reduce.or.v32i1
; SSA: br i1 {{.*}}, label %pto.loop.scheduler, label %exit
; TILE-LABEL: define void @dynamic_loop
; TILE: phi i64
; TILE: phi <32 x i32>
; TILE: icmp ne i64
; TILE: br i1
; TILE-NOT: llvm.vp.
; TILE-NOT: llvm.vector.reduce
; TILE: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
