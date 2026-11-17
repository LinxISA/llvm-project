; RUN: not llc -mtriple=linx64v5 -mcpu=janus < %s 2>&1 | FileCheck %s
; CHECK: memory-accessing callee needs a matching shareability contract
declare void @opaque(ptr)
define void @f(ptr %p) "linx-scalar-shareability"="intra" {
  call void @opaque(ptr %p)
  ret void
}
