; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-remarks=%t.remarks.json < %s > /dev/null
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

; TSVC control-flow inspired shapes. These are intentionally compiler-boundary
; checks, not grouped runtime positives.

define void @masked_update_mul(ptr nocapture %a,
                               ptr nocapture readonly %b,
                               ptr nocapture readonly %c) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %cond = fcmp ogt float %bv, 0.0
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  %cv = load float, ptr %cp, align 4
  %mul = fmul float %bv, %cv
  %sum = fadd float %av, %mul
  %out = select i1 %cond, float %sum, float %av
  store float %out, ptr %ap, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @nested_dual_update(ptr nocapture %a,
                                ptr nocapture %b,
                                ptr nocapture %c,
                                ptr nocapture readonly %d,
                                ptr nocapture readonly %e,
                                i64 %x) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %join2 ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %cmp0 = fcmp ogt float %av, %bv
  br i1 %cmp0, label %then0, label %else0

then0:
  %dp0 = getelementptr inbounds float, ptr %d, i64 %i
  %dv0 = load float, ptr %dp0, align 4
  %mul0 = fmul float %bv, %dv0
  %an = fadd float %av, %mul0
  store float %an, ptr %ap, align 4
  %cmp1 = icmp sgt i64 64, 10
  br i1 %cmp1, label %then1, label %else1

then1:
  %cp1 = getelementptr inbounds float, ptr %c, i64 %i
  %cv1 = load float, ptr %cp1, align 4
  %mul1 = fmul float %dv0, %dv0
  %cn1 = fadd float %cv1, %mul1
  store float %cn1, ptr %cp1, align 4
  br label %join2

else1:
  %ep1 = getelementptr inbounds float, ptr %e, i64 %i
  %ev1 = load float, ptr %ep1, align 4
  %mul2 = fmul float %dv0, %ev1
  %cn2 = fadd float %mul2, 1.0
  %cp2 = getelementptr inbounds float, ptr %c, i64 %i
  store float %cn2, ptr %cp2, align 4
  br label %join2

else0:
  %ep0 = getelementptr inbounds float, ptr %e, i64 %i
  %ev0 = load float, ptr %ep0, align 4
  %mul3 = fmul float %ev0, %ev0
  %bn = fadd float %av, %mul3
  store float %bn, ptr %bp, align 4
  %cmp2 = icmp sgt i64 %x, 0
  br i1 %cmp2, label %then2, label %else2

then2:
  %dp2 = getelementptr inbounds float, ptr %d, i64 %i
  %dv2 = load float, ptr %dp2, align 4
  %mul4 = fmul float %dv2, %dv2
  %cn3 = fadd float %av, %mul4
  %cp3 = getelementptr inbounds float, ptr %c, i64 %i
  store float %cn3, ptr %cp3, align 4
  br label %join2

else2:
  %cp4 = getelementptr inbounds float, ptr %c, i64 %i
  %cv4 = load float, ptr %cp4, align 4
  %mul5 = fmul float %ev0, %ev0
  %cn4 = fadd float %cv4, %mul5
  store float %cn4, ptr %cp4, align 4
  br label %join2

join2:
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit2, label %loop

exit2:
  ret void
}

define void @independent_conditional_dual_store(ptr nocapture %a,
                                                ptr nocapture %b,
                                                ptr nocapture readonly %c,
                                                ptr nocapture readonly %d,
                                                ptr nocapture readonly %e,
                                                float %t) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ep = getelementptr inbounds float, ptr %e, i64 %i
  %ev = load float, ptr %ep, align 4
  %cond = fcmp oge float %ev, %t
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  %cv = load float, ptr %cp, align 4
  %dp = getelementptr inbounds float, ptr %d, i64 %i
  %dv = load float, ptr %dp, align 4
  %cmul = fmul float %cv, %dv
  %an = fadd float %av, %cmul
  %outa = select i1 %cond, float %an, float %av
  store float %outa, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %csq = fmul float %cv, %cv
  %bn = fadd float %bv, %csq
  %outb = select i1 %cond, float %bn, float %bv
  store float %outb, ptr %bp, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit3, label %loop

exit3:
  ret void
}

define void @dependent_conditional_update(ptr nocapture %a,
                                          ptr nocapture %b,
                                          ptr nocapture %c,
                                          ptr nocapture readonly %d,
                                          ptr nocapture readonly %e) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %cp = getelementptr inbounds float, ptr %c, i64 %i
  %cv = load float, ptr %cp, align 4
  %ep = getelementptr inbounds float, ptr %e, i64 %i
  %ev = load float, ptr %ep, align 4
  %dp = getelementptr inbounds float, ptr %d, i64 %i
  %dv = load float, ptr %dp, align 4
  %mul0 = fmul float %ev, %dv
  %newa = fadd float %cv, %mul0
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  store float %newa, ptr %ap, align 4
  %cond = fcmp ogt float %newa, 0.0
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %newb = fadd float %newa, %bv
  %outa = select i1 %cond, float %newa, float %mul0
  %outb = select i1 %cond, float %newb, float %bv
  store float %outa, ptr %ap, align 4
  store float %outb, ptr %bp, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit4, label %loop

exit4:
  ret void
}

; REMARK: "function":"masked_update_mul"
; REMARK: "status":"reject"
; REMARK: "reason":"unsupported_value_expr:select"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
; REMARK: "function":"nested_dual_update"
; REMARK: "status":"reject"
; REMARK: "reason":"unsupported_value_expr:fadd"
; REMARK: "layout_kind":"scalar-replay"
; REMARK: "cf_strategy":"exec-mask-save-restore-required"
; REMARK: "function":"independent_conditional_dual_store"
; REMARK: "status":"reject"
; REMARK: "reason":"unsupported_value_expr:select"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
; REMARK: "function":"dependent_conditional_update"
; REMARK: "status":"reject"
; REMARK: "reason":"unsupported_value_expr:select"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
