; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj -o %t

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)

define void @dynamic_diamond(ptr noalias %output, i32 %limit) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  br label %inner
inner:
  %iteration = phi i32 [ 0, %header ], [ %increment, %join ]
  %sum = phi i32 [ 0, %header ], [ %addition, %join ]
  %less = icmp ult i32 %iteration, %limit
  br i1 %less, label %test, label %inner.exit
test:
  %value = add i32 %element, %iteration
  %condition = icmp ult i32 %element, 16
  br i1 %condition, label %left, label %right
left:
  %left.value = add i32 %value, 1
  br label %join
right:
  %right.value = add i32 %value, 2
  br label %join
join:
  %selected = phi i32 [ %left.value, %left ], [ %right.value, %right ]
  %addition = add i32 %sum, %selected
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

; CHECK-LABEL: define void @dynamic_diamond
; CHECK: pto.loop.scheduler:
; CHECK-COUNT-2: phi <32 x i32>
; CHECK-NOT: phi <32 x i32>
; CHECK: [[ACTIVE:%.*]] = select <32 x i1> {{.*}}, <32 x i1> zeroinitializer
; CHECK: [[VALUE:%.*]] = call <32 x i32> @llvm.vp.add{{.*}}<32 x i1> [[ACTIVE]], i32 32)
; CHECK-NOT: pto.loop.retain{{.*}}[[VALUE]],
; CHECK: call <32 x i32> @llvm.vp.add{{.*}}<32 x i1> [[ACTIVE]], i32 32)
; CHECK: call i1 @llvm.vector.reduce.or.v32i1
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
