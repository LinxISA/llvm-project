; RUN: rm -f %t.reject.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.reject.remarks.json < %s > /dev/null
; RUN: FileCheck %s --check-prefix=REMARK-REJECT < %t.reject.remarks.json

define void @search_store_index_exit_store_reject(ptr nocapture %a, ptr nocapture %out) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %cont ]
  %slot = getelementptr inbounds i32, ptr %a, i64 %i
  %v = load i32, ptr %slot, align 4
  %found = icmp sgt i32 %v, 0
  br i1 %found, label %break, label %cont

break:
  %iret = trunc i64 %i to i32
  store i32 %iret, ptr %out, align 4
  br label %exit

cont:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; REMARK-REJECT: "function":"search_store_index_exit_store_reject"
; REMARK-REJECT: "status":"reject"
; REMARK-REJECT: "reason":"pto0583_body_branch_reserved"
; REMARK-REJECT: "layout_policy":"grouped"
