; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-region-verify' -disable-output %s 2>&1 | FileCheck %s --check-prefix=RESIDUAL
;
; This is the explicit generic IR stage, not the target lowering. Retaining the
; mandatory marker ensures the final target verifier cannot silently accept it.
; RESIDUAL: required region lowering did not complete

declare void @llvm.linx.experimental.element.region(metadata)

define void @three_way_phi(ptr noalias %input, ptr noalias %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %header
header:
  %i = phi i64 [0, %entry], [%next, %latch]
  %p = getelementptr i32, ptr %input, i64 %i
  %x = load i32, ptr %p, align 4
  %negative = icmp slt i32 %x, 0
  br i1 %negative, label %negative.arm, label %nonnegative
negative.arm:
  %negated = sub i32 0, %x
  br label %merge
nonnegative:
  %zero = icmp eq i32 %x, 0
  br i1 %zero, label %merge, label %positive.arm
positive.arm:
  %quotient = sdiv i32 100, %x
  br label %merge
merge:
  %value = phi i32 [%negated, %negative.arm], [7, %nonnegative], [%quotient, %positive.arm]
  %q = getelementptr i32, ptr %output, i64 %i
  store i32 %value, ptr %q, align 4
  br label %latch
latch:
  %next = add nuw i64 %i, 1
  %again = icmp ult i64 %next, 4
  br i1 %again, label %header, label %exit, !llvm.loop !1
exit:
  ret void
}
; CHECK-LABEL: define void @three_way_phi
; CHECK: call void @llvm.linx.experimental.element.region
; CHECK: call <4 x i32> @llvm.vp.gather
; CHECK: icmp slt <4 x i32>
; CHECK: select <4 x i1>
; CHECK: call <4 x i32> @llvm.vp.sdiv
; CHECK: pto.phi
; CHECK: call void @llvm.vp.scatter
; CHECK-NOT: phi i64
; CHECK: ret void

define void @tail_33(ptr noalias %input, ptr noalias %output, i64 %valid, i1 %enabled, i32 %divisor) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !2)
  br label %header
header:
  %i = phi i64 [0, %entry], [%next, %latch]
  br i1 %enabled, label %test, label %latch
test:
  %active = icmp ult i64 %i, %valid
  br i1 %active, label %body, label %latch
body:
  %p = getelementptr i32, ptr %input, i64 %i
  %x = load i32, ptr %p, align 4
  %value = udiv i32 %x, %divisor
  %q = getelementptr i32, ptr %output, i64 %i
  store i32 %value, ptr %q, align 4
  br label %latch
latch:
  %next = add nuw i64 %i, 1
  %again = icmp ult i64 %next, 33
  br i1 %again, label %header, label %exit, !llvm.loop !3
exit:
  ret void
}
; CHECK-LABEL: define void @tail_33
; CHECK: select <33 x i1>
; CHECK: call <33 x i32> @llvm.vp.gather
; CHECK: call <33 x i32> @llvm.vp.udiv
; CHECK: call void @llvm.vp.scatter
; CHECK-NOT: phi i64
; CHECK: ret void

define void @tail_129(ptr noalias %input, ptr noalias %output, i64 %valid, i1 %enabled, i32 %divisor) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !4)
  br label %header
header:
  %i = phi i64 [0, %entry], [%next, %latch]
  br i1 %enabled, label %test, label %latch
test:
  %active = icmp ult i64 %i, %valid
  br i1 %active, label %body, label %latch
body:
  %p = getelementptr i32, ptr %input, i64 %i
  %x = load i32, ptr %p, align 4
  %value = udiv i32 %x, %divisor
  %q = getelementptr i32, ptr %output, i64 %i
  store i32 %value, ptr %q, align 4
  br label %latch
latch:
  %next = add nuw i64 %i, 1
  %again = icmp ult i64 %next, 129
  br i1 %again, label %header, label %exit, !llvm.loop !5
exit:
  ret void
}
; CHECK-LABEL: define void @tail_129
; CHECK: select <129 x i1>
; CHECK: call <129 x i32> @llvm.vp.gather
; CHECK: call <129 x i32> @llvm.vp.udiv
; CHECK: call void @llvm.vp.scatter
; CHECK-NOT: phi i64
; CHECK: ret void

define void @two_regions(ptr noalias %data) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !6)
  br label %first
first:
  %i = phi i64 [0, %entry], [%next, %first]
  %p = getelementptr i32, ptr %data, i64 %i
  %x = load i32, ptr %p, align 4
  %y = add i32 %x, 3
  store i32 %y, ptr %p, align 4
  %next = add nuw i64 %i, 1
  %again = icmp ult i64 %next, 4
  br i1 %again, label %first, label %bridge, !llvm.loop !7
bridge:
  call void @llvm.linx.experimental.element.region(metadata !8)
  br label %second
second:
  %j = phi i64 [0, %bridge], [%next2, %second]
  %q = getelementptr i32, ptr %data, i64 %j
  %a = load i32, ptr %q, align 4
  %b = mul i32 %a, 5
  store i32 %b, ptr %q, align 4
  %next2 = add nuw i64 %j, 1
  %again2 = icmp ult i64 %next2, 4
  br i1 %again2, label %second, label %exit, !llvm.loop !9
exit:
  ret void
}
; CHECK-LABEL: define void @two_regions
; CHECK: call void @llvm.linx.experimental.element.region
; CHECK: call <4 x i32> @llvm.vp.add
; CHECK: call void @llvm.vp.scatter
; CHECK: call void @llvm.linx.experimental.element.region
; CHECK: call <4 x i32> @llvm.vp.mul
; CHECK: call void @llvm.vp.scatter
; CHECK-NOT: phi i64
; CHECK: ret void

!0 = distinct !{}
!1 = distinct !{!1, !10}
!10 = !{!"llvm.loop.linx.pto.element.region", !0}

!2 = distinct !{}
!3 = distinct !{!3, !12}
!12 = !{!"llvm.loop.linx.pto.element.region", !2}

!4 = distinct !{}
!5 = distinct !{!5, !14}
!14 = !{!"llvm.loop.linx.pto.element.region", !4}

!6 = distinct !{}
!7 = distinct !{!7, !16}
!16 = !{!"llvm.loop.linx.pto.element.region", !6}

!8 = distinct !{}
!9 = distinct !{!9, !18}
!18 = !{!"llvm.loop.linx.pto.element.region", !8}
