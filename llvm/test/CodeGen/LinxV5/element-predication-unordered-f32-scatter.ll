; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=PRED
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=FINAL
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s -o %t.ll
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=obj %t.ll -o %t.o
; RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
;
; Duplicate destination indices have no LLVM vp.scatter ordering contract here.
; The source region instead owns one isolated unordered f32 store effect.

target triple = "linx64v5"

declare void @llvm.linx.experimental.element.region(metadata)

define void @f32_scatter(ptr noalias %input, ptr noalias %indices,
                         ptr noalias %output, i1 %enabled) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !0)
  br label %loop

loop:
  %e = phi i32 [ 0, %entry ], [ %next, %latch ]
  %input.ptr = getelementptr float, ptr %input, i32 %e
  %value = load float, ptr %input.ptr, align 4
  %index.ptr = getelementptr i32, ptr %indices, i32 %e
  %index = load i32, ptr %index.ptr, align 4
  %destination = getelementptr float, ptr %output, i32 %index
  br i1 %enabled, label %store, label %latch

store:
  store float %value, ptr %destination, align 4
  br label %latch

latch:
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 32
  br i1 %more, label %loop, label %exit, !llvm.loop !1

exit:
  ret void
}

define void @f16_scatter(ptr noalias %input, ptr noalias %indices,
                         ptr noalias %output, i1 %enabled) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !5)
  br label %loop

loop:
  %e = phi i32 [ 0, %entry ], [ %next, %latch ]
  %input.ptr = getelementptr half, ptr %input, i32 %e
  %value = load half, ptr %input.ptr, align 2
  br i1 %enabled, label %store, label %latch

store:
  %index.ptr = getelementptr i32, ptr %indices, i32 %e
  %index = load i32, ptr %index.ptr, align 4
  %destination = getelementptr half, ptr %output, i32 %index
  store half %value, ptr %destination, align 2
  br label %latch

latch:
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 64
  br i1 %more, label %loop, label %exit, !llvm.loop !6

exit:
  ret void
}

define void @u16_scatter(ptr noalias %input, ptr noalias %indices,
                         ptr noalias %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !10)
  br label %loop
loop:
  %e = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.ptr = getelementptr i16, ptr %input, i32 %e
  %value = load i16, ptr %input.ptr, align 2
  %index.ptr = getelementptr i32, ptr %indices, i32 %e
  %index = load i32, ptr %index.ptr, align 4
  %destination = getelementptr i16, ptr %output, i32 %index
  store i16 %value, ptr %destination, align 2
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 64
  br i1 %more, label %loop, label %exit, !llvm.loop !11
exit:
  ret void
}

define void @u32_logical64_scatter(ptr noalias %input, ptr noalias %indices,
                                   ptr noalias %output) {
entry:
  call void @llvm.linx.experimental.element.region(metadata !15)
  br label %loop
loop:
  %e = phi i32 [ 0, %entry ], [ %next, %loop ]
  %input.ptr = getelementptr i32, ptr %input, i32 %e
  %value = load i32, ptr %input.ptr, align 4
  %index.ptr = getelementptr i32, ptr %indices, i32 %e
  %index = load i32, ptr %index.ptr, align 4
  %destination = getelementptr i32, ptr %output, i32 %index
  store i32 %value, ptr %destination, align 4
  %next = add nuw nsw i32 %e, 1
  %more = icmp ult i32 %next, 64
  br i1 %more, label %loop, label %exit, !llvm.loop !16
exit:
  ret void
}

; PRED-LABEL: define void @f32_scatter
; PRED: call void @llvm.linx.experimental.element.scatter.v32f32.v32p0
; PRED-SAME: metadata [[TOKEN:![0-9]+]])
; PRED-SAME: !llvm.loop.linx.pto.element.inter_element_order
; PRED-SAME: !llvm.loop.linx.pto.element.intra_element_order
; PRED-SAME: !linx.pto.element.vp.owner [[TOKEN]]
; PRED-NOT: @llvm.vp.scatter
; PRED: ret void
;
; FINAL-LABEL: define void @f32_scatter
; FINAL: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; FINAL: call <32 x float> @llvm.linx.experimental.ew.mgather.gpr.masked
; FINAL: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
; FINAL-NOT: llvm.linx.experimental.element.scatter
; FINAL-NOT: llvm.vp.scatter
; FINAL-NOT: llvm.linx.experimental.element.region
; FINAL: ret void
;
; PRED-LABEL: define void @f16_scatter
; PRED: call void @llvm.linx.experimental.element.scatter.v64f16.v64p0
; PRED-SAME: i32 64, metadata [[TOKEN64:![0-9]+]])
; PRED-SAME: !linx.pto.element.vp.owner [[TOKEN64]]
; PRED-NOT: @llvm.vp.scatter
; PRED: ret void
;
; FINAL-LABEL: define void @f16_scatter
; FINAL: call <64 x half> @llvm.linx.experimental.ew.mgather.gpr.masked
; FINAL: call <64 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
; FINAL: call <64 x i64> @llvm.linx.experimental.ew.tlea.v64i64.v64i32{{.*}}i64 16)
; FINAL: call void @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 32, i64 2, i64 4, i64 29, i64 16,
; FINAL-NOT: llvm.linx.experimental.element.scatter
; FINAL-NOT: llvm.vp.scatter
; FINAL: ret void
;
; OBJ: BSTART.TLSU MSCATTER, FP16
;
; FINAL-LABEL: define void @u16_scatter
; FINAL: call void @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 32, i64 2, i64 26, i64 29, i64 16,
; FINAL-LABEL: define void @u32_logical64_scatter
; FINAL: call void @llvm.linx.experimental.ew.mscatter.gpr.masked{{.*}}i64 32, i64 2, i64 25, i64 29, i64 16,
; OBJ: BSTART.TLSU MSCATTER, U16
; OBJ: BSTART.TLSU MSCATTER, U32

!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
!5 = distinct !{}
!6 = distinct !{!6, !7, !8, !9}
!7 = !{!"llvm.loop.linx.pto.element.region", !5}
!8 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!9 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
!10 = distinct !{}
!11 = distinct !{!11, !12, !13, !14}
!12 = !{!"llvm.loop.linx.pto.element.region", !10}
!13 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!14 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
!15 = distinct !{}
!16 = distinct !{!16, !17, !18, !19}
!17 = !{!"llvm.loop.linx.pto.element.region", !15}
!18 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!19 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
