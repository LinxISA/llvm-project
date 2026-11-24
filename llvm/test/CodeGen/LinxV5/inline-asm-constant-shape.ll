; RUN: opt -mtriple=linx64v5 -passes=linx-v5-inline-asm-constant-prepare -verify-each -S %s | FileCheck %s --check-prefix=PREP
; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s --check-prefix=OPT
;
; Prepare a private sret shape only when it feeds an immediate asm input.
; Do this before scoped Scalar loads/stores make its fields opaque to SROA.
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

; PREP-LABEL: define void @constant_shape(
; PREP-NOT: alloca
; PREP: call i64 asm sideeffect "", "=r,i,i"(i64 32, i64 64)
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

; External stores must be scoped before ordinary DSE can remove the first.
; OPT-LABEL: define void @external_memory(
; OPT: call void @llvm.linx.scalar.store(ptr %p, i64 1, i32 4, i32 1)
; OPT: call void @llvm.linx.scalar.store(ptr %p, i64 2, i32 4, i32 1)
; OPT: ret void
define void @external_memory(ptr %p) {
  store i32 1, ptr %p
  store i32 2, ptr %p
  ret void
}

; PREP-LABEL: define void @runtime_immediate(
; PREP: call void asm sideeffect "", "i"(i64 %n)
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
; PREP-LABEL: define void @gm_shape(
; PREP: call void @gm_factory
; PREP: call void asm sideeffect "", "i"(i64 %v)
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
; PREP-LABEL: define void @respect_noinline(
; PREP: call void @noinline_factory
; PREP: call void asm sideeffect "", "i"(i64 %v)
define void @respect_noinline() {
  %s = alloca %Shape
  call void @noinline_factory(ptr sret(%Shape) %s)
  %v = load i64, ptr %s
  call void asm sideeffect "", "i"(i64 %v)
  ret void
}

; Register-only operands must not trigger the preparation.
; PREP-LABEL: define void @register_shape(
; PREP: call void @shape_factory
; PREP: call void asm sideeffect "", "r"(i64 %v)
define void @register_shape() {
  %s = alloca %Shape
  call void @shape_factory(ptr sret(%Shape) %s)
  %v = load i64, ptr %s
  call void asm sideeffect "", "r"(i64 %v)
  ret void
}
