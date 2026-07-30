; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s

declare i64 @llvm.linx.get.thread.id()

define i64 @get_peid() {
  %id = call i64 @llvm.linx.get.thread.id()
  ret i64 %id
}

; CHECK-LABEL: get_peid:
; CHECK: ssrget 2050,
