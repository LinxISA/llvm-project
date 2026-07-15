; RUN: rm -f %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=auto --linx-simt-autovec-remarks=%t.remarks.json < %s > /dev/null
; RUN: FileCheck %s --check-prefix=AUTO < %t.remarks.json
; RUN: llc -mtriple=linx64 -O2 --linx-simt-autovec=1 --linx-simt-autovec-mode=auto < %s | FileCheck %s --check-prefix=ASM

declare float @sinf(float)
declare float @cosf(float)
declare float @fabsf(float)

define void @sinf_loop(ptr nocapture %dst, ptr nocapture readonly %src) #0 {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds float, ptr %src, i64 %i
  %value = load float, ptr %src.ptr, align 4
  %result = call float @sinf(float %value)
  %dst.ptr = getelementptr inbounds float, ptr %dst, i64 %i
  store float %result, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @cosf_loop(ptr nocapture %dst, ptr nocapture readonly %src) #0 {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds float, ptr %src, i64 %i
  %value = load float, ptr %src.ptr, align 4
  %result = call float @cosf(float %value)
  %dst.ptr = getelementptr inbounds float, ptr %dst, i64 %i
  store float %result, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

define void @fabsf_loop(ptr nocapture %dst, ptr nocapture readonly %src) #0 {
entry:
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %inc, %loop ]
  %src.ptr = getelementptr inbounds float, ptr %src, i64 %i
  %value = load float, ptr %src.ptr, align 4
  %result = call float @fabsf(float %value)
  %dst.ptr = getelementptr inbounds float, ptr %dst, i64 %i
  store float %result, ptr %dst.ptr, align 4
  %inc = add nuw nsw i64 %i, 1
  %done = icmp eq i64 %inc, 64
  br i1 %done, label %exit, label %loop

exit:
  ret void
}

attributes #0 = { noinline nounwind }

; AUTO: "function":"sinf_loop"
; AUTO: "status":"reject"
; AUTO: "reason":"contains_call"
; AUTO: "function":"cosf_loop"
; AUTO: "status":"reject"
; AUTO: "reason":"contains_call"
; AUTO: "function":"fabsf_loop"
; AUTO: "status":"lowered"

; ASM-LABEL: fabsf_loop:
; ASM: BSTART.MSEQ
; ASM: v.fabs
