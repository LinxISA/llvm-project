; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-remarks=%t.remarks.json < %s > /dev/null
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq < %s | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 < %s | FileCheck %s --check-prefix=GROUP

define void @saxpy_like(ptr nocapture %a, ptr nocapture %b, ptr nocapture %c, i64 %n) {
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
  %cmp = icmp ult i64 %inc, 1024
  br i1 %cmp, label %loop, label %exit

exit:
  ret void
}

define void @nested_store(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %outer

outer:                                            ; preds = %entry, %outer_latch
  %nl = phi i32 [ 0, %entry ], [ %nl.next, %outer_latch ]
  br label %inner

inner:                                            ; preds = %outer, %inner
  %i = phi i64 [ 0, %outer ], [ %i.next, %inner ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %sum = fadd float %bv, 1.000000e+00
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  store float %sum, ptr %ap, align 4
  %i.next = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %i.next, 320
  br i1 %done, label %outer_latch, label %inner

outer_latch:                                      ; preds = %inner
  %nl.next = add nuw nsw i32 %nl, 1
  %outer.done = icmp eq i32 %nl.next, 64
  br i1 %outer.done, label %exit, label %outer

exit:
  ret void
}

define void @inner_if_uniform(ptr nocapture %a, ptr nocapture %b, i64 %mode) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %join ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %cond = icmp slt i64 %mode, 0
  br i1 %cond, label %then, label %else

then:
  %t = fadd float %bv, 1.000000e+00
  store float %t, ptr %ap, align 4
  br label %join

else:
  %e = fsub float %bv, 1.000000e+00
  store float %e, ptr %ap, align 4
  br label %join

join:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 512
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; REMARK: "function":"saxpy_like"
; REMARK: "status":"lowered"
; REMARK: "reason":"lowered_vblock_mseq_affine"
; REMARK: "configured_mode":"mseq"
; REMARK: "selected_mode":"mseq"
; REMARK: "header_kind":"mseq"
; REMARK: "touches_memory":true
; REMARK: "tripcount_source":"scev_constant"
; REMARK: "address_model":"affine"
; REMARK-NOT: "fallback_marker"
; REMARK: "function":"nested_store"
; REMARK: "status":"lowered"
; REMARK: "function":"inner_if_uniform"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"

; ASM: BSTART.MSEQ
; ASM: B.IOR
; ASM: v.lw.brg
; ASM: v.fadd
; ASM: v.sw.brg
; ASM: setc.lt
; ASM-NOT: b.lt

; GROUP: BSTART.MSEQ
; GROUP: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; GROUP: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb1}}
; GROUP: v.add lc0, lc1.uw<<5
