; RUN: rm -f %t.auto.remarks.json %t.grouped.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.auto.remarks.json < %s | FileCheck %s --check-prefix=AUTO
; RUN: FileCheck %s --check-prefix=REMARK-AUTO < %t.auto.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.grouped.remarks.json < %s | FileCheck %s --check-prefix=GROUPED
; RUN: FileCheck %s --check-prefix=REMARK-GROUPED < %t.grouped.remarks.json

define void @search_store_index_nested(ptr nocapture %a, ptr nocapture %b, ptr nocapture %out) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %cont ]
  %slot = getelementptr inbounds i32, ptr %a, i64 %i
  %v = load i32, ptr %slot, align 4
  %found = icmp sgt i32 %v, 0
  br i1 %found, label %break, label %work

work:
  %dst = getelementptr inbounds float, ptr %b, i64 %i
  %small = icmp slt i32 %v, 5
  br i1 %small, label %then0, label %else0

then0:
  store float 1.000000e+00, ptr %dst, align 4
  br label %cont

else0:
  store float 2.000000e+00, ptr %dst, align 4
  br label %cont

break:
  %iret = trunc i64 %i to i32
  br label %exit

cont:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  %res = phi i32 [ %iret, %break ], [ -1, %cont ]
  store i32 %res, ptr %out, align 4
  ret void
}

define void @search_store_index_split_addrs(ptr nocapture %a, ptr nocapture %b,
                                            ptr nocapture %c, ptr nocapture %out) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %cont ]
  %slot = getelementptr inbounds i32, ptr %a, i64 %i
  %v = load i32, ptr %slot, align 4
  %found = icmp sgt i32 %v, 0
  br i1 %found, label %break, label %work

work:
  %dst0 = getelementptr inbounds float, ptr %b, i64 %i
  %dst1 = getelementptr inbounds float, ptr %c, i64 %i
  %small = icmp slt i32 %v, 5
  br i1 %small, label %then0, label %else0

then0:
  store float 1.000000e+00, ptr %dst0, align 4
  br label %cont

else0:
  store float 2.000000e+00, ptr %dst1, align 4
  br label %cont

break:
  %iret = trunc i64 %i to i32
  br label %exit

cont:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  %res = phi i32 [ %iret, %break ], [ -1, %cont ]
  store i32 %res, ptr %out, align 4
  ret void
}

define void @store_split_addrs_raw(ptr nocapture readonly %a,
                                   ptr nocapture writeonly %b,
                                   ptr nocapture writeonly %c) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %cont ]
  %slot = getelementptr inbounds i32, ptr %a, i64 %i
  %v = load i32, ptr %slot, align 4
  %dst0 = getelementptr inbounds float, ptr %b, i64 %i
  %dst1 = getelementptr inbounds float, ptr %c, i64 %i
  %small = icmp slt i32 %v, 5
  br i1 %small, label %then0, label %else0

then0:
  store float 1.000000e+00, ptr %dst0, align 4
  br label %cont

else0:
  store float 2.000000e+00, ptr %dst1, align 4
  br label %cont

cont:
  %inc = add nuw i64 %i, 1
  %done = icmp ult i64 %inc, 64
  br i1 %done, label %loop, label %exit

exit:
  ret void
}

; AUTO-LABEL: search_store_index_nested:
; AUTO-NOT: BSTART.MSEQ

; REMARK-AUTO: "function":"search_store_index_nested"
; REMARK-AUTO: "status":"reject"
; REMARK-AUTO: "reason":"pto0583_body_branch_reserved"
; REMARK-AUTO: "layout_policy":"auto"
; REMARK-AUTO: "layout_kind":"grouped-strip-mined"
; REMARK-AUTO: "cf_strategy":"active-replay"

; GROUPED-LABEL: search_store_index_split_addrs:
; GROUPED-NOT: BSTART.MSEQ

; REMARK-GROUPED: "function":"search_store_index_split_addrs"
; REMARK-GROUPED: "status":"reject"
; REMARK-GROUPED: "reason":"pto0583_body_branch_reserved"
; REMARK-GROUPED: "layout_policy":"grouped"
; REMARK-GROUPED: "layout_kind":"grouped-strip-mined"
; REMARK-GROUPED: "cf_strategy":"active-replay"
; REMARK-GROUPED: "function":"store_split_addrs_raw"
; REMARK-GROUPED: "status":"reject"
; REMARK-GROUPED: "reason":"grouped_layout_requires_exec_mask_save_restore"
; REMARK-GROUPED: "layout_policy":"grouped"
; REMARK-GROUPED: "cf_strategy":"exec-mask-save-restore-required"
