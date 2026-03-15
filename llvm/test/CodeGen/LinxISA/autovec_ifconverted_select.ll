; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_inner_select(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %add = fadd float %bv, 1.000000e+00
  %neg = fsub float 0.000000e+00, %bv
  %cond = fcmp ogt float %bv, 0.000000e+00
  %sel = select i1 %cond, float %add, float %neg
  store float %sel, ptr %ap, align 4
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

define void @vector_nested_select(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %gt10 = fcmp ogt float %bv, 1.000000e+01
  %hi = select i1 %gt10, float 1.000000e+00, float 2.000000e+00
  %gt0 = fcmp ogt float %bv, 0.000000e+00
  %sel = select i1 %gt0, float %hi, float 3.000000e+00
  store float %sel, ptr %ap, align 4
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; ASM-LABEL: vector_inner_select:
; ASM: BSTART.MSEQ
; ASM: B.TEXT
; ASM: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; ASM: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; ASM: v.flt
; ASM: v.csel
; ASM: v.sw.brg
; ASM-NOT: b.nz
; ASM-NOT: v.rdor

; ASM-LABEL: vector_nested_select:
; ASM: BSTART.MSEQ
; ASM: B.TEXT
; ASM: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; ASM: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; ASM: v.flt
; ASM: v.csel
; ASM: v.csel
; ASM: v.sw.brg
; ASM-NOT: b.nz
; ASM-NOT: v.rdor

; REMARK: "function":"vector_inner_select"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"

; REMARK: "function":"vector_nested_select"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
