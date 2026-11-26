; RUN: opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -S %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o

declare void @llvm.prefetch.p0(ptr, i32 immarg, i32 immarg, i32 immarg)
define void @runtime_prefetch(ptr %p) "linx-runtime-intra-abi" {
  call void @llvm.prefetch.p0(ptr %p, i32 0, i32 3, i32 1)
  ret void
}
; CHECK-LABEL: define void @runtime_prefetch(
; CHECK-NOT: call void @llvm.prefetch
; CHECK: ret void
