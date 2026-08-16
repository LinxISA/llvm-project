; RUN: llc -mtriple=linx64 -O0 -filetype=obj %s -o %t
; RUN: llvm-objdump -d --triple=linx64 %t | FileCheck %s

; Keep the canonical BSTART.ICALL return target in uimm5 range even when the
; callee computation block is longer than 62 bytes. The return landing then
; transfers to the semantic continuation.
define i64 @long_icall(ptr %callee, ptr %p) {
entry:
  %v0 = load volatile i64, ptr %p
  %p1 = getelementptr i64, ptr %p, i64 1
  %v1 = load volatile i64, ptr %p1
  %p2 = getelementptr i64, ptr %p, i64 2
  %v2 = load volatile i64, ptr %p2
  %p3 = getelementptr i64, ptr %p, i64 3
  %v3 = load volatile i64, ptr %p3
  %p4 = getelementptr i64, ptr %p, i64 4
  %v4 = load volatile i64, ptr %p4
  %p5 = getelementptr i64, ptr %p, i64 5
  %v5 = load volatile i64, ptr %p5
  %p6 = getelementptr i64, ptr %p, i64 6
  %v6 = load volatile i64, ptr %p6
  %p7 = getelementptr i64, ptr %p, i64 7
  %v7 = load volatile i64, ptr %p7
  %sum01 = add i64 %v0, %v1
  %sum23 = add i64 %v2, %v3
  %sum45 = add i64 %v4, %v5
  %sum67 = add i64 %v6, %v7
  %sum03 = add i64 %sum01, %sum23
  %sum47 = add i64 %sum45, %sum67
  %arg = add i64 %sum03, %sum47
  %result = call i64 %callee(i64 %arg)
  ret i64 %result
}

; A noreturn indirect call still needs a valid architectural return address.
; If the callee unexpectedly returns, it must enter a nearby fail-closed sink.
define void @noreturn_icall(ptr %callee) noreturn {
entry:
  call void %callee() noreturn
  unreachable
}

; CHECK-LABEL: <long_icall>:
; CHECK: 01 60 96 50{{[[:space:]]+}}BSTART.ICALL{{[[:space:]]+}}0x{{[0-9a-f]+}},{{[[:space:]]+}}->ra
; CHECK-NOT: C.BSTART.STD{{[[:space:]]+}}ICALL
; CHECK: <.LBB{{[0-9_]+}}>:
; CHECK: C.BSTART DIRECT
; CHECK-LABEL: <noreturn_icall>:
; CHECK: 01 60 96 50{{[[:space:]]+}}BSTART.ICALL{{[[:space:]]+}}0x{{[0-9a-f]+}},{{[[:space:]]+}}->ra
; CHECK-NOT: C.BSTART.STD{{[[:space:]]+}}ICALL
; CHECK: <.LBB{{[0-9_]+}}>:
; CHECK: 02 00{{[[:space:]]+}}C.BSTART DIRECT, [[SINK:0x[0-9a-f]+]]
