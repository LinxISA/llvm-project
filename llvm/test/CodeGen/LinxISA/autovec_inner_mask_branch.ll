; RUN: rm -f %t.remarks.json
; RUN: clang -target linx64-linx-none-elf -O2 -mllvm -linx-simt-autovec=1 -mllvm -linx-simt-autovec-mode=mseq -mllvm -linx-simt-autovec-remarks=%t.remarks.json -S -x ir %s -o - | FileCheck %s
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_inner_if(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %join ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %cond = fcmp ogt float %bv, 0.000000e+00
  br i1 %cond, label %then, label %else

then:
  %t = fadd float %bv, 1.000000e+00
  store float %t, ptr %ap, align 4
  br label %join

else:
  %e = fsub float 0.000000e+00, %bv
  store float %e, ptr %ap, align 4
  br label %join

join:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: vector_inner_if:
; CHECK: BSTART.MSEQ
; CHECK: B.TEXT
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.flt
; CHECK: v.csel
; CHECK: v.sw.brg
; CHECK-NOT: b.nz
; CHECK-NOT: v.rdor

; REMARK: "function":"vector_inner_if"
; REMARK: "single_block":true
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
