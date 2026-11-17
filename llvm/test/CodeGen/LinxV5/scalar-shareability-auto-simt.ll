; RUN: opt -mtriple=linx64v5 -mcpu=simt -passes='default<O2>' -S %s | FileCheck %s
; RUN: opt -mtriple=linx64v5 -mcpu=janus -passes=linx-v5-scalar-shareability -S %s | FileCheck %s
; SIMT bridge bodies retain SIMT memory semantics.
define i32 @bridge_body(ptr %p) "target-cpu"="simt" {
  %v = load volatile i32, ptr %p, align 4
  ret i32 %v
}
; CHECK-LABEL: define i32 @bridge_body
; CHECK-NOT: @llvm.linx.scalar
; CHECK: load volatile i32
; CHECK: ret i32
