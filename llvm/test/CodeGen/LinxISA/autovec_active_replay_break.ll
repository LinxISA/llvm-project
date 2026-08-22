; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @search_store_index(ptr nocapture %a, ptr nocapture %out) {
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

; CHECK-LABEL: search_store_index:
; CHECK: FENTRY
; CHECK: C.BSTART.STD
; CHECK-NOT: BSTART.MSEQ

; REMARK: "function":"search_store_index"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"
