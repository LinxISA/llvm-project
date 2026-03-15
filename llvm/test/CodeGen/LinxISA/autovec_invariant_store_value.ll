; RUN: llc -mtriple=linx64 -O2 \
; RUN:   --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq \
; RUN:   --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 \
; RUN:   < %s | FileCheck %s

define void @fill_i32(ptr nocapture writeonly %buf, i32 %value) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ptr = getelementptr inbounds i32, ptr %buf, i64 %i
  store i32 %value, ptr %ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @fill_i8(ptr nocapture writeonly %buf, i8 %value) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ptr = getelementptr inbounds i8, ptr %buf, i64 %i
  store i8 %value, ptr %ptr, align 1
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @fill_i16(ptr nocapture writeonly %buf, i16 %value) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %ptr = getelementptr inbounds i16, ptr %buf, i64 %i
  store i16 %value, ptr %ptr, align 2
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

; CHECK-LABEL: fill_i32:
; CHECK: BSTART.MSEQ
; CHECK: B.TEXT
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.add zero, ri{{[0-9]+}}{{(\.sw)?}}, ->vt#{{[0-9]+}}
; CHECK: v.sw.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0<<2, vt#{{[0-9]+}}<<2]

; CHECK-LABEL: fill_i8:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.add zero, ri{{[0-9]+}}{{(\.sw)?}}, ->vt#{{[0-9]+}}
; CHECK: v.sb.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0, vt#{{[0-9]+}}]

; CHECK-LABEL: fill_i16:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.add zero, ri{{[0-9]+}}{{(\.sw)?}}, ->vt#{{[0-9]+}}
; CHECK: v.sh.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0<<1, vt#{{[0-9]+}}<<1]
