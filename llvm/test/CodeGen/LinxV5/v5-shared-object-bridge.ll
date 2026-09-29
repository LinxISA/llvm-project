; RUN: llc < %s -O0 -mtriple=linx64v5 -mcpu=janus \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=false \
; RUN:   -stop-after=linxv5-shared-copy-elim -o - | FileCheck %s

target triple = "linx64v5"

; A Shared producer is written through an ordinary i64 object on one CFG path
; and read after the merge. The target pass removes only this proven bridge;
; it must not create a Shared copy or spill.

; CHECK-LABEL: name: shared_object_bridge
; CHECK: INLINEASM
; CHECK-SAME: regdef:Shared_ABS
; CHECK-NOT: SDI
; CHECK-NOT: LDI
; CHECK-NOT: COPY
; CHECK: INLINEASM
; CHECK: reguse:Shared_ABS

define void @shared_object_bridge(i1 %cond) {
entry:
  %slot = alloca i64, align 8
  br i1 %cond, label %left, label %right
left:
  %left_shared = call i64 asm sideeffect "", "=@2Sr"()
  store i64 %left_shared, ptr %slot, align 8
  br label %merge
right:
  %right_shared = call i64 asm sideeffect "", "=@2Sr"()
  store i64 %right_shared, ptr %slot, align 8
  br label %merge
merge:
  %shared = load i64, ptr %slot, align 8
  call void asm sideeffect "", "@2Sr"(i64 %shared)
  ret void
}
