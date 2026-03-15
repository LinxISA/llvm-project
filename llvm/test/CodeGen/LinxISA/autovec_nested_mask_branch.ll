; RUN: rm -f %t.remarks.json
; RUN: clang -target linx64-linx-none-elf -O2 -mllvm -linx-simt-autovec=1 -mllvm -linx-simt-autovec-mode=mseq -mllvm -linx-simt-autovec-remarks=%t.remarks.json -S -x ir %s -o - | FileCheck %s
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @vector_nested_if(ptr nocapture %a, ptr nocapture %b) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %join ]
  %bp = getelementptr inbounds float, ptr %b, i64 %i
  %bv = load float, ptr %bp, align 4
  %ap = getelementptr inbounds float, ptr %a, i64 %i
  %cond0 = fcmp ogt float %bv, 0.000000e+00
  br i1 %cond0, label %then0, label %else0

then0:
  %cond1 = fcmp ogt float %bv, 1.000000e+01
  br i1 %cond1, label %then1, label %else1

then1:
  store float 1.000000e+00, ptr %ap, align 4
  br label %join

else1:
  store float 2.000000e+00, ptr %ap, align 4
  br label %join

else0:
  store float 3.000000e+00, ptr %ap, align 4
  br label %join

join:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; CHECK-LABEL: vector_nested_if:
; CHECK: BSTART.MSEQ
; CHECK: B.TEXT
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.flt
; CHECK: v.csel
; CHECK: v.flt zero,
; CHECK: v.csel
; CHECK: v.sw.brg
; CHECK-NOT: b.nz
; CHECK-NOT: v.rdor

; REMARK: "function":"vector_nested_if"
; REMARK: "single_block":true
; REMARK: "layout_kind":"grouped-strip-mined"
; REMARK: "cf_strategy":"if-converted-single-block"
