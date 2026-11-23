; RUN: opt -mtriple=linx64v5 -passes=linx-v5-inline-asm-constant-prepare -verify-each -S %s | FileCheck %s
;
; Early preparation cannot cross memory, scope, domain or noinline boundaries.
target triple = "linx64v5"
%Shape = type { i64 }

define internal void @volatile_factory(ptr sret(%Shape) %out) {
  store volatile i64 32, ptr %out
  ret void
}
define internal void @atomic_factory(ptr sret(%Shape) %out) {
  store atomic i64 32, ptr %out monotonic, align 8
  ret void
}
define internal void @scoped_factory(ptr sret(%Shape) %out) "linx-scalar-shareability"="inter" {
  store i64 32, ptr %out
  ret void
}
define internal void @domain_factory(ptr sret(%Shape) %out) "linx-tile-pe-domain"="8" {
  store i64 32, ptr %out
  ret void
}
define internal void @asm_factory(ptr sret(%Shape) %out) {
  %v = call i64 asm sideeffect "", "=Sr"()
  store i64 %v, ptr %out
  ret void
}
define internal void @recursive_factory(ptr sret(%Shape) %out) {
  call void @recursive_factory(ptr sret(%Shape) %out)
  ret void
}
define internal void @scalar_factory(ptr sret(%Shape) %out) {
  store i64 32, ptr %out
  ret void
}
; CHECK-LABEL: define void @boundaries(
; CHECK: call void @volatile_factory
; CHECK: call void @atomic_factory
; CHECK: call void @scoped_factory
; CHECK: call void @domain_factory
; CHECK: call void @asm_factory
; CHECK: call void @recursive_factory
; CHECK: call void @scalar_factory{{.*}}#[[NOINLINE:[0-9]+]]
; CHECK: call void asm sideeffect "", "i,i,i,i,i,i,i"
define void @boundaries() {
  %s0 = alloca %Shape
  %s1 = alloca %Shape
  %s2 = alloca %Shape
  %s3 = alloca %Shape
  %s4 = alloca %Shape
  %s5 = alloca %Shape
  %s6 = alloca %Shape
  call void @volatile_factory(ptr sret(%Shape) %s0)
  call void @atomic_factory(ptr sret(%Shape) %s1)
  call void @scoped_factory(ptr sret(%Shape) %s2)
  call void @domain_factory(ptr sret(%Shape) %s3)
  call void @asm_factory(ptr sret(%Shape) %s4)
  call void @recursive_factory(ptr sret(%Shape) %s5)
  call void @scalar_factory(ptr sret(%Shape) %s6) noinline
  %v0 = load i64, ptr %s0
  %v1 = load i64, ptr %s1
  %v2 = load i64, ptr %s2
  %v3 = load i64, ptr %s3
  %v4 = load i64, ptr %s4
  %v5 = load i64, ptr %s5
  %v6 = load i64, ptr %s6
  call void asm sideeffect "", "i,i,i,i,i,i,i"(i64 %v0, i64 %v1, i64 %v2, i64 %v3, i64 %v4, i64 %v5, i64 %v6)
  ret void
}

; Explicit callsite contracts must also survive early preparation.
; CHECK-LABEL: define void @callsite_scope(
; CHECK: call void @scalar_factory{{.*}}#[[SCOPE:[0-9]+]]
; CHECK: call void asm sideeffect "", "i"
define void @callsite_scope() {
  %s = alloca %Shape
  call void @scalar_factory(ptr sret(%Shape) %s) "linx-scalar-shareability"="inter"
  %v = load i64, ptr %s
  call void asm sideeffect "", "i"(i64 %v)
  ret void
}

; A local buffer can be an ordinary argument alongside a GM sret output.
; Finding that call through the local buffer must not pull its GM store in.
define internal void @gm_output_factory(ptr sret(%Shape) %out, ptr %local) {
  store i64 64, ptr %out
  ret void
}
; CHECK-LABEL: define void @gm_output(
; CHECK: call void @gm_output_factory
; CHECK: call void asm sideeffect "", "i"
define void @gm_output(ptr %gm) {
  %s = alloca i64
  store i64 32, ptr %s
  call void @gm_output_factory(ptr sret(%Shape) %gm, ptr %s)
  %v = load i64, ptr %s
  call void asm sideeffect "", "i"(i64 %v)
  ret void
}
; CHECK-DAG: attributes #[[NOINLINE]] = { noinline }
; CHECK-DAG: attributes #[[SCOPE]] = { "linx-scalar-shareability"="inter" }
