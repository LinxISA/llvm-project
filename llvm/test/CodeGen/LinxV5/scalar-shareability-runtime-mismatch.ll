; RUN: not llc -mtriple=linx64v5 -mcpu=janus %s 2>&1 | FileCheck %s
; CHECK: external memory call 'private_callee' needs
declare void @private_callee(ptr) "linx-scalar-shareability"="private"
define void @runtime(ptr %p) {
  call void @private_callee(ptr %p) "linx-scalar-shareability"="intra"
  ret void
}
