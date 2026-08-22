; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.remarks.json < %s | FileCheck %s
; RUN: FileCheck %s --check-prefix=REMARK < %t.remarks.json

define void @search_store_index_split_addrs_near_canonical(ptr nocapture readonly %a,
                                                           ptr nocapture writeonly %b,
                                                           ptr nocapture writeonly %c,
                                                           ptr nocapture writeonly %out) {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %work ]
  %slot = getelementptr inbounds i32, ptr %a, i64 %i
  %v = load i32, ptr %slot, align 4
  %keep_going = icmp slt i32 %v, 11
  br i1 %keep_going, label %work, label %break

work:
  %small = icmp slt i32 %v, 5
  %dst = select i1 %small, ptr %b, ptr %c
  %val = select i1 %small, float 1.000000e+00, float 2.000000e+00
  %dst.slot = getelementptr inbounds float, ptr %dst, i64 %i
  store float %val, ptr %dst.slot, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop, !llvm.loop !0

break:
  %iret = trunc nuw nsw i64 %i to i32
  br label %exit

exit:
  %res = phi i32 [ %iret, %break ], [ -1, %work ]
  store i32 %res, ptr %out, align 4
  ret void
}

; CHECK-LABEL: search_store_index_split_addrs_near_canonical:
; CHECK-NOT: BSTART.MSEQ

; REMARK: "function":"search_store_index_split_addrs_near_canonical"
; REMARK: "status":"reject"
; REMARK: "reason":"pto0583_body_branch_reserved"
; REMARK: "layout_policy":"grouped"
; REMARK: "layout_kind":"none"
; REMARK: "cf_strategy":"none"

!0 = distinct !{!0, !1}
!1 = !{!"llvm.loop.mustprogress"}
