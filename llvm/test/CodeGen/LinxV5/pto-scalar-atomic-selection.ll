; RUN: llc -mtriple=linx64v5 -mcpu=janus -stop-after=finalize-isel -verify-machineinstrs < %s | FileCheck %s
; Check memory widths/order survive promotion, and cmpxchg success compares
; only memory bits even when the native old-value extension differs by width.
target triple = "linx64v5"

; CHECK-LABEL: name: byte_success
; CHECK: %[[OLD:[0-9]+]]:mixedgpr = CASB_AQ
; CHECK-SAME: volatile load store acquire acquire (s8)
; CHECK: %[[MASK:[0-9]+]]:mixedgpr = BIC %[[OLD]], 8, 56
; CHECK: %[[EXPECTED:[0-9]+]]:mixedgpr = BIC %{{[0-9]+}}, 8, 56
; CHECK: CMP_EQ killed %[[MASK]], killed %[[EXPECTED]]
define {i8, i1} @byte_success(ptr %p, i8 %expected, i8 %desired) {
  %r = cmpxchg volatile ptr %p, i8 %expected, i8 %desired acquire acquire
  ret {i8, i1} %r
}

; CHECK-LABEL: name: word_success
; CHECK: %[[OLDW:[0-9]+]]:mixedgpr = CASW_AQRL
; CHECK-SAME: load store acq_rel acquire (s32)
; CHECK: %[[MASKW:[0-9]+]]:mixedgpr = BIC %[[OLDW]], 32, 32
; CHECK: %[[EXPECTEDW:[0-9]+]]:mixedgpr = BIC %{{[0-9]+}}, 32, 32
; CHECK: CMP_EQ killed %[[MASKW]], killed %[[EXPECTEDW]]
define {i32, i1} @word_success(ptr %p, i32 %expected, i32 %desired) {
  %r = cmpxchg ptr %p, i32 %expected, i32 %desired acq_rel acquire
  ret {i32, i1} %r
}

; CHECK-LABEL: name: sc_cas
; CHECK: FENCE_D 3, 3
; CHECK: CASD_AQRL
; CHECK-SAME: load store seq_cst seq_cst (s64)
; CHECK: FENCE_D 3, 3
define i64 @sc_cas(ptr %p, i64 %expected, i64 %desired) {
  %r = cmpxchg ptr %p, i64 %expected, i64 %desired seq_cst seq_cst
  %old = extractvalue {i64, i1} %r, 0
  ret i64 %old
}

; CHECK-LABEL: name: ordered_store
; CHECK: SWAPD_RL
; CHECK-SAME: :: (load store release (s64)
; CHECK: PseudoRET
define void @ordered_store(ptr %p, i64 %v) {
  store atomic i64 %v, ptr %p release, align 8
  ret void
}
