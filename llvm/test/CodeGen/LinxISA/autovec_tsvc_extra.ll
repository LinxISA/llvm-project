; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s --check-prefix=ASM
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_shift_half_index(ptr nocapture readonly %b,
                                     ptr nocapture readonly %c,
                                     ptr nocapture writeonly %a) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %bp = getelementptr inbounds i32, ptr %b, i64 %i
  %bv = load i32, ptr %bp, align 4
  %half = lshr i64 %i, 1
  %cp = getelementptr inbounds i32, ptr %c, i64 %half
  %cv = load i32, ptr %cp, align 4
  %sum = add nsw i32 %bv, %cv
  %ap = getelementptr inbounds i32, ptr %a, i64 %i
  store i32 %sum, ptr %ap, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @vector_min_select_store(ptr nocapture readonly %a,
                                     ptr nocapture readonly %b,
                                     ptr nocapture writeonly %out) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %gt = fcmp ogt float %av, %bv
  %minv = select i1 %gt, float %bv, float %av
  %op = getelementptr inbounds float, ptr %out, i64 %i
  store float %minv, ptr %op, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @vector_shifted_out_store(ptr nocapture %a,
                                      ptr nocapture readonly %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap0 = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap0, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %av, %bv
  %outi = add nuw nsw i64 %i, 32
  %ap1 = getelementptr inbounds float, ptr %a, i64 %outi
  store float %sum, ptr %ap1, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 32
  br i1 %done, label %exit1, label %loop

exit1:
  ret void
}

define void @vector_shifted_out_param(ptr nocapture %a,
                                      ptr nocapture readonly %b,
                                      i64 %m) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ap0 = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap0, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %av, %bv
  %outi = add nuw nsw i64 %i, %m
  %ap1 = getelementptr inbounds float, ptr %a, i64 %outi
  store float %sum, ptr %ap1, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp ult i64 %inc, %m
  br i1 %done, label %loop, label %exit2

exit2:
  ret void
}

define void @vector_stride_inc(ptr nocapture %a,
                               ptr nocapture readonly %b,
                               i64 %incv) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %next, %loop ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  store float %bv, ptr %ap, align 4
  %next = add nuw nsw i64 %i, %incv
  %done = icmp ult i64 %next, 64
  br i1 %done, label %loop, label %exit3

exit3:
  ret void
}

define void @vector_dynamic_recurrence_store(ptr nocapture readonly %a,
                                             ptr nocapture readonly %b,
                                             ptr nocapture writeonly %out,
                                             i64 %n) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %carry = phi float [ 0.000000e+00, %entry ], [ %next, %loop ]
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %av = load float, ptr %ap, align 4
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %mul = fmul float %av, %bv
  %next = fadd float %carry, %mul
  %op = getelementptr inbounds float, ptr %out, i64 %i
  store float %next, ptr %op, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp ult i64 %inc, %n
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; REMARK: "function":"vector_shift_half_index"
; REMARK: "status":"reject"
; REMARK: "reason":"unsupported_value_expr:add"
; REMARK: "layout_kind":"scalar-replay"
; REMARK: "cf_strategy":"straight-line-single-block"
; REMARK: "function":"vector_min_select_store"
; REMARK: "status":"lowered"
; REMARK: "reason":"lowered_vblock_mseq_affine"
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
; REMARK: "function":"vector_shifted_out_store"
; REMARK: "status":"lowered"
; REMARK: "layout_kind":"grouped-single-group"
; REMARK: "cf_strategy":"straight-line-single-block"
; REMARK: "function":"vector_shifted_out_param"
; REMARK: "status":"lowered"
; REMARK: "layout_kind":"scalar-replay"
; REMARK: "cf_strategy":"straight-line-single-block"
; REMARK: "function":"vector_stride_inc"
; REMARK: "status":"lowered"
; REMARK: "layout_kind":"scalar-replay"
; REMARK: "cf_strategy":"straight-line-single-block"
; REMARK: "function":"vector_dynamic_recurrence_store"
; REMARK: "status":"lowered"
; REMARK: "layout_kind":"scalar-replay"
; REMARK: "cf_strategy":"straight-line-single-block"

; ASM-LABEL: vector_shift_half_index:
; ASM: C.BSTART.STD
; ASM-NOT: BSTART.MSEQ
; ASM: addw
; ASM: c.swi

; ASM-LABEL: vector_min_select_store:
; ASM: BSTART.MSEQ
; ASM: B.TEXT
; ASM: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; ASM: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; ASM: v.flt
; ASM: v.csel
; ASM: v.sw.brg
; ASM-NOT: v.rdor

; ASM-LABEL: vector_shifted_out_store:
; ASM: BSTART.MSEQ
; ASM: B.TEXT
; ASM: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb1}}
; ASM: v.fadd
; ASM: v.sw.brg

; ASM-LABEL: vector_shifted_out_param:
; ASM: BSTART.MSEQ
; ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; ASM: B.DIM
; ASM: v.fadd
; ASM: v.sw.brg

; ASM-LABEL: vector_stride_inc:
; ASM: BSTART.MSEQ
; ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; ASM: B.DIM{{[[:space:]]+}}a3,{{[[:space:]]+}}0,{{[[:space:]]+}}->lb1
; ASM: v.mul
; ASM: v.sw.brg

; ASM-LABEL: vector_dynamic_recurrence_store:
; ASM: BSTART.MSEQ
; ASM: C.B.DIMI{{[[:space:]]+}}1,{{.*->lb0}}
; ASM: B.DIM
; ASM: v.fmul {{.*}}, ->[[MUL:vt#[0-9]+]]
; ASM: v.lw.brg [ri{{[0-9]+}}, lc0<<2, zero<<2], ->[[CARRY:vt#[0-9]+]]
; ASM: v.fadd [[CARRY]], [[MUL]], ->[[NEXT:vt#[0-9]+]]
; ASM: v.sw.brg [[NEXT]], [ri{{[0-9]+}}, lc0<<2, {{.*}}]
; ASM: v.sw.brg [[NEXT]], [ri{{[0-9]+}}, lc0<<2, zero<<2]
; ASM-NOT: v.lw.local
; ASM-NOT: v.sw.local
