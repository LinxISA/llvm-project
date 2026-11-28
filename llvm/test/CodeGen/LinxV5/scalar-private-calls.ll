; RUN: opt -mtriple=linx64v5 -passes='function(linx-v5-scalar-shareability),default<O2>' -S %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj %s -o %t.o
; External/indirect calls need no sharing annotations under the Private ABI.
declare void @external(ptr)

; CHECK-LABEL: define i32 @derived(
; CHECK-NOT: @llvm.linx.scalar
; CHECK: store i32 42
; CHECK-NOT: load i32
; CHECK: ret i32 42
define i32 @derived(ptr %p, i64 %index) {
  %q = getelementptr i32, ptr %p, i64 %index
  store i32 42, ptr %q
  %r = load i32, ptr %q
  ret i32 %r
}
; CHECK-LABEL: define void @calls(
; CHECK-NOT: @llvm.linx.scalar
; CHECK: call void @external
; CHECK: call void %callback
; CHECK: ret void
define void @calls(ptr %p, ptr %callback) {
  call void @external(ptr %p)
  call void %callback(ptr %p)
  ret void
}
; CHECK-LABEL: define i32 @explicit_private(
; CHECK-NOT: @llvm.linx.scalar
; CHECK: store i32 42
; CHECK-NOT: load i32
; CHECK: ret i32 42
define i32 @explicit_private(ptr %p) "linx-scalar-shareability"="private" {
  store i32 42, ptr %p
  %r = load i32, ptr %p
  ret i32 %r
}
