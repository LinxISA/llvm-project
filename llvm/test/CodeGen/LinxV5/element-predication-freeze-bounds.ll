; RUN: split-file %s %t
; RUN: opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication,verify' -S %t/bounded.ll | FileCheck %s --check-prefix=GOOD
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/poison.ll 2>&1 | FileCheck %s --check-prefix=BAD
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/overflow.ll 2>&1 | FileCheck %s --check-prefix=BAD
; RUN: not opt -mtriple=linx64v5 -passes='loop-simplify,lcssa,linx-v5-element-predication' -disable-output %t/phi-overflow.ll 2>&1 | FileCheck %s --check-prefix=BAD
; GOOD-LABEL: define void @bounded
; GOOD-NOT: call void @llvm.trap
; GOOD: @llvm.vp.scatter
; BAD: unsupported opcode or effect in generic IR stage: call llvm.trap

;--- bounded.ll

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()
define void @bounded(ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%next, %latch]
  %shift = shl nuw nsw i32 %i, 3
  %rows = and i32 %shift, 496
  %cols = add nuw nsw i32 %rows, 14
  %low = and i32 %i, 1
  %index = or i32 %cols, %low
  %f = freeze i32 %index
  %ok = icmp ult i32 %f, 512
  br i1 %ok, label %body, label %trap
trap:
  call void @llvm.trap()
  br label %body
body:
  %p = getelementptr i32, ptr %output, i32 %i
  store i32 %f, ptr %p
  br label %latch
latch:
  %next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %next, 64
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- poison.ll

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()
define void @poison(ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%next, %latch]
  %index = add i32 poison, %i
  %f = freeze i32 %index
  %ok = icmp ult i32 %f, 512
  br i1 %ok, label %body, label %trap
trap:
  call void @llvm.trap()
  br label %body
body:
  %p = getelementptr i32, ptr %output, i32 %i
  store i32 %f, ptr %p
  br label %latch
latch:
  %next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %next, 64
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- overflow.ll

target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()
define void @overflow(ptr %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%next, %latch]
  %bad = add nuw i32 %i, -1
  %index = and i32 %bad, 511
  %f = freeze i32 %index
  %ok = icmp ult i32 %f, 512
  br i1 %ok, label %body, label %trap
trap:
  call void @llvm.trap()
  br label %body
body:
  %p = getelementptr i32, ptr %output, i32 %i
  store i32 %f, ptr %p
  br label %latch
latch:
  %next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %next, 64
  br i1 %done, label %exit, label %loop, !llvm.loop !1
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}

;--- phi-overflow.ll
target triple = "linx64v5"
declare void @llvm.linx.experimental.element.region(metadata)
declare void @llvm.trap()
define void @phi_overflow(ptr %output) {
entry:
  br label %outer
outer:
  %part = phi i32 [0, %entry], [%part.next, %outer.latch]
  %poisonable = phi i32 [0, %entry], [%bad.next, %outer.latch]
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop
loop:
  %i = phi i32 [0, %outer], [%next, %latch]
  %index = and i32 %poisonable, 511
  %f = freeze i32 %index
  %ok = icmp ult i32 %f, 512
  br i1 %ok, label %body, label %trap
trap:
  call void @llvm.trap()
  br label %body
body:
  %p = getelementptr i32, ptr %output, i32 %i
  store i32 %f, ptr %p
  br label %latch
latch:
  %next = add nuw nsw i32 %i, 1
  %done = icmp eq i32 %next, 64
  br i1 %done, label %outer.latch, label %loop, !llvm.loop !1
outer.latch:
  %part.next = add nuw nsw i32 %part, 1
  %bad.next = add nsw i32 %poisonable, 2147483647
  %outer.done = icmp eq i32 %part.next, 3
  br i1 %outer.done, label %exit, label %outer
exit:
  ret void
}
!0 = distinct !{}
!1 = distinct !{!1, !2}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
