; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=auto --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json
; RUN: rm -f %t.grouped.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.grouped.remarks.json < %s | FileCheck %s --check-prefix=GROUPED-ASM
; RUN: FileCheck %s --check-prefix=GROUPED-REMARK < %t.grouped.remarks.json
; RUN: rm -f %t.mpar-safe.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mpar-safe --linx-simt-autovec-layout=auto --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.mpar-safe.remarks.json < %s | FileCheck %s --check-prefix=MPAR-SAFE-ASM
; RUN: FileCheck %s --check-prefix=MPAR-SAFE-REMARK < %t.mpar-safe.remarks.json

define float @constant_product_recurrence() {
entry:
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %inc, %loop ]
  %q = phi float [ 1.000000e+00, %entry ], [ %q.next, %loop ]
  %q.next = fmul float %q, 0x3FEFAE1480000000
  %inc = add nuw nsw i32 %i, 1
  %done = icmp ult i32 %inc, 160
  br i1 %done, label %loop, label %exit, !llvm.loop !0

exit:
  ret float %q.next
}

; Recurrences are order-dependent. Auto layout must replay all 160 iterations
; through one lane instead of treating the recurrence slot as 32 independent
; lane states across five groups.
; ASM-LABEL: constant_product_recurrence:
; ASM: BSTART.MSEQ
; ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; ASM: C.B.DIMI{{[[:space:]]+}}160,{{.*->lb1}}
; ASM-NOT: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; ASM-NOT: C.B.DIMI{{[[:space:]]+}}5,{{.*->lb1}}
; ASM: v.lw.brg [ri{{[0-9]+}}, lc0<<2, zero<<2], ->[[Q:vt#[0-9]+]]
; ASM: v.fmul [[Q]], {{.*}}, ->[[NEXT:vt#[0-9]+]]
; ASM: v.add {{.*}}, ->[[IVNEXT:vt#[0-9]+]]
; ASM-NOT: v.sw.brg [[IVNEXT]], [ri{{[0-9]+}}, lc0<<2, zero<<2]
; ASM: v.sw.brg [[NEXT]], [ri{{[0-9]+}}, lc0<<2, zero<<2]

; REMARK: "function":"constant_product_recurrence"
; REMARK: "status":"lowered"
; REMARK: "lane_count":1
; REMARK: "group_count":160
; REMARK: "has_recurrence":true
; REMARK: "header_kind":"mseq"
; REMARK: "touches_memory":true
; REMARK: "layout_kind":"scalar-replay"

; GROUPED-ASM-LABEL: constant_product_recurrence:
; GROUPED-ASM-NOT: BSTART.MSEQ
; GROUPED-ASM-NOT: BSTART.VSEQ

; GROUPED-REMARK: "function":"constant_product_recurrence"
; GROUPED-REMARK: "status":"reject"
; GROUPED-REMARK: "reason":"grouped_layout_unsupported_recurrence"
; GROUPED-REMARK: "layout_policy":"grouped"

; A parallel-safe hint must not select MPAR for an ordered recurrence.
; MPAR-SAFE-ASM-LABEL: constant_product_recurrence:
; MPAR-SAFE-ASM: BSTART.MSEQ
; MPAR-SAFE-ASM-NOT: BSTART.MPAR
; MPAR-SAFE-ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; MPAR-SAFE-ASM: C.B.DIMI{{[[:space:]]+}}160,{{.*->lb1}}

; MPAR-SAFE-REMARK: "function":"constant_product_recurrence"
; MPAR-SAFE-REMARK: "selected_mode":"mseq"
; MPAR-SAFE-REMARK: "lane_count":1
; MPAR-SAFE-REMARK: "group_count":160
; MPAR-SAFE-REMARK: "has_recurrence":true
; MPAR-SAFE-REMARK: "header_kind":"mseq"
; MPAR-SAFE-REMARK: "layout_kind":"scalar-replay"

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.vectorize.enable", i1 true}
