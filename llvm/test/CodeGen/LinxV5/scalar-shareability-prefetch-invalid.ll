; RUN: not opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -disable-output %s 2>&1 | FileCheck %s
; CHECK: prefetch needs an explicit scoped lowering

declare void @llvm.prefetch.p0(ptr, i32 immarg, i32 immarg, i32 immarg)
define void @shared_prefetch(ptr %p) "linx-scalar-shareability"="intra" {
  call void @llvm.prefetch.p0(ptr %p, i32 0, i32 3, i32 1)
  ret void
}
