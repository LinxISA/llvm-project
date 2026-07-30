; RUN: rm -f %t.auto.remarks.json %t.grouped.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.auto.remarks.json < %s | FileCheck %s --check-prefix=AUTO
; RUN: FileCheck %s --check-prefix=REMARK-AUTO < %t.auto.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 --linx-simt-autovec-remarks=%t.grouped.remarks.json < %s | FileCheck %s --check-prefix=GROUPED
; RUN: FileCheck %s --check-prefix=REMARK-GROUPED < %t.grouped.remarks.json

define void @search_store_index_grouped_boundary(ptr nocapture %a, ptr nocapture %out) {
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

; AUTO-LABEL: search_store_index_grouped_boundary:
; AUTO: BSTART.MSEQ
; AUTO: B.TEXT
; AUTO: B.IOT{{.*}}->t<128B>
; AUTO: B.IOT{{.*}}->u<256B>
; AUTO: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; AUTO: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; AUTO: v.add{{[[:space:]]+}}lc0, lc1.uw<<5, ->vt#1
; AUTO: v.sw.brg.local{{.*}}ri1, [ts, lc0<<2, lc1<<7]
; AUTO: v.lw.brg.local{{.*}}[ts, lc0<<2, lc1<<7]
; AUTO: v.rdor
; AUTO: b.ne
; AUTO: v.sw.brg{{[[:space:]]+}}vt#1,
; AUTO: v.sw.brg.local{{.*}}zero, [ts, lc0<<2, lc1<<7]
; AUTO: v.sw.brg.local{{.*}}zero, [ts, lc0<<2, lc1<<7]

; REMARK-AUTO: "function":"search_store_index_grouped_boundary"
; REMARK-AUTO: "status":"lowered"
; REMARK-AUTO: "layout_policy":"auto"
; REMARK-AUTO: "layout_kind":"grouped-strip-mined"
; REMARK-AUTO: "cf_strategy":"active-replay"

; GROUPED-LABEL: search_store_index_grouped_boundary:
; GROUPED: BSTART.MSEQ
; GROUPED: B.TEXT
; GROUPED: B.IOT{{.*}}->t<128B>
; GROUPED: B.IOT{{.*}}->u<256B>
; GROUPED: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; GROUPED: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; GROUPED: v.sw.brg.local{{.*}}ri1, [ts, lc0<<2, lc1<<7]
; GROUPED: v.lw.brg.local{{.*}}[ts, lc0<<2, lc1<<7]
; GROUPED: v.sw.brg{{[[:space:]]+}}vt#1,
; GROUPED: v.sw.brg.local{{.*}}zero, [ts, lc0<<2, lc1<<7]
; GROUPED: v.sw.brg.local{{.*}}zero, [ts, lc0<<2, lc1<<7]

; REMARK-GROUPED: "function":"search_store_index_grouped_boundary"
; REMARK-GROUPED: "status":"lowered"
; REMARK-GROUPED: "layout_policy":"grouped"
; REMARK-GROUPED: "layout_kind":"grouped-strip-mined"
; REMARK-GROUPED: "cf_strategy":"active-replay"
