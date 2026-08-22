; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_store_diamond(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %merge ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %cond = fcmp ogt float %bv, 0.000000e+00
  br i1 %cond, label %then, label %else

then:
  %t = fadd float %bv, 1.000000e+00
  store float %t, ptr %ap, align 4
  br label %merge

else:
  %e = fsub float 0.000000e+00, %bv
  store float %e, ptr %ap, align 4
  br label %merge

merge:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; ASM-LABEL: vector_store_diamond:
; ASM-NOT: BSTART.MSEQ

; REMARK: "function":"vector_store_diamond"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"
; REMARK: "layout_kind":"none"
; REMARK: "cf_strategy":"none"
