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
