; RUN: llc -mtriple=linx64v5 -mcpu=janus -O0 < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -O2 < %s | FileCheck %s

declare void @llvm.linx.sync.core() convergent noduplicate

define void @one_sync(ptr %p) {
  store i64 1, ptr %p
  call void @llvm.linx.sync.core()
  store i64 2, ptr %p
  ret void
}

; CHECK-LABEL: one_sync:
; CHECK: sd
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: SYNC.CORE
; CHECK-NEXT: BSTOP
; CHECK: sd

define void @two_syncs() {
  call void @llvm.linx.sync.core()
  call void @llvm.linx.sync.core()
  ret void
}

; CHECK-LABEL: two_syncs:
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: SYNC.CORE
; CHECK-NEXT: BSTOP
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: SYNC.CORE
; CHECK-NEXT: BSTOP

; SETC must not be hoisted across SYNC.CORE.  EmitHeader isolates the sync in
; a SYS block, and condition state is local to the following COND block.
define i64 @conditional_after_sync(i64 %value) {
entry:
  call void @llvm.linx.sync.core()
  %condition = icmp ne i64 %value, 1
  br i1 %condition, label %true, label %false

true:
  ret i64 2

false:
  ret i64 1
}

; CHECK-LABEL: conditional_after_sync:
; CHECK: BSTART.SYS FALL
; CHECK-NEXT: SYNC.CORE
; CHECK-NEXT: BSTOP
; CHECK: BSTART.STD{{[[:space:]]+}}COND
; CHECK-NEXT: setc.eqi{{[[:space:]]+}}{{[^,]+}}, 1
