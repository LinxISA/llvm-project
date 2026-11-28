; RUN: opt -mtriple=linx64v5 -passes='default<O2>' -verify-each -S %s | FileCheck %s
;
; Inlining an aggregate-returning C++ range factory leaves integer ABI
; plumbing around its reference. SROA alone cannot see through this chain.
; Normal optimization promotes the handle and eliminates overwritten stores.
target triple = "linx64v5"
%Shared = type { i64, i32 }

define void @carrier(ptr %external) {
; CHECK-LABEL: define void @carrier(
; CHECK-NOT: alloca
; CHECK: [[HANDLE:%.*]] = {{(tail )?}}call i64 asm sideeffect "", "=Sr"()
; CHECK: call void asm sideeffect "", "Sr"(i64 [[HANDLE]])
; CHECK-NOT: store i32 1
; CHECK: store i32 2, ptr %external
; CHECK: ret void
  %local = alloca %Shared, align 8
  %address = ptrtoint ptr %local to i64
  %a = insertvalue [2 x i64] poison, i64 %address, 0
  %b = insertvalue [2 x i64] %a, i64 0, 1
  %c = extractvalue [2 x i64] %b, 0
  %p = inttoptr i64 %c to ptr
  %handle = call i64 asm sideeffect "", "=Sr"()
  store i64 %handle, ptr %p, align 8
  %read = load i64, ptr %local, align 8
  call void asm sideeffect "", "Sr"(i64 %read)
  store i32 1, ptr %external, align 4
  store i32 2, ptr %external, align 4
  ret void
}

