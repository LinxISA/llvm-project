; RUN: llc < %s -O2 | FileCheck %s --dump-input always -vv

; CHECK-LABEL: test1:
; CHECK:      //APP
; CHECK-NEXT: //NO_APP
; CHECK:      setc.ne a0, a1
define void @test1(i64 %a, i64 %b) {
  %cmp = icmp eq i64 %a, %b
  tail call void asm sideeffect "", ""()
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}