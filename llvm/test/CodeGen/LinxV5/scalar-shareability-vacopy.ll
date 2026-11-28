; RUN: opt -mtriple=linx64v5 -passes=linx-v5-scalar-shareability -S %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o
declare void @llvm.va_copy(ptr, ptr)
define void @copy(ptr %destination, ptr %source) "linx-scalar-shareability"="intra" {
  call void @llvm.va_copy(ptr %destination, ptr %source)
  ret void
}
; CHECK-LABEL: define void @copy
; Unaligned va_list copies retain byte-safe scoped loads/stores.
; CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}, i32 1, i32 1)
; CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}, i64 {{.*}}, i32 1, i32 1)
; CHECK-NOT: call void @llvm.va_copy
