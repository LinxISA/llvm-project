; RUN: opt -mtriple=linx64v5 -passes='always-inline,function(loop-simplify,lcssa,linx-v5-element-predication),verify' -verify-each -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -passes='always-inline,function(loop-simplify,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify),verify' -verify-each -disable-output %s

declare void @llvm.linx.experimental.element.region(metadata)

define internal void @phase(ptr %data) alwaysinline {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %element = phi i64 [ 0, %entry ], [ %next, %loop ]
  %address = getelementptr i32, ptr %data, i64 %element
  %value = load i32, ptr %address, align 4
  %result = add i32 %value, 1
  store i32 %result, ptr %address, align 4
  %next = add nuw i64 %element, 1
  %more = icmp ult i64 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1
exit:
  ret void
}

define void @divergent(ptr %left, ptr %right, i1 %flag) {
entry:
  br i1 %flag, label %left.path, label %right.path
left.path:
  call void @phase(ptr %left)
  br label %exit
right.path:
  call void @phase(ptr %right)
  br label %exit
exit:
  ret void
}

; CHECK-LABEL: define void @divergent(
; CHECK: call void @llvm.linx.experimental.element.region(metadata ![[LEFT:[0-9]+]])
; CHECK: call <32 x i32> @llvm.vp.add{{.*}}!linx.pto.element.vp.owner ![[LEFT]]
; CHECK: call void @llvm.linx.experimental.element.region(metadata ![[RIGHT:[0-9]+]])
; CHECK: call <32 x i32> @llvm.vp.add{{.*}}!linx.pto.element.vp.owner ![[RIGHT]]
; CHECK: ret void

define void @sequential(ptr %data) {
entry:
  call void @phase(ptr %data)
  call void @phase(ptr %data)
  ret void
}

; CHECK-LABEL: define void @sequential(
; CHECK: call void @llvm.linx.experimental.element.region(metadata ![[FIRST:[0-9]+]])
; CHECK: call <32 x i32> @llvm.vp.add{{.*}}!linx.pto.element.vp.owner ![[FIRST]]
; CHECK: call void @llvm.linx.experimental.element.region(metadata ![[SECOND:[0-9]+]])
; CHECK: call <32 x i32> @llvm.vp.add{{.*}}!linx.pto.element.vp.owner ![[SECOND]]
; CHECK: ret void
; CHECK-DAG: ![[LEFT]] = distinct !{}
; CHECK-DAG: ![[RIGHT]] = distinct !{}
; CHECK-DAG: ![[FIRST]] = distinct !{}
; CHECK-DAG: ![[SECOND]] = distinct !{}

!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
