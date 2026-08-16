; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare i64 @baz_leaf(i64) #0

define i64 @call_return(i64 %x) #1 {
entry:
  %r = call i64 @baz_leaf(i64 %x)
  ret i64 %r
}

define i64 @bar(i64 %x) #0 {
entry:
  %a = call i64 @baz_leaf(i64 %x)
  %b = add i64 %a, 3
  ret i64 %b
}

define i64 @foo(i64 %x) #0 {
entry:
  %slot = alloca i64, align 8
  %a = call i64 @bar(i64 %x)
  %b = call i64 @baz_leaf(i64 %a)
  %c = add i64 %a, %b
  store i64 %c, ptr %slot, align 8
  %d = load i64, ptr %slot, align 8
  ret i64 %d
}

; CHECK-LABEL: call_return:
; CHECK-NOT: BSTART
; CHECK: FENTRY
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, baz_leaf{{(@plt)?}}, ra=[[CR_RET:\.LBB[0-9_]+]]
; CHECK-NOT: c.setret
; CHECK: [[CR_RET]]:
; CHECK-NOT: BSTART
; CHECK: FRET.STK
; CHECK-NOT: C.BSTOP

; CHECK-LABEL: bar:
; CHECK-NOT: BSTART
; CHECK: FENTRY
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, baz_leaf{{(@plt)?}}, ra=[[BAR_RET:\.LBB[0-9_]+]]
; CHECK-NOT: c.setret
; CHECK: [[BAR_RET]]:
; CHECK: FRET.STK
; CHECK-NOT: C.BSTOP

; CHECK-LABEL: foo:
; CHECK-NOT: BSTART
; CHECK: FENTRY
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, bar{{(@plt)?}}, ra=[[FOO_RET1:\.LBB[0-9_]+]]
; CHECK-NOT: c.setret
; CHECK: [[FOO_RET1]]:
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, baz_leaf{{(@plt)?}}, ra=[[FOO_RET2:\.LBB[0-9_]+]]
; CHECK-NOT: c.setret
; CHECK: [[FOO_RET2]]:
; CHECK: FRET.STK
; CHECK-NOT: C.BSTOP

attributes #0 = { noinline }
attributes #1 = { noinline "disable-tail-calls"="true" }
