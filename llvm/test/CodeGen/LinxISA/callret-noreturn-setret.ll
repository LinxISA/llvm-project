; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

declare void @sink(ptr) noreturn

define void @panic_like(ptr %p) noreturn {
entry:
  call void @sink(ptr %p) noreturn
  unreachable
}

; CHECK-LABEL: panic_like:
; CHECK: HL.BSTART.STD{{[[:space:]]+}}CALL, sink{{(@plt)?}}, ra=
