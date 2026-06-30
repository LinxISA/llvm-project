; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

%struct.iter = type { ptr, ptr }

declare void @callee(ptr byval(%struct.iter) align 8)
declare void @use(ptr)

define void @byval_copy_does_not_alias_caller_local(ptr %p, ptr %q) #0 {
entry:
  %local = alloca %struct.iter, align 8
  store ptr %p, ptr %local, align 8
  %field1 = getelementptr inbounds %struct.iter, ptr %local, i64 0, i32 1
  store ptr %q, ptr %field1, align 8
  call void @callee(ptr byval(%struct.iter) align 8 %local)
  call void @use(ptr %local)
  ret void
}

attributes #0 = { noinline "disable-tail-calls"="true" }

; CHECK-LABEL: byval_copy_does_not_alias_caller_local:
; CHECK: BSTART{{[[:space:]]+}}CALL, callee{{(@plt)?}}
; CHECK-DAG: sdi{{[[:space:]]+}}a0, [sp, 24]
; CHECK-DAG: sdi{{[[:space:]]+}}a0, [sp, 8]
; CHECK-DAG: sdi{{[[:space:]]+}}a1, [sp, 32]
; CHECK-DAG: sdi{{[[:space:]]+}}a1, [sp, 16]
; CHECK: addi{{[[:space:]]+}}sp, 8,{{[[:space:]]+}}->a0
; CHECK: BSTART{{[[:space:]]+}}CALL, use{{(@plt)?}}
; CHECK: addi{{[[:space:]]+}}sp, 24,{{[[:space:]]+}}->a0
