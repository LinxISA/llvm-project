; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s
;
; Materialization is edge-local: undefined inputs become real zero Tiles while
; existing Tile producers, including producers on a loop backedge, remain the
; PHI inputs selected on their original control-flow edges.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)
declare <32 x i32> @llvm.linx.experimental.element.view.v32i32(
    <32 x i32>, i64, i64, i64, i64, i64, i64, i64, i64)

define void @preserve_actual_seed(i1 %has.seed, ptr noalias %output) {
entry:
  br i1 %has.seed, label %seed.path, label %empty.path

seed.path:
  %nonzero.seed = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %preheader

empty.path:
  br label %preheader

preheader:
  %import = phi <32 x i32> [ %nonzero.seed, %seed.path ],
                             [ undef, %empty.path ]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %import, i64 1, i64 1, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %address = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %address, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  ret void
}

; CHECK-LABEL: define void @preserve_actual_seed
; CHECK: seed.path:
; CHECK: [[ACTUAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: br label %preheader
; CHECK: empty.path:
; CHECK: [[EMPTY_SCALAR:%.*]] = freeze i32 0
; CHECK: [[EMPTY_BITS:%.*]] = zext i32 [[EMPTY_SCALAR]] to i64
; CHECK: [[EMPTY_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[EMPTY_BITS]], i64 0)
; CHECK: br label %preheader
; CHECK: preheader:
; CHECK: %import = phi <32 x i32> [ [[ACTUAL]], %seed.path ], [ [[EMPTY_ZERO]], %empty.path ]
; CHECK-NOT: phi <32 x i32> [ zeroinitializer
; CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; CHECK: ret void

define void @duplicate_predecessor_edges(i1 %take.left, i1 %duplicate,
                                         ptr noalias %output) {
entry:
  br i1 %take.left, label %left, label %right

left:
  br i1 %duplicate, label %preheader, label %preheader

right:
  %real = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %preheader

preheader:
  %seed = phi <32 x i32> [ poison, %left ], [ poison, %left ],
                           [ %real, %right ]
  call void @llvm.linx.experimental.element.region(metadata !9)
  br label %element.loop

element.loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %element.loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %seed, i64 40, i64 40, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %address = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %address, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %element.loop, label %exit, !llvm.loop !10

exit:
  ret void
}

; CHECK-LABEL: define void @duplicate_predecessor_edges
; CHECK: left:
; CHECK: [[DUP_SCALAR:%.*]] = freeze i32 0
; CHECK: [[DUP_BITS:%.*]] = zext i32 [[DUP_SCALAR]] to i64
; CHECK: [[DUP_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[DUP_BITS]], i64 0)
; CHECK-NOT: @llvm.linx.experimental.ew.tci
; CHECK: br i1 {{.*}}, label %preheader, label %preheader
; CHECK: right:
; CHECK: [[DUP_REAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: preheader:
; CHECK: %seed = phi <32 x i32> [ [[DUP_ZERO]], %left ], [ [[DUP_ZERO]], %left ], [ [[DUP_REAL]], %right ]
; CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; CHECK: ret void

define void @nested_view_phi_chain(i32 %choice, ptr noalias %output) {
entry:
  switch i32 %choice, label %actual.path [
    i32 0, label %wrapped.path
    i32 1, label %second.empty
  ]

wrapped.path:
  %undef.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> undef, i64 30, i64 30, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %first.merge

actual.path:
  %actual = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %first.merge

first.merge:
  %inner = phi <32 x i32> [ %undef.view, %wrapped.path ],
                            [ %actual, %actual.path ]
  br label %preheader

second.empty:
  %poison.view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> poison, i64 31, i64 31, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  br label %preheader

preheader:
  %outer = phi <32 x i32> [ %inner, %first.merge ],
                            [ %poison.view, %second.empty ]
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %element.loop

element.loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %element.loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %outer, i64 32, i64 32, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %address = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %address, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %element.loop, label %exit, !llvm.loop !7

exit:
  ret void
}

; CHECK-LABEL: define void @nested_view_phi_chain
; CHECK: wrapped.path:
; CHECK: [[INNER_SCALAR:%.*]] = freeze i32 0
; CHECK: [[INNER_BITS:%.*]] = zext i32 [[INNER_SCALAR]] to i64
; CHECK: [[INNER_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[INNER_BITS]], i64 0)
; CHECK: actual.path:
; CHECK: [[CHAIN_ACTUAL:%.*]] = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: first.merge:
; CHECK: %inner = phi <32 x i32> [ [[INNER_ZERO]], %wrapped.path ], [ [[CHAIN_ACTUAL]], %actual.path ]
; CHECK: second.empty:
; CHECK: [[OUTER_SCALAR:%.*]] = freeze i32 0
; CHECK: [[OUTER_BITS:%.*]] = zext i32 [[OUTER_SCALAR]] to i64
; CHECK: [[OUTER_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[OUTER_BITS]], i64 0)
; CHECK: preheader:
; CHECK: %outer = phi <32 x i32> [ %inner, %first.merge ], [ [[OUTER_ZERO]], %second.empty ]
; CHECK-NOT: llvm.linx.experimental.element.view
; CHECK-NOT: phi <32 x i32> [ undef
; CHECK-NOT: phi <32 x i32> [ poison
; CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; CHECK: ret void

define void @loop_carried_import(i32 %rounds, i1 %replace,
                                 ptr noalias %output) {
entry:
  br label %seed.loop

seed.loop:
  %round = phi i32 [ 0, %entry ], [ %round.next, %seed.latch ]
  %carrier = phi <32 x i32> [ poison, %entry ],
                              [ %next.carrier, %seed.latch ]
  %continue = icmp ult i32 %round, %rounds
  br i1 %continue, label %seed.test, label %preheader

seed.test:
  br i1 %replace, label %seed.update, label %seed.keep

seed.update:
  %loop.seed = call <32 x i32> asm sideeffect "", "=@2Tr"()
  br label %seed.latch

seed.keep:
  br label %seed.latch

seed.latch:
  %next.carrier = phi <32 x i32> [ %loop.seed, %seed.update ],
                                   [ %carrier, %seed.keep ]
  %round.next = add nuw i32 %round, 1
  br label %seed.loop

preheader:
  %import = phi <32 x i32> [ %carrier, %seed.loop ]
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %element.loop

element.loop:
  %element = phi i32 [ 0, %preheader ], [ %next, %element.loop ]
  %view = call <32 x i32> @llvm.linx.experimental.element.view.v32i32(
      <32 x i32> %import, i64 2, i64 2, i64 0, i64 128, i64 25,
      i64 32, i64 1, i64 29)
  %value = extractelement <32 x i32> %view, i32 %element
  %address = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %address, align 4
  %next = add nuw nsw i32 %element, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %element.loop, label %exit, !llvm.loop !4

exit:
  ret void
}

; CHECK-LABEL: define void @loop_carried_import
; CHECK: entry:
; CHECK: [[EXIT_SCALAR:%.*]] = freeze i32 0
; CHECK: [[EXIT_BITS:%.*]] = zext i32 [[EXIT_SCALAR]] to i64
; CHECK: [[EXIT_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[EXIT_BITS]], i64 0)
; CHECK: br i1 {{.*}}, label %preheader, label %seed.test.lr.ph
; CHECK: seed.test.lr.ph:
; CHECK: [[LOOP_SCALAR:%.*]] = freeze i32 0
; CHECK: [[LOOP_BITS:%.*]] = zext i32 [[LOOP_SCALAR]] to i64
; CHECK: [[LOOP_ZERO:%.*]] = call <32 x i32> @llvm.linx.experimental.ew.tci.v32i32(i64 32, i64 1, i64 25, i64 29, i64 [[LOOP_BITS]], i64 0)
; CHECK: seed.test:
; CHECK: %carrier3 = phi <32 x i32> [ [[LOOP_ZERO]], %seed.test.lr.ph ], [ %next.carrier, %seed.latch ]
; CHECK: seed.update:
; CHECK: %loop.seed = call <32 x i32> asm sideeffect "", "=@2Tr"()
; CHECK: seed.latch:
; CHECK: %next.carrier = phi <32 x i32> [ %loop.seed, %seed.update ], [ %carrier3, %seed.keep ]
; CHECK: preheader:
; CHECK: %import = phi <32 x i32> [ {{.*}}, %seed.loop.preheader_crit_edge ], [ [[EXIT_ZERO]], %entry ]
; CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
!6 = distinct !{}
!7 = distinct !{!7, !8}
!8 = !{!"llvm.loop.linx.pto.element.region", !6}
!9 = distinct !{}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.linx.pto.element.region", !9}

