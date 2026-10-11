; RUN: not llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=asm %s -o /dev/null 2>&1 | FileCheck %s

target triple = "linx64v5"

; `%K` is only legal for a proven killed pure input. This first use remains
; live for the following asm and must fail rather than silently emitting a
; premature bare B.IOS. The caller must use ordinary `%S` here.
; CHECK: error: invalid operand in inline asm
define void @shared_last_use_requires_kill() {
  %shared = call i64 asm sideeffect "B.IOS mask=1111, ->${0:S}<128B>", "=@2Sr"()
  call void asm sideeffect "B.IOS ${0:K}, mask=1111", "@2Sr"(i64 %shared)
  call void asm sideeffect "B.IOS ${0:S}, mask=1111", "@2Sr"(i64 %shared)
  ret void
}
