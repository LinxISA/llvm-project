; RUN: llc -mtriple=linx64 -O2 \
; RUN:   --linx-simt-autovec=1 --linx-simt-autovec-mode=mseq \
; RUN:   --linx-simt-autovec-layout=grouped --linx-simt-autovec-lanes=32 \
; RUN:   < %s | FileCheck %s

define void @copy_u8(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i8, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i8, ptr %dst, i64 %i
  %val = load i8, ptr %src.ptr, align 1
  store i8 %val, ptr %dst.ptr, align 1
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @copy_u16(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i16, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i16, ptr %dst, i64 %i
  %val = load i16, ptr %src.ptr, align 2
  store i16 %val, ptr %dst.ptr, align 2
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @widen_i8_to_i32(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i8, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %i
  %val = load i8, ptr %src.ptr, align 1
  %wide = sext i8 %val to i32
  store i32 %wide, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @widen_i16_to_i32(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i16, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %i
  %val = load i16, ptr %src.ptr, align 2
  %wide = sext i16 %val to i32
  store i32 %wide, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @sign_classify_i8(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i8, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %i
  %val = load i8, ptr %src.ptr, align 1
  %neg = icmp slt i8 %val, 0
  %out = select i1 %neg, i32 -1, i32 1
  store i32 %out, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @sign_classify_i16(ptr nocapture readonly %src, ptr nocapture writeonly %dst) local_unnamed_addr {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds i16, ptr %src, i64 %i
  %dst.ptr = getelementptr inbounds i32, ptr %dst, i64 %i
  %val = load i16, ptr %src.ptr, align 2
  %neg = icmp slt i16 %val, 0
  %out = select i1 %neg, i32 -1, i32 1
  store i32 %out, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

; CHECK-LABEL: copy_u8:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lbu.brg [ri{{[0-9]+}}, lc0, vt#{{[0-9]+}}], ->vt#{{[0-9]+}}
; CHECK: v.sb.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0, vt#{{[0-9]+}}]

; CHECK-LABEL: copy_u16:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lhu.brg [ri{{[0-9]+}}, lc0<<1, vt#{{[0-9]+}}<<1], ->vt#{{[0-9]+}}
; CHECK: v.sh.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0<<1, vt#{{[0-9]+}}<<1]

; CHECK-LABEL: widen_i8_to_i32:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lb.brg [ri{{[0-9]+}}, lc0, vt#{{[0-9]+}}], ->vt#{{[0-9]+}}
; CHECK: v.sw.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0<<2, vt#{{[0-9]+}}<<2]

; CHECK-LABEL: widen_i16_to_i32:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lh.brg [ri{{[0-9]+}}, lc0<<1, vt#{{[0-9]+}}<<1], ->vt#{{[0-9]+}}
; CHECK: v.sw.brg vt#{{[0-9]+}}, [ri{{[0-9]+}}, lc0<<2, vt#{{[0-9]+}}<<2]

; CHECK-LABEL: sign_classify_i8:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lb.brg [ri{{[0-9]+}}, lc0, vt#{{[0-9]+}}], ->vt#{{[0-9]+}}
; CHECK: v.cmp.lt
; CHECK: v.csel
; CHECK: v.sw.brg

; CHECK-LABEL: sign_classify_i16:
; CHECK: BSTART.MSEQ
; CHECK: C.B.DIMI{{[[:space:]]+}}32,{{.*->lb0}}
; CHECK: C.B.DIMI{{[[:space:]]+}}2,{{.*->lb1}}
; CHECK: v.lh.brg [ri{{[0-9]+}}, lc0<<1, vt#{{[0-9]+}}<<1], ->vt#{{[0-9]+}}
; CHECK: v.cmp.lt
; CHECK: v.csel
; CHECK: v.sw.brg
