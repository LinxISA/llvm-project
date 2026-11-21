; RUN: not llc -mtriple=linx64v5 -mcpu=janus < %s 2>&1 | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=simt -stop-after=finalize-isel < %s | FileCheck %s --check-prefix=SIMT
; CHECK: couldn't allocate output register for constraint 'vr'
; CHECK: couldn't allocate output register for constraint 'vr'
; CHECK: couldn't allocate input reg for constraint 'vr'
; CHECK-NOT: Do not know how to promote
; SIMT: INLINEASM
; SIMT: INLINEASM
; SIMT: INLINEASM
; LLVM IR encodes the two-letter vr constraint as ^vr (Clang uses @2vr).
define void @scalar(ptr %p) {
  %src = load float, ptr %p
  %r = call i32 asm sideeffect "", "=^vr,^vr"(float %src)
  ret void
}
define void @packed(ptr %p) {
  %src = load float, ptr %p
  %r = call i32 asm sideeffect "", "=^vr,^vr,^vr"(float %src, float %src)
  ret void
}
define void @input_only(i32 %src) {
  call void asm sideeffect "", "^vr"(i32 %src)
  ret void
}
