; RUN: llc -mtriple=linx64 -O2 \
; RUN:   --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq \
; RUN:   --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 \
; RUN:   < %s | FileCheck %s

define void @copy_and_last_value_liveout(ptr nocapture readonly %a,
                                         ptr nocapture writeonly %tmp,
                                         ptr nocapture writeonly %out) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ptr = getelementptr inbounds float, ptr %a, i64 %i
  %val = load float, ptr %ptr, align 4
  %dst = getelementptr inbounds float, ptr %tmp, i64 %i
  store float %val, ptr %dst, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  store float %val, ptr %out, align 4
  ret void
}

; CHECK-LABEL: copy_and_last_value_liveout:
; CHECK: BSTART.MSEQ
; CHECK: B.TEXT
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lw.brg
; CHECK: v.sw.brg
; CHECK: v.sw.brg
