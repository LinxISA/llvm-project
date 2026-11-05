; RUN: llc -mtriple=linx64v5 -mcpu=janus -linxv5-enable-clock-hand-opt=false < %s | FileCheck %s
; Unsupported size/alignment must keep the generic atomic runtime fallback.
target triple = "linx64v5"

; CHECK-LABEL: unaligned_load:
; CHECK: __atomic_load
define i64 @unaligned_load(ptr %p) {
  %v = load atomic i64, ptr %p acquire, align 1
  ret i64 %v
}

; CHECK-LABEL: unaligned_store:
; CHECK: __atomic_store
define void @unaligned_store(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p release, align 1
  ret void
}

; CHECK-LABEL: wide_cas:
; CHECK: __atomic_compare_exchange
define {i128, i1} @wide_cas(ptr %p, i128 %a, i128 %b) {
  %r = cmpxchg ptr %p, i128 %a, i128 %b seq_cst seq_cst
  ret {i128, i1} %r
}

; CHECK-LABEL: unaligned_rmw:
; CHECK: __atomic
define i64 @unaligned_rmw(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v acq_rel, align 1
  ret i64 %r
}
