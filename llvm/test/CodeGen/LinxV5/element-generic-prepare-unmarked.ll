; RUN: opt -mtriple=linx64v5 -passes='linx-v5-element-generic-prepare' -S %s | FileCheck %s
; The generic feature must not run extra canonicalization on unrelated code.
define i32 @ordinary(i32 %x) {
entry:
  %slot = alloca i32
  store i32 %x, ptr %slot
  %value = load i32, ptr %slot
  %identity = add i32 %value, 0
  ret i32 %identity
}
; CHECK-LABEL: define i32 @ordinary
; CHECK: %slot = alloca i32
; CHECK: store i32 %x, ptr %slot
; CHECK: %value = load i32, ptr %slot
; CHECK: %identity = add i32 %value, 0
; CHECK: ret i32 %identity
