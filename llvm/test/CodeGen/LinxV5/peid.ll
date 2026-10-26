; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s

declare i64 @llvm.linx.get.thread.id()

define i64 @get_peid() {
  %id = call i64 @llvm.linx.get.thread.id()
  ret i64 %id
}

; CHECK-LABEL: get_peid:
; THREAD_ID is SSR 0x0026 (38), as defined by the current PTO ISA spec.
; The backend printer calls the 32-bit SYS carrier BSTART.AUX.
; CHECK: BSTART.AUX
; CHECK: ssrget 38,
