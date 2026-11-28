; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s

target triple = "linx64v5"

; Normal inlining promotes reference wrappers and permits ordinary DSE.
define internal void @consume(ptr %handle, ptr %gm) {
  %s = load i64, ptr %handle
  call void asm sideeffect "", "@2Sr"(i64 %s)
  store i32 1, ptr %gm
  store i32 2, ptr %gm
  ret void
}

; CHECK-LABEL: define void @caller(
; CHECK-NOT: alloca
; CHECK: %s = {{(tail )?}}call i64 asm sideeffect "", "=@2Sr"()
; CHECK-NOT: store i64
; CHECK-NOT: load i64
; CHECK: call void asm sideeffect "", "@2Sr"(i64 %s)
; CHECK-NEXT: store i32 2, ptr %gm
; CHECK-NEXT: ret void
define void @caller(ptr %gm) {
  %slot = alloca i64
  %s = call i64 asm sideeffect "", "=@2Sr"()
  store i64 %s, ptr %slot
  call void @consume(ptr %slot, ptr %gm)
  ret void
}

