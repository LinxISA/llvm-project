; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus %s -o /dev/null 2>&1 | FileCheck %s

target triple = "linx64v5"

; CHECK: LLVM ERROR: LinxV5 Shared handle requires Shared-only COPY/PHI aliases and B.IOS operands
define void @shared_handle_integer() {
  %handle = call i64 asm sideeffect "", "=@2Sr"()
  %integer = add i64 %handle, 1
  call void asm sideeffect "", "@2Sr"(i64 %integer)
  ret void
}
