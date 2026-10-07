; RUN: opt -mtriple=linx64v5 -passes='default<O0>' -S %s | FileCheck %s

target triple = "linx64v5"

declare i32 @ordinary_call(i32)

define i32 @unmarked_o0(i32 %value) noinline optnone {
entry:
  %local = alloca i32, align 4
  store i32 %value, ptr %local, align 4
  %loaded = load i32, ptr %local, align 4
  %result = call i32 @ordinary_call(i32 %loaded)
  ret i32 %result
}

; CHECK-LABEL: define i32 @unmarked_o0
; CHECK: %local = alloca i32
; CHECK: store i32 %value, ptr %local
; CHECK: %loaded = load i32, ptr %local
; CHECK: %result = call i32 @ordinary_call(i32 %loaded)

