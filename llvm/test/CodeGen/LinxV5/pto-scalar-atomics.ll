; RUN: llc -mtriple=linx64v5 -mcpu=janus -linxv5-enable-clock-hand-opt=false < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -verify-machineinstrs -stop-after=finalize-isel < %s -o /dev/null
; RUN: llc -mtriple=linx64v5 -mcpu=janus -linxv5-enable-clock-hand-opt=false -filetype=obj < %s -o %t.o
; Native PTO atomics must never silently lower to atomic library calls.
; CHECK-NOT: __atomic
target triple = "linx64v5"

; CHECK-LABEL: cas_8_monotonic:
; CHECK: casb
define { i8, i1 } @cas_8_monotonic(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg ptr %p, i8 %expected, i8 %desired monotonic monotonic
  ret { i8, i1 } %r
}

; CHECK-LABEL: swap_8_monotonic:
; CHECK: swapb
define i8 @swap_8_monotonic(ptr %p, i8 %v) {
  %r = atomicrmw xchg ptr %p, i8 %v monotonic
  ret i8 %r
}

; CHECK-LABEL: cas_8_acquire:
; CHECK: casb.aq
define { i8, i1 } @cas_8_acquire(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg ptr %p, i8 %expected, i8 %desired acquire acquire
  ret { i8, i1 } %r
}

; CHECK-LABEL: swap_8_acquire:
; CHECK: swapb.aq
define i8 @swap_8_acquire(ptr %p, i8 %v) {
  %r = atomicrmw xchg ptr %p, i8 %v acquire
  ret i8 %r
}

; CHECK-LABEL: cas_8_release:
; CHECK: casb.rl
define { i8, i1 } @cas_8_release(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg ptr %p, i8 %expected, i8 %desired release monotonic
  ret { i8, i1 } %r
}

; CHECK-LABEL: swap_8_release:
; CHECK: swapb.rl
define i8 @swap_8_release(ptr %p, i8 %v) {
  %r = atomicrmw xchg ptr %p, i8 %v release
  ret i8 %r
}

; CHECK-LABEL: cas_8_acq_rel:
; CHECK: casb.aqrl
define { i8, i1 } @cas_8_acq_rel(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg ptr %p, i8 %expected, i8 %desired acq_rel acquire
  ret { i8, i1 } %r
}

