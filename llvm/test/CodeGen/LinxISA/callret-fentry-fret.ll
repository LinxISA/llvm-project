; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare i64 @callee(i64)

define i64 @callret_normal(i64 %x) {
entry:
  %r = call i64 @callee(i64 %x)
  ret i64 %r
}

; CHECK-LABEL: callret_normal:
; CHECK: FENTRY
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, callee{{(@plt)?}}, ra=[[RET:\.LBB[0-9_]+]]
; CHECK: [[RET]]:
; CHECK: FRET.STK
; CHECK-NOT: FEXIT
