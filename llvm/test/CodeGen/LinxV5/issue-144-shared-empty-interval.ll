; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -stop-after=linxv5-shared-copy-elim %s -o - | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t

target triple = "linx64v5"

; CHECK-LABEL: name: undef_only
; CHECK: INLINEASM
; CHECK-SAME: reguse:Shared_ABS
; CHECK-SAME: undef {{.*}}$shared_s0
; CHECK: INLINEASM
; CHECK-SAME: reguse:Shared_ABS
; CHECK-SAME: undef {{.*}}$shared_s0
define void @undef_only() {
  call void asm sideeffect "", "@2Sr"(i64 undef)
  call void asm sideeffect "", "@2Sr"(i64 undef)
  ret void
}

; CHECK-LABEL: name: mixed_intervals
; CHECK: INLINEASM
; CHECK-SAME: regdef:Shared_ABS
; CHECK-SAME: def {{.*}}$shared_s0
; CHECK: INLINEASM
; CHECK-SAME: regdef:Shared_ABS
; CHECK-SAME: def {{.*}}$shared_s1
; CHECK: INLINEASM
; CHECK-SAME: undef {{.*}}$shared_s0
; CHECK: INLINEASM
; CHECK-SAME: killed {{.*}}$shared_s0
; CHECK: INLINEASM
; CHECK-SAME: killed {{.*}}$shared_s1
define void @mixed_intervals() {
  %first = call i64 asm sideeffect "", "=@2Sr"()
  %second = call i64 asm sideeffect "", "=@2Sr"()
  call void asm sideeffect "", "@2Sr"(i64 undef)
  call void asm sideeffect "", "@2Sr"(i64 %first)
  call void asm sideeffect "", "@2Sr"(i64 %second)
  ret void
}

; CHECK-LABEL: name: no_shared_intervals
; CHECK: PseudoRET
define void @no_shared_intervals() {
  ret void
}
