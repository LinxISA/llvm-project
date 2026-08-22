; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_inner_diamond(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %merge ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %cond = fcmp ogt float %bv, 0.000000e+00
  br i1 %cond, label %then, label %else

then:
  %t = fadd float %bv, 1.000000e+00
  br label %merge

else:
  %e = fsub float 0.000000e+00, %bv
  br label %merge

merge:
  %sel = phi float [ %t, %then ], [ %e, %else ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  store float %sel, ptr %ap, align 4
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

define void @vector_nested_diamond(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %merge1 ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %gt0 = fcmp ogt float %bv, 0.000000e+00
  br i1 %gt0, label %then0, label %else0

then0:
  %gt10 = fcmp ogt float %bv, 1.000000e+01
  br i1 %gt10, label %then1, label %else1

then1:
  br label %merge0

else1:
  br label %merge0

merge0:
  %hi = phi float [ 1.000000e+00, %then1 ], [ 2.000000e+00, %else1 ]
  br label %merge1

else0:
  br label %merge1

merge1:
  %sel = phi float [ %hi, %merge0 ], [ 3.000000e+00, %else0 ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  store float %sel, ptr %ap, align 4
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; ASM-LABEL: vector_inner_diamond:
; ASM-NOT: BSTART.MSEQ

; ASM-LABEL: vector_nested_diamond:
; ASM-NOT: BSTART.MSEQ

; REMARK: "function":"vector_inner_diamond"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-diamond"

; REMARK: "function":"vector_nested_diamond"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-diamond"
