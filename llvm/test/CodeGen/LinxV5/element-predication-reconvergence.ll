; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -disable-output %s

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)

define void @nested_reconvergence(ptr noalias %output, i32 %limit, i32 %split) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %outer = icmp ult i32 %element, %limit
  br i1 %outer, label %inner, label %latch
inner:
  %condition = icmp ult i32 %element, %split
  br i1 %condition, label %left, label %right
left:
  %left.value = add i32 %element, 1
  br label %join
right:
  %right.value = add i32 %element, 2
  br label %join
join:
  %value = phi i32 [ %left.value, %left ], [ %right.value, %right ]
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %value, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}

; CHECK-LABEL: define void @nested_reconvergence
; CHECK: [[OUTER:%.*]] = icmp ult <32 x i32>
; CHECK: [[THEN:%.*]] = select <32 x i1> [[OUTER]],
; CHECK: xor <32 x i1> [[OUTER]], [[THEN]]
; CHECK-NOT: pto.block.mask
; CHECK: call void @llvm.vp.scatter{{.*}}<32 x i1> [[OUTER]], i32 32)
; CHECK-NOT: llvm.linx.blkv
; CHECK: ret void

define void @bypass_is_not_reconvergence(ptr noalias %output, i32 %limit,
                                        i32 %split) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !3)
  br label %header
header:
  %element = phi i32 [ 0, %entry ], [ %next, %latch ]
  %outer = icmp ult i32 %element, %limit
  br i1 %outer, label %inner, label %latch
inner:
  %condition = icmp ult i32 %element, %split
  br i1 %condition, label %body, label %latch
body:
  %destination = getelementptr i32, ptr %output, i32 %element
  store i32 %element, ptr %destination, align 4
  br label %latch
latch:
  %next = add nuw nsw i32 %element, 1
  %again = icmp ult i32 %next, 32
  br i1 %again, label %header, label %exit, !llvm.loop !4
exit:
  ret void
}

; CHECK-LABEL: define void @bypass_is_not_reconvergence
; CHECK: [[OUTER:%.*]] = icmp ult <32 x i32>
; CHECK: [[INNER:%.*]] = icmp ult <32 x i32>
; CHECK: [[ACTIVE:%.*]] = select <32 x i1> [[OUTER]], <32 x i1> [[INNER]], <32 x i1> zeroinitializer
; CHECK: call void @llvm.vp.scatter{{.*}}<32 x i1> [[ACTIVE]], i32 32)
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = distinct !{}
!4 = distinct !{!4, !5}
!5 = !{!"llvm.loop.linx.pto.element.region", !3}
