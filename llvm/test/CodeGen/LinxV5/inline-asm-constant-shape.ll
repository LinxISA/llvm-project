; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s --check-prefix=OPT
;
; Normal inlining and SROA recover constant shapes for asm immediates.
target triple = "linx64v5"
%Shape = type { i64, i64 }

define internal void @shape_factory(ptr sret(%Shape) %out) {
  store i64 32, ptr %out
  %k = getelementptr %Shape, ptr %out, i64 0, i32 1
  store i64 64, ptr %k
  ret void
}
define internal void @nested_factory(ptr sret(%Shape) %out) {
  call void @shape_factory(ptr sret(%Shape) %out)
  ret void
}

; OPT-LABEL: define void @constant_shape(
; OPT-NOT: @llvm.linx.scalar
; OPT: call i64 asm sideeffect "", "=r,i,i"(i64 32, i64 64)
define void @constant_shape(ptr %gm) {
  %s = alloca %Shape
  call void @nested_factory(ptr sret(%Shape) %s)
  %n = load i64, ptr %s
  %kp = getelementptr %Shape, ptr %s, i64 0, i32 1
  %k = load i64, ptr %kp
  %unused = call i64 asm sideeffect "", "=r,i,i"(i64 %n, i64 %k)
  ret void
}

; Ordinary external stores follow normal DSE rules.
; OPT-LABEL: define void @external_memory(
; OPT-NOT: store i32 1
; OPT: store i32 2, ptr %p
; OPT: ret void
define void @external_memory(ptr %p) {
  store i32 1, ptr %p
  store i32 2, ptr %p
  ret void
}

; OPT-LABEL: define void @runtime_immediate(
; OPT: call void asm sideeffect "", "i"(i64 %n)
define void @runtime_immediate(i64 %n) {
  call void asm sideeffect "", "i"(i64 %n)
  ret void
}

; A real GM load is not eligible for early inlining, even through sret.
define internal void @gm_factory(ptr sret(%Shape) %out, ptr %gm) {
  %v = load i64, ptr %gm
  store i64 %v, ptr %out
  ret void
}
; OPT-LABEL: define void @gm_shape(
; OPT: load i64, ptr %gm
; OPT: call void asm sideeffect "", "i"(i64 %{{.*}})
define void @gm_shape(ptr %gm) {
  %s = alloca %Shape
  call void @gm_factory(ptr sret(%Shape) %s, ptr %gm)
  %v = load i64, ptr %s
  call void asm sideeffect "", "i"(i64 %v)
  ret void
}

define internal void @noinline_factory(ptr sret(%Shape) %out) noinline {
  store i64 32, ptr %out
  ret void
}
; OPT-LABEL: define void @respect_noinline(
; OPT: call {{.*}}void @noinline_factory
; OPT: call void asm sideeffect "", "i"
define void @respect_noinline() {
  %s = alloca %Shape
  call void @noinline_factory(ptr sret(%Shape) %s)
  %v = load i64, ptr %s
  call void asm sideeffect "", "i"(i64 %v)
  ret void
}

; Register-only operands must not trigger the preparation.
; OPT-LABEL: define void @register_shape(
; OPT-NOT: alloca
; OPT: call void asm sideeffect "", "r"(i64 32)
define void @register_shape() {
  %s = alloca %Shape
  call void @shape_factory(ptr sret(%Shape) %s)
  %v = load i64, ptr %s
  call void asm sideeffect "", "r"(i64 %v)
  ret void
}
