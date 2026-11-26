; RUN: opt -mtriple=linx64v5 -passes=linx-v5-tile-helper-prepare -verify-each -S %s | FileCheck %s

target triple = "linx64v5"

; A C++ reference must not turn an opaque Shared handle into scoped memory.
; Leave real GM stores in order, including an overwritten store: scoping has
; not assigned their PE ownership yet, so ordinary DSE is not legal here.
define internal void @consume(ptr %handle, ptr %gm) {
  %s = load i64, ptr %handle
  call void asm sideeffect "", "@2Sr"(i64 %s)
  store i32 1, ptr %gm
  store i32 2, ptr %gm
  ret void
}

; CHECK-LABEL: define void @caller(
; CHECK-NOT: alloca
; CHECK: %s = call i64 asm sideeffect "", "=@2Sr"()
; CHECK-NOT: store i64
; CHECK-NOT: load i64
; CHECK: call void asm sideeffect "", "@2Sr"(i64 %s)
; CHECK-NEXT: store i32 1, ptr %gm
; CHECK-NEXT: store i32 2, ptr %gm
; CHECK-NEXT: ret void
define void @caller(ptr %gm) {
  %slot = alloca i64
  %s = call i64 asm sideeffect "", "=@2Sr"()
  store i64 %s, ptr %slot
  call void @consume(ptr %slot, ptr %gm) "linx-scalar-shareability"="intra"
  ret void
}

define internal void @noinline_tile() noinline {
  call void asm sideeffect "", "@2Tr"(<32 x i32> zeroinitializer)
  ret void
}
define weak void @replaceable_tile() {
  call void asm sideeffect "", "@2Tr"(<32 x i32> zeroinitializer)
  ret void
}
define internal void @nullable_tile() null_pointer_is_valid {
  call void asm sideeffect "", "@2Tr"(<32 x i32> zeroinitializer)
  ret void
}
define internal void @scoped_tile() "linx-scalar-shareability"="intra" {
  call void asm sideeffect "", "@2Tr"(<32 x i32> zeroinitializer)
  ret void
}
define internal void @owned_tile() "linx-tile-pe-domain"="8" {
  call void asm sideeffect "", "@2Tr"(<32 x i32> zeroinitializer)
  ret void
}
; CHECK-LABEL: define void @boundaries(
; CHECK-NEXT: call void @noinline_tile()
; CHECK-NEXT: call void @replaceable_tile()
; CHECK-NEXT: call void @nullable_tile()
; CHECK-NEXT: call void @scoped_tile()
; CHECK-NEXT: call void @owned_tile()
; CHECK-NEXT: ret void
define void @boundaries() {
  call void @noinline_tile()
  call void @replaceable_tile()
  call void @nullable_tile()
  call void @scoped_tile()
  call void @owned_tile()
  ret void
}

; Explicit caller ownership also prevents moving a helper's accesses into it.
; CHECK-LABEL: define void @owned_caller(
; CHECK: call void @consume(ptr %handle, ptr %gm)
define void @owned_caller(ptr %handle, ptr %gm) "linx-tile-pe-domain"="8" {
  call void @consume(ptr %handle, ptr %gm)
  ret void
}

; A non-default call-site scope must not be pulled into the default caller.
; CHECK-LABEL: define void @scoped_call(
; CHECK: call void @consume(ptr %handle, ptr %gm)
define void @scoped_call(ptr %handle, ptr %gm) {
  call void @consume(ptr %handle, ptr %gm) "linx-scalar-shareability"="private"
  ret void
}

