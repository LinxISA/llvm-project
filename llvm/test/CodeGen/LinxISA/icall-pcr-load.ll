; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

@fp = global ptr null, align 8
@obj = global i64 0, align 8

define void @foo() noinline {
entry:
  %p = load ptr, ptr @fp, align 8
  %isnull = icmp eq ptr %p, null
  br i1 %isnull, label %ret, label %call

call:
  call void %p(ptr @obj)
  br label %ret

ret:
  ret void
}

; CHECK-LABEL: foo:
; CHECK: {{(hl\.)?}}ld.pcr{{[[:space:]]+}}[fp],{{[[:space:]]+}}->[[CALLEE:[a-z0-9#]+]]
; CHECK: setc.{{eqi|nei}}{{[[:space:]]+}}[[CALLEE]],{{[[:space:]]+}}0
; CHECK: BSTART.ICALL{{[[:space:]]+}}[[RET:\.LBB[0-9_]+]],{{[[:space:]]+}}->ra
; CHECK-NOT: setret
; CHECK: c.setc.tgt{{[[:space:]]+}}[[CALLEE]]
; CHECK: [[RET]]:
