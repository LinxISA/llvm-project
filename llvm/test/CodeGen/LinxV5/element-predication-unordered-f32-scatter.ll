; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,verify' -verify-each -S %s | FileCheck %s --check-prefix=PRED
; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S %s | FileCheck %s --check-prefix=FINAL
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

!0 = distinct !{}
!1 = distinct !{!1, !2, !3, !4}
!2 = !{!"llvm.loop.linx.pto.element.region", !0}
!3 = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
!4 = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