; CHECK-LABEL: swap_8_acq_rel:
; CHECK: swapb.aqrl
define i8 @swap_8_acq_rel(ptr %p, i8 %v) {
  %r = atomicrmw xchg ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: cas_8_seq_cst:
; CHECK: casb.aqrl
define { i8, i1 } @cas_8_seq_cst(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg ptr %p, i8 %expected, i8 %desired seq_cst seq_cst
  ret { i8, i1 } %r
}

; CHECK-LABEL: swap_8_seq_cst:
; CHECK: swapb.aqrl
define i8 @swap_8_seq_cst(ptr %p, i8 %v) {
  %r = atomicrmw xchg ptr %p, i8 %v seq_cst
  ret i8 %r
}

; CHECK-LABEL: cas_16_monotonic:
; CHECK: cash
define { i16, i1 } @cas_16_monotonic(ptr %p, i16 %expected, i16 %desired) {
  %r = cmpxchg ptr %p, i16 %expected, i16 %desired monotonic monotonic
  ret { i16, i1 } %r
}

; CHECK-LABEL: swap_16_monotonic:
; CHECK: swaph
define i16 @swap_16_monotonic(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v monotonic
  ret i16 %r
}

; CHECK-LABEL: cas_16_acquire:
; CHECK: cash.aq
define { i16, i1 } @cas_16_acquire(ptr %p, i16 %expected, i16 %desired) {
  %r = cmpxchg ptr %p, i16 %expected, i16 %desired acquire acquire
  ret { i16, i1 } %r
}

; CHECK-LABEL: swap_16_acquire:
; CHECK: swaph.aq
define i16 @swap_16_acquire(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v acquire
  ret i16 %r
}

; CHECK-LABEL: cas_16_release:
; CHECK: cash.rl
define { i16, i1 } @cas_16_release(ptr %p, i16 %expected, i16 %desired) {
  %r = cmpxchg ptr %p, i16 %expected, i16 %desired release monotonic
  ret { i16, i1 } %r
}

; CHECK-LABEL: swap_16_release:
; CHECK: swaph.rl
define i16 @swap_16_release(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v release
  ret i16 %r
}

; CHECK-LABEL: cas_16_acq_rel:
; CHECK: cash.aqrl
define { i16, i1 } @cas_16_acq_rel(ptr %p, i16 %expected, i16 %desired) {
  %r = cmpxchg ptr %p, i16 %expected, i16 %desired acq_rel acquire
  ret { i16, i1 } %r
}

; CHECK-LABEL: swap_16_acq_rel:
; CHECK: swaph.aqrl
define i16 @swap_16_acq_rel(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: cas_16_seq_cst:
; CHECK: cash.aqrl
define { i16, i1 } @cas_16_seq_cst(ptr %p, i16 %expected, i16 %desired) {
  %r = cmpxchg ptr %p, i16 %expected, i16 %desired seq_cst seq_cst
  ret { i16, i1 } %r
}

; CHECK-LABEL: swap_16_seq_cst:
; CHECK: swaph.aqrl
define i16 @swap_16_seq_cst(ptr %p, i16 %v) {
  %r = atomicrmw xchg ptr %p, i16 %v seq_cst
  ret i16 %r
}

; CHECK-LABEL: cas_32_monotonic:
; CHECK: casw
define { i32, i1 } @cas_32_monotonic(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired monotonic monotonic
  ret { i32, i1 } %r
}

; CHECK-LABEL: swap_32_monotonic:
; CHECK: swapw
define i32 @swap_32_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: cas_32_acquire:
; CHECK: casw.aq
define { i32, i1 } @cas_32_acquire(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired acquire acquire
  ret { i32, i1 } %r
}

; CHECK-LABEL: swap_32_acquire:
; CHECK: swapw.aq
define i32 @swap_32_acquire(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: cas_32_release:
; CHECK: casw.rl
define { i32, i1 } @cas_32_release(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired release monotonic
  ret { i32, i1 } %r
}

; CHECK-LABEL: swap_32_release:
; CHECK: swapw.rl
define i32 @swap_32_release(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: cas_32_acq_rel:
; CHECK: casw.aqrl
define { i32, i1 } @cas_32_acq_rel(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired acq_rel acquire
  ret { i32, i1 } %r
}

; CHECK-LABEL: swap_32_acq_rel:
; CHECK: swapw.aqrl
define i32 @swap_32_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: cas_32_seq_cst:
; CHECK: casw.aqrl
define { i32, i1 } @cas_32_seq_cst(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired seq_cst seq_cst
  ret { i32, i1 } %r
}

; CHECK-LABEL: swap_32_seq_cst:
; CHECK: swapw.aqrl
define i32 @swap_32_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw xchg ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: cas_64_monotonic:
; CHECK: casd
define { i64, i1 } @cas_64_monotonic(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired monotonic monotonic
  ret { i64, i1 } %r
}

; CHECK-LABEL: swap_64_monotonic:
; CHECK: swapd
define i64 @swap_64_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: cas_64_acquire:
; CHECK: casd.aq
define { i64, i1 } @cas_64_acquire(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired acquire acquire
  ret { i64, i1 } %r
}

; CHECK-LABEL: swap_64_acquire:
; CHECK: swapd.aq
define i64 @swap_64_acquire(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: cas_64_release:
; CHECK: casd.rl
define { i64, i1 } @cas_64_release(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired release monotonic
  ret { i64, i1 } %r
}

; CHECK-LABEL: swap_64_release:
; CHECK: swapd.rl
define i64 @swap_64_release(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: cas_64_acq_rel:
; CHECK: casd.aqrl
define { i64, i1 } @cas_64_acq_rel(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired acq_rel acquire
  ret { i64, i1 } %r
}

; CHECK-LABEL: swap_64_acq_rel:
; CHECK: swapd.aqrl
define i64 @swap_64_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: cas_64_seq_cst:
; CHECK: casd.aqrl
define { i64, i1 } @cas_64_seq_cst(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired seq_cst seq_cst
  ret { i64, i1 } %r
}

; CHECK-LABEL: swap_64_seq_cst:
; CHECK: swapd.aqrl
define i64 @swap_64_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw xchg ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_32_add_monotonic:
; CHECK: lw.add
define i32 @rmw_32_add_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_add_acquire:
; CHECK: lw.add.aq
define i32 @rmw_32_add_acquire(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_add_release:
; CHECK: lw.add.rl
define i32 @rmw_32_add_release(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_add_acq_rel:
; CHECK: lw.add.aqrl
define i32 @rmw_32_add_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_add_seq_cst:
; CHECK: lw.add.aqrl
define i32 @rmw_32_add_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_sub_monotonic:
; CHECK: lw.add
define i32 @rmw_32_sub_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_sub_acquire:
; CHECK: lw.add.aq
define i32 @rmw_32_sub_acquire(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_sub_release:
; CHECK: lw.add.rl
define i32 @rmw_32_sub_release(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_sub_acq_rel:
; CHECK: lw.add.aqrl
define i32 @rmw_32_sub_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_sub_seq_cst:
; CHECK: lw.add.aqrl
define i32 @rmw_32_sub_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_and_monotonic:
; CHECK: lw.and
define i32 @rmw_32_and_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_and_acquire:
; CHECK: lw.and.aq
define i32 @rmw_32_and_acquire(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_and_release:
; CHECK: lw.and.rl
define i32 @rmw_32_and_release(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_and_acq_rel:
; CHECK: lw.and.aqrl
define i32 @rmw_32_and_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_and_seq_cst:
; CHECK: lw.and.aqrl
define i32 @rmw_32_and_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_or_monotonic:
; CHECK: lw.or
define i32 @rmw_32_or_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_or_acquire:
; CHECK: lw.or.aq
define i32 @rmw_32_or_acquire(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_or_release:
; CHECK: lw.or.rl
define i32 @rmw_32_or_release(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_or_acq_rel:
; CHECK: lw.or.aqrl
define i32 @rmw_32_or_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_or_seq_cst:
; CHECK: lw.or.aqrl
define i32 @rmw_32_or_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_xor_monotonic:
; CHECK: lw.xor
define i32 @rmw_32_xor_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_xor_acquire:
; CHECK: lw.xor.aq
define i32 @rmw_32_xor_acquire(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_xor_release:
; CHECK: lw.xor.rl
define i32 @rmw_32_xor_release(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_xor_acq_rel:
; CHECK: lw.xor.aqrl
define i32 @rmw_32_xor_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_xor_seq_cst:
; CHECK: lw.xor.aqrl
define i32 @rmw_32_xor_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_max_monotonic:
; CHECK: lw.smax
define i32 @rmw_32_max_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_max_acquire:
; CHECK: lw.smax.aq
define i32 @rmw_32_max_acquire(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_max_release:
; CHECK: lw.smax.rl
define i32 @rmw_32_max_release(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_max_acq_rel:
; CHECK: lw.smax.aqrl
define i32 @rmw_32_max_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_max_seq_cst:
; CHECK: lw.smax.aqrl
define i32 @rmw_32_max_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_min_monotonic:
; CHECK: lw.smin
define i32 @rmw_32_min_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_min_acquire:
; CHECK: lw.smin.aq
define i32 @rmw_32_min_acquire(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_min_release:
; CHECK: lw.smin.rl
define i32 @rmw_32_min_release(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_min_acq_rel:
; CHECK: lw.smin.aqrl
define i32 @rmw_32_min_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_min_seq_cst:
; CHECK: lw.smin.aqrl
define i32 @rmw_32_min_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umax_monotonic:
; CHECK: lw.umax
define i32 @rmw_32_umax_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umax_acquire:
; CHECK: lw.umax.aq
define i32 @rmw_32_umax_acquire(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umax_release:
; CHECK: lw.umax.rl
define i32 @rmw_32_umax_release(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umax_acq_rel:
; CHECK: lw.umax.aqrl
define i32 @rmw_32_umax_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umax_seq_cst:
; CHECK: lw.umax.aqrl
define i32 @rmw_32_umax_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umin_monotonic:
; CHECK: lw.umin
define i32 @rmw_32_umin_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v monotonic
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umin_acquire:
; CHECK: lw.umin.aq
define i32 @rmw_32_umin_acquire(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v acquire
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umin_release:
; CHECK: lw.umin.rl
define i32 @rmw_32_umin_release(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v release
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umin_acq_rel:
; CHECK: lw.umin.aqrl
define i32 @rmw_32_umin_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: rmw_32_umin_seq_cst:
; CHECK: lw.umin.aqrl
define i32 @rmw_32_umin_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v seq_cst
  ret i32 %r
}

; CHECK-LABEL: rmw_64_add_monotonic:
; CHECK: ld.add
define i64 @rmw_64_add_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_add_acquire:
; CHECK: ld.add.aq
define i64 @rmw_64_add_acquire(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_add_release:
; CHECK: ld.add.rl
define i64 @rmw_64_add_release(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_add_acq_rel:
; CHECK: ld.add.aqrl
define i64 @rmw_64_add_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_add_seq_cst:
; CHECK: ld.add.aqrl
define i64 @rmw_64_add_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_sub_monotonic:
; CHECK: ld.add
define i64 @rmw_64_sub_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_sub_acquire:
; CHECK: ld.add.aq
define i64 @rmw_64_sub_acquire(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_sub_release:
; CHECK: ld.add.rl
define i64 @rmw_64_sub_release(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_sub_acq_rel:
; CHECK: ld.add.aqrl
define i64 @rmw_64_sub_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_sub_seq_cst:
; CHECK: ld.add.aqrl
define i64 @rmw_64_sub_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_and_monotonic:
; CHECK: ld.and
define i64 @rmw_64_and_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_and_acquire:
; CHECK: ld.and.aq
define i64 @rmw_64_and_acquire(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_and_release:
; CHECK: ld.and.rl
define i64 @rmw_64_and_release(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_and_acq_rel:
; CHECK: ld.and.aqrl
define i64 @rmw_64_and_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_and_seq_cst:
; CHECK: ld.and.aqrl
define i64 @rmw_64_and_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_or_monotonic:
; CHECK: ld.or
define i64 @rmw_64_or_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_or_acquire:
; CHECK: ld.or.aq
define i64 @rmw_64_or_acquire(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_or_release:
; CHECK: ld.or.rl
define i64 @rmw_64_or_release(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_or_acq_rel:
; CHECK: ld.or.aqrl
define i64 @rmw_64_or_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_or_seq_cst:
; CHECK: ld.or.aqrl
define i64 @rmw_64_or_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_xor_monotonic:
; CHECK: ld.xor
define i64 @rmw_64_xor_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_xor_acquire:
; CHECK: ld.xor.aq
define i64 @rmw_64_xor_acquire(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_xor_release:
; CHECK: ld.xor.rl
define i64 @rmw_64_xor_release(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_xor_acq_rel:
; CHECK: ld.xor.aqrl
define i64 @rmw_64_xor_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_xor_seq_cst:
; CHECK: ld.xor.aqrl
define i64 @rmw_64_xor_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_max_monotonic:
; CHECK: ld.smax
define i64 @rmw_64_max_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_max_acquire:
; CHECK: ld.smax.aq
define i64 @rmw_64_max_acquire(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_max_release:
; CHECK: ld.smax.rl
define i64 @rmw_64_max_release(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_max_acq_rel:
; CHECK: ld.smax.aqrl
define i64 @rmw_64_max_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_max_seq_cst:
; CHECK: ld.smax.aqrl
define i64 @rmw_64_max_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_min_monotonic:
; CHECK: ld.smin
define i64 @rmw_64_min_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_min_acquire:
; CHECK: ld.smin.aq
define i64 @rmw_64_min_acquire(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_min_release:
; CHECK: ld.smin.rl
define i64 @rmw_64_min_release(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_min_acq_rel:
; CHECK: ld.smin.aqrl
define i64 @rmw_64_min_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_min_seq_cst:
; CHECK: ld.smin.aqrl
define i64 @rmw_64_min_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umax_monotonic:
; CHECK: ld.umax
define i64 @rmw_64_umax_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umax_acquire:
; CHECK: ld.umax.aq
define i64 @rmw_64_umax_acquire(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umax_release:
; CHECK: ld.umax.rl
define i64 @rmw_64_umax_release(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umax_acq_rel:
; CHECK: ld.umax.aqrl
define i64 @rmw_64_umax_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umax_seq_cst:
; CHECK: ld.umax.aqrl
define i64 @rmw_64_umax_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umin_monotonic:
; CHECK: ld.umin
define i64 @rmw_64_umin_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v monotonic
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umin_acquire:
; CHECK: ld.umin.aq
define i64 @rmw_64_umin_acquire(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v acquire
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umin_release:
; CHECK: ld.umin.rl
define i64 @rmw_64_umin_release(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v release
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umin_acq_rel:
; CHECK: ld.umin.aqrl
define i64 @rmw_64_umin_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v acq_rel
  ret i64 %r
}

; CHECK-LABEL: rmw_64_umin_seq_cst:
; CHECK: ld.umin.aqrl
define i64 @rmw_64_umin_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v seq_cst
  ret i64 %r
}

; CHECK-LABEL: unused_8_add_monotonic:
; CHECK: sb.add
define void @unused_8_add_monotonic(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v monotonic
  ret void
}

; CHECK-LABEL: unused_8_add_release:
; CHECK: sb.add.rl
define void @unused_8_add_release(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v release
  ret void
}

; CHECK-LABEL: unused_8_sub_monotonic:
; CHECK: sb.sub
define void @unused_8_sub_monotonic(ptr %p, i8 %v) {
  %r = atomicrmw sub ptr %p, i8 %v monotonic
  ret void
}

; CHECK-LABEL: unused_8_sub_release:
; CHECK: sb.sub.rl
define void @unused_8_sub_release(ptr %p, i8 %v) {
  %r = atomicrmw sub ptr %p, i8 %v release
  ret void
}

; CHECK-LABEL: load_8_monotonic:
; CHECK: lr.b
define i8 @load_8_monotonic(ptr %p) {
  %r = load atomic i8, ptr %p monotonic, align 1
  ret i8 %r
}

; CHECK-LABEL: load_8_acquire:
; CHECK: lr.b.aq
define i8 @load_8_acquire(ptr %p) {
  %r = load atomic i8, ptr %p acquire, align 1
  ret i8 %r
}

; CHECK-LABEL: load_8_seq_cst:
; CHECK: lr.b.aqrl
define i8 @load_8_seq_cst(ptr %p) {
  %r = load atomic i8, ptr %p seq_cst, align 1
  ret i8 %r
}

; CHECK-LABEL: store_8_monotonic:
; CHECK: sbi
define void @store_8_monotonic(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p monotonic, align 1
  ret void
}

; CHECK-LABEL: store_8_release:
; CHECK: swapb.rl
define void @store_8_release(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p release, align 1
  ret void
}

; CHECK-LABEL: store_8_seq_cst:
; CHECK: fence.d 3, 3
; CHECK: swapb.aqrl
define void @store_8_seq_cst(ptr %p, i8 %v) {
  store atomic i8 %v, ptr %p seq_cst, align 1
  ret void
}

; CHECK-LABEL: unused_16_add_monotonic:
; CHECK: sh.add
define void @unused_16_add_monotonic(ptr %p, i16 %v) {
  %r = atomicrmw add ptr %p, i16 %v monotonic
  ret void
}

; CHECK-LABEL: unused_16_add_release:
; CHECK: sh.add.rl
define void @unused_16_add_release(ptr %p, i16 %v) {
  %r = atomicrmw add ptr %p, i16 %v release
  ret void
}

; CHECK-LABEL: unused_16_sub_monotonic:
; CHECK: sh.sub
define void @unused_16_sub_monotonic(ptr %p, i16 %v) {
  %r = atomicrmw sub ptr %p, i16 %v monotonic
  ret void
}

; CHECK-LABEL: unused_16_sub_release:
; CHECK: sh.sub.rl
define void @unused_16_sub_release(ptr %p, i16 %v) {
  %r = atomicrmw sub ptr %p, i16 %v release
  ret void
}

; CHECK-LABEL: load_16_monotonic:
; CHECK: lr.h
define i16 @load_16_monotonic(ptr %p) {
  %r = load atomic i16, ptr %p monotonic, align 2
  ret i16 %r
}

; CHECK-LABEL: load_16_acquire:
; CHECK: lr.h.aq
define i16 @load_16_acquire(ptr %p) {
  %r = load atomic i16, ptr %p acquire, align 2
  ret i16 %r
}

; CHECK-LABEL: load_16_seq_cst:
; CHECK: lr.h.aqrl
define i16 @load_16_seq_cst(ptr %p) {
  %r = load atomic i16, ptr %p seq_cst, align 2
  ret i16 %r
}

; CHECK-LABEL: store_16_monotonic:
; CHECK: shi.u
define void @store_16_monotonic(ptr %p, i16 %v) {
  store atomic i16 %v, ptr %p monotonic, align 2
  ret void
}

; CHECK-LABEL: store_16_release:
; CHECK: swaph.rl
define void @store_16_release(ptr %p, i16 %v) {
  store atomic i16 %v, ptr %p release, align 2
  ret void
}

; CHECK-LABEL: store_16_seq_cst:
; CHECK: fence.d 3, 3
; CHECK: swaph.aqrl
define void @store_16_seq_cst(ptr %p, i16 %v) {
  store atomic i16 %v, ptr %p seq_cst, align 2
  ret void
}

; CHECK-LABEL: unused_32_add_monotonic:
; CHECK: sw.add
define void @unused_32_add_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_add_release:
; CHECK: sw.add.rl
define void @unused_32_add_release(ptr %p, i32 %v) {
  %r = atomicrmw add ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_sub_monotonic:
; CHECK: sw.sub
define void @unused_32_sub_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_sub_release:
; CHECK: sw.sub.rl
define void @unused_32_sub_release(ptr %p, i32 %v) {
  %r = atomicrmw sub ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: load_32_monotonic:
; CHECK: lr.w
define i32 @load_32_monotonic(ptr %p) {
  %r = load atomic i32, ptr %p monotonic, align 4
  ret i32 %r
}

; CHECK-LABEL: load_32_acquire:
; CHECK: lr.w.aq
define i32 @load_32_acquire(ptr %p) {
  %r = load atomic i32, ptr %p acquire, align 4
  ret i32 %r
}

; CHECK-LABEL: load_32_seq_cst:
; CHECK: lr.w.aqrl
define i32 @load_32_seq_cst(ptr %p) {
  %r = load atomic i32, ptr %p seq_cst, align 4
  ret i32 %r
}

; CHECK-LABEL: store_32_monotonic:
; CHECK: swi.u
define void @store_32_monotonic(ptr %p, i32 %v) {
  store atomic i32 %v, ptr %p monotonic, align 4
  ret void
}

; CHECK-LABEL: store_32_release:
; CHECK: swapw.rl
define void @store_32_release(ptr %p, i32 %v) {
  store atomic i32 %v, ptr %p release, align 4
  ret void
}

; CHECK-LABEL: store_32_seq_cst:
; CHECK: fence.d 3, 3
; CHECK: swapw.aqrl
define void @store_32_seq_cst(ptr %p, i32 %v) {
  store atomic i32 %v, ptr %p seq_cst, align 4
  ret void
}

; CHECK-LABEL: unused_64_add_monotonic:
; CHECK: sd.add
define void @unused_64_add_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_add_release:
; CHECK: sd.add.rl
define void @unused_64_add_release(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_sub_monotonic:
; CHECK: sd.sub
define void @unused_64_sub_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_sub_release:
; CHECK: sd.sub.rl
define void @unused_64_sub_release(ptr %p, i64 %v) {
  %r = atomicrmw sub ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: load_64_monotonic:
; CHECK: lr.d
define i64 @load_64_monotonic(ptr %p) {
  %r = load atomic i64, ptr %p monotonic, align 8
  ret i64 %r
}

; CHECK-LABEL: load_64_acquire:
; CHECK: lr.d.aq
define i64 @load_64_acquire(ptr %p) {
  %r = load atomic i64, ptr %p acquire, align 8
  ret i64 %r
}

; CHECK-LABEL: load_64_seq_cst:
; CHECK: lr.d.aqrl
define i64 @load_64_seq_cst(ptr %p) {
  %r = load atomic i64, ptr %p seq_cst, align 8
  ret i64 %r
}

; CHECK-LABEL: store_64_monotonic:
; CHECK: sdi.u
define void @store_64_monotonic(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p monotonic, align 8
  ret void
}

; CHECK-LABEL: store_64_release:
; CHECK: swapd.rl
define void @store_64_release(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p release, align 8
  ret void
}

; CHECK-LABEL: store_64_seq_cst:
; CHECK: fence.d 3, 3
; CHECK: swapd.aqrl
define void @store_64_seq_cst(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p seq_cst, align 8
  ret void
}

; CHECK-LABEL: loop_8_add:
; CHECK: casb.aqrl
define i8 @loop_8_add(ptr %p, i8 %v) {
  %r = atomicrmw add ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_sub:
; CHECK: casb.aqrl
define i8 @loop_8_sub(ptr %p, i8 %v) {
  %r = atomicrmw sub ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_and:
; CHECK: casb.aqrl
define i8 @loop_8_and(ptr %p, i8 %v) {
  %r = atomicrmw and ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_or:
; CHECK: casb.aqrl
define i8 @loop_8_or(ptr %p, i8 %v) {
  %r = atomicrmw or ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_xor:
; CHECK: casb.aqrl
define i8 @loop_8_xor(ptr %p, i8 %v) {
  %r = atomicrmw xor ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_min:
; CHECK: casb.aqrl
define i8 @loop_8_min(ptr %p, i8 %v) {
  %r = atomicrmw min ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_max:
; CHECK: casb.aqrl
define i8 @loop_8_max(ptr %p, i8 %v) {
  %r = atomicrmw max ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_umin:
; CHECK: casb.aqrl
define i8 @loop_8_umin(ptr %p, i8 %v) {
  %r = atomicrmw umin ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_umax:
; CHECK: casb.aqrl
define i8 @loop_8_umax(ptr %p, i8 %v) {
  %r = atomicrmw umax ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_8_nand:
; CHECK: casb.aqrl
define i8 @loop_8_nand(ptr %p, i8 %v) {
  %r = atomicrmw nand ptr %p, i8 %v acq_rel
  ret i8 %r
}

; CHECK-LABEL: loop_16_add:
; CHECK: cash.aqrl
define i16 @loop_16_add(ptr %p, i16 %v) {
  %r = atomicrmw add ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_sub:
; CHECK: cash.aqrl
define i16 @loop_16_sub(ptr %p, i16 %v) {
  %r = atomicrmw sub ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_and:
; CHECK: cash.aqrl
define i16 @loop_16_and(ptr %p, i16 %v) {
  %r = atomicrmw and ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_or:
; CHECK: cash.aqrl
define i16 @loop_16_or(ptr %p, i16 %v) {
  %r = atomicrmw or ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_xor:
; CHECK: cash.aqrl
define i16 @loop_16_xor(ptr %p, i16 %v) {
  %r = atomicrmw xor ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_min:
; CHECK: cash.aqrl
define i16 @loop_16_min(ptr %p, i16 %v) {
  %r = atomicrmw min ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_max:
; CHECK: cash.aqrl
define i16 @loop_16_max(ptr %p, i16 %v) {
  %r = atomicrmw max ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_umin:
; CHECK: cash.aqrl
define i16 @loop_16_umin(ptr %p, i16 %v) {
  %r = atomicrmw umin ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_umax:
; CHECK: cash.aqrl
define i16 @loop_16_umax(ptr %p, i16 %v) {
  %r = atomicrmw umax ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_16_nand:
; CHECK: cash.aqrl
define i16 @loop_16_nand(ptr %p, i16 %v) {
  %r = atomicrmw nand ptr %p, i16 %v acq_rel
  ret i16 %r
}

; CHECK-LABEL: loop_32_nand:
; CHECK: casw.aqrl
define i32 @loop_32_nand(ptr %p, i32 %v) {
  %r = atomicrmw nand ptr %p, i32 %v acq_rel
  ret i32 %r
}

; CHECK-LABEL: loop_64_nand:
; CHECK: casd.aqrl
define i64 @loop_64_nand(ptr %p, i64 %v) {
  %r = atomicrmw nand ptr %p, i64 %v acq_rel
  ret i64 %r
}


; A weak IR cmpxchg may use the strong native CAS. The expected high bit must
; be compared at the memory width (B/H zero-extend; W sign-extends).
; CHECK-LABEL: weak_byte:
; CHECK: casb.aq
; CHECK: cmp.eq
define i1 @weak_byte(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg weak ptr %p, i8 %expected, i8 %desired acquire monotonic
  %ok = extractvalue {i8, i1} %r, 1
  ret i1 %ok
}

; CHECK-LABEL: float_add:
; CHECK: casw.aqrl
define float @float_add(ptr %p, float %v) {
  %r = atomicrmw fadd ptr %p, float %v acq_rel
  ret float %r
}

; CHECK-LABEL: dead_acquire:
; CHECK: ld.add.aq
define void @dead_acquire(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: dead_seq_cst:
; CHECK: fence.d 3, 3
; CHECK: ld.add.aqrl
; CHECK: fence.d 3, 3
define void @dead_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw add ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_and_monotonic:
; CHECK: sw.and
define void @unused_32_and_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_and_release:
; CHECK: sw.and.rl
define void @unused_32_and_release(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_and_acquire:
; CHECK: lw.and.aq
define void @unused_32_and_acquire(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_and_acq_rel:
; CHECK: lw.and.aqrl
define void @unused_32_and_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_and_seq_cst:
; CHECK: lw.and.aqrl
define void @unused_32_and_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw and ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_or_monotonic:
; CHECK: sw.or
define void @unused_32_or_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_or_release:
; CHECK: sw.or.rl
define void @unused_32_or_release(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_or_acquire:
; CHECK: lw.or.aq
define void @unused_32_or_acquire(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_or_acq_rel:
; CHECK: lw.or.aqrl
define void @unused_32_or_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_or_seq_cst:
; CHECK: lw.or.aqrl
define void @unused_32_or_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw or ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_xor_monotonic:
; CHECK: sw.xor
define void @unused_32_xor_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_xor_release:
; CHECK: sw.xor.rl
define void @unused_32_xor_release(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_xor_acquire:
; CHECK: lw.xor.aq
define void @unused_32_xor_acquire(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_xor_acq_rel:
; CHECK: lw.xor.aqrl
define void @unused_32_xor_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_xor_seq_cst:
; CHECK: lw.xor.aqrl
define void @unused_32_xor_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw xor ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_min_monotonic:
; CHECK: sw.smin
define void @unused_32_min_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_min_release:
; CHECK: sw.smin.rl
define void @unused_32_min_release(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_min_acquire:
; CHECK: lw.smin.aq
define void @unused_32_min_acquire(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_min_acq_rel:
; CHECK: lw.smin.aqrl
define void @unused_32_min_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_min_seq_cst:
; CHECK: lw.smin.aqrl
define void @unused_32_min_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw min ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_max_monotonic:
; CHECK: sw.smax
define void @unused_32_max_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_max_release:
; CHECK: sw.smax.rl
define void @unused_32_max_release(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_max_acquire:
; CHECK: lw.smax.aq
define void @unused_32_max_acquire(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_max_acq_rel:
; CHECK: lw.smax.aqrl
define void @unused_32_max_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_max_seq_cst:
; CHECK: lw.smax.aqrl
define void @unused_32_max_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw max ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_umin_monotonic:
; CHECK: sw.umin
define void @unused_32_umin_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_umin_release:
; CHECK: sw.umin.rl
define void @unused_32_umin_release(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_umin_acquire:
; CHECK: lw.umin.aq
define void @unused_32_umin_acquire(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_umin_acq_rel:
; CHECK: lw.umin.aqrl
define void @unused_32_umin_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_umin_seq_cst:
; CHECK: lw.umin.aqrl
define void @unused_32_umin_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw umin ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_32_umax_monotonic:
; CHECK: sw.umax
define void @unused_32_umax_monotonic(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v monotonic
  ret void
}

; CHECK-LABEL: unused_32_umax_release:
; CHECK: sw.umax.rl
define void @unused_32_umax_release(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v release
  ret void
}

; CHECK-LABEL: unused_32_umax_acquire:
; CHECK: lw.umax.aq
define void @unused_32_umax_acquire(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v acquire
  ret void
}

; CHECK-LABEL: unused_32_umax_acq_rel:
; CHECK: lw.umax.aqrl
define void @unused_32_umax_acq_rel(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_32_umax_seq_cst:
; CHECK: lw.umax.aqrl
define void @unused_32_umax_seq_cst(ptr %p, i32 %v) {
  %r = atomicrmw umax ptr %p, i32 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_and_monotonic:
; CHECK: sd.and
define void @unused_64_and_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_and_release:
; CHECK: sd.and.rl
define void @unused_64_and_release(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_and_acquire:
; CHECK: ld.and.aq
define void @unused_64_and_acquire(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_and_acq_rel:
; CHECK: ld.and.aqrl
define void @unused_64_and_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_and_seq_cst:
; CHECK: ld.and.aqrl
define void @unused_64_and_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw and ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_or_monotonic:
; CHECK: sd.or
define void @unused_64_or_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_or_release:
; CHECK: sd.or.rl
define void @unused_64_or_release(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_or_acquire:
; CHECK: ld.or.aq
define void @unused_64_or_acquire(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_or_acq_rel:
; CHECK: ld.or.aqrl
define void @unused_64_or_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_or_seq_cst:
; CHECK: ld.or.aqrl
define void @unused_64_or_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw or ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_xor_monotonic:
; CHECK: sd.xor
define void @unused_64_xor_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_xor_release:
; CHECK: sd.xor.rl
define void @unused_64_xor_release(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_xor_acquire:
; CHECK: ld.xor.aq
define void @unused_64_xor_acquire(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_xor_acq_rel:
; CHECK: ld.xor.aqrl
define void @unused_64_xor_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_xor_seq_cst:
; CHECK: ld.xor.aqrl
define void @unused_64_xor_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw xor ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_min_monotonic:
; CHECK: sd.smin
define void @unused_64_min_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_min_release:
; CHECK: sd.smin.rl
define void @unused_64_min_release(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_min_acquire:
; CHECK: ld.smin.aq
define void @unused_64_min_acquire(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_min_acq_rel:
; CHECK: ld.smin.aqrl
define void @unused_64_min_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_min_seq_cst:
; CHECK: ld.smin.aqrl
define void @unused_64_min_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw min ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_max_monotonic:
; CHECK: sd.smax
define void @unused_64_max_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_max_release:
; CHECK: sd.smax.rl
define void @unused_64_max_release(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_max_acquire:
; CHECK: ld.smax.aq
define void @unused_64_max_acquire(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_max_acq_rel:
; CHECK: ld.smax.aqrl
define void @unused_64_max_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_max_seq_cst:
; CHECK: ld.smax.aqrl
define void @unused_64_max_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw max ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_umin_monotonic:
; CHECK: sd.umin
define void @unused_64_umin_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_umin_release:
; CHECK: sd.umin.rl
define void @unused_64_umin_release(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_umin_acquire:
; CHECK: ld.umin.aq
define void @unused_64_umin_acquire(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_umin_acq_rel:
; CHECK: ld.umin.aqrl
define void @unused_64_umin_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_umin_seq_cst:
; CHECK: ld.umin.aqrl
define void @unused_64_umin_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw umin ptr %p, i64 %v seq_cst
  ret void
}

; CHECK-LABEL: unused_64_umax_monotonic:
; CHECK: sd.umax
define void @unused_64_umax_monotonic(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v monotonic
  ret void
}

; CHECK-LABEL: unused_64_umax_release:
; CHECK: sd.umax.rl
define void @unused_64_umax_release(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v release
  ret void
}

; CHECK-LABEL: unused_64_umax_acquire:
; CHECK: ld.umax.aq
define void @unused_64_umax_acquire(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v acquire
  ret void
}

; CHECK-LABEL: unused_64_umax_acq_rel:
; CHECK: ld.umax.aqrl
define void @unused_64_umax_acq_rel(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v acq_rel
  ret void
}

; CHECK-LABEL: unused_64_umax_seq_cst:
; CHECK: ld.umax.aqrl
define void @unused_64_umax_seq_cst(ptr %p, i64 %v) {
  %r = atomicrmw umax ptr %p, i64 %v seq_cst
  ret void
}
