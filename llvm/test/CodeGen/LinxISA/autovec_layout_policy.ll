; RUN: rm -f %t.auto.remarks.json %t.reject.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.auto.remarks.json < %s | FileCheck %s --check-prefix=AUTO
; RUN: FileCheck %s --check-prefix=REMARK-AUTO < %t.auto.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=scalar-replay --linx-simt-autovec-lanes=32 < %s | FileCheck %s --check-prefix=SCALAR
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.reject.remarks.json < %s > /dev/null
; RUN: FileCheck %s --check-prefix=REMARK-REJECT < %t.reject.remarks.json

define void @single_group(ptr nocapture %a, ptr nocapture %b, ptr nocapture %c) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %av, %bv
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  store float %sum, ptr %cp, align 4
  %inc = add nuw i64 %i, 1
  %cmp = icmp ult i64 %inc, 8
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

define void @strip_mined(ptr nocapture %a, ptr nocapture %b, ptr nocapture %c) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %av, %bv
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  store float %sum, ptr %cp, align 4
  %inc = add nuw i64 %i, 1
  %cmp = icmp ult i64 %inc, 128
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

define void @dynamic_tripcount(ptr nocapture %a, ptr nocapture %b, ptr nocapture %c, i64 %n) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %av, %bv
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  store float %sum, ptr %cp, align 4
  %inc = add nuw i64 %i, 1
  %cmp = icmp ult i64 %inc, %n
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

; AUTO-LABEL: single_group:
; AUTO: BSTART.MSEQ
; AUTO: C.B.DIMI{{[[:space:]]+}}8,{{.*->lb0}}
; AUTO: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb1}}

; AUTO-LABEL: strip_mined:
; AUTO: BSTART.MSEQ
; AUTO: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; AUTO: C.B.DIMI{{[[:space:]]+}}4,{{.*->lb1}}
; AUTO: v.add lc0, lc1{{(\.uw)?}}<<5

; SCALAR-LABEL: single_group:
; SCALAR: BSTART.MSEQ
; SCALAR: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; SCALAR: C.B.DIMI{{[[:space:]]+}}8,{{.*->lb1}}

; REMARK-AUTO: "function":"single_group"
; REMARK-AUTO: "layout_policy":"auto"
; REMARK-AUTO: "layout_kind":"grouped-single-group"
; REMARK-AUTO: "function":"strip_mined"
; REMARK-AUTO: "layout_policy":"auto"
; REMARK-AUTO: "layout_kind":"grouped-strip-mined"

; REMARK-REJECT: "function":"dynamic_tripcount"
; REMARK-REJECT: "status":"reject"
; REMARK-REJECT: "reason":"grouped_layout_requires_static_tripcount"
; REMARK-REJECT: "layout_policy":"grouped"
