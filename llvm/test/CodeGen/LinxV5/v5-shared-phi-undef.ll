; RUN: llc < %s -mtriple=linx64v5 -mcpu=janus \
; RUN:   -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=false \
; RUN:   -stop-after=linxv5-shared-copy-elim -o - | FileCheck %s

target triple = "linx64v5"

; A Shared handle is published on one control-flow path and consumed after
; the merge. The unsupported Shared->GPR->Shared bridge must be removed after
; physical register rewriting; no MOVR/ORI fallback is permitted.
;
; CHECK-LABEL: name: shared_pe0_only
; CHECK: INLINEASM
; CHECK-SAME: regdef:Shared_ABS
; CHECK-NOT: shared_s0 = COPY
; CHECK-NOT: COPY killed renamable $r2
; CHECK: INLINEASM
; CHECK-SAME: reguse:Shared_ABS
; CHECK-NOT: MOVR
; CHECK-NOT: ORI

define void @shared_pe0_only(i1 %cond) {
entry:
  br i1 %cond, label %publish, label %merge
publish:
  %shared = call i64 asm sideeffect "", "=@2Sr"()
  br label %merge
merge:
  %handle = phi i64 [ %shared, %publish ], [ undef, %entry ]
  call void asm sideeffect "", "@2Sr"(i64 %handle)
  ret void
}
