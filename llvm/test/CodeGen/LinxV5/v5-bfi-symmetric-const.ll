; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; Issue #61: the symmetric 64-bit constant materialization packed M/N wrong
; (bit width divided by 8, then a 3-bit M field). It emitted
; `hl.bfi t, t, 4, 4` which only rewrites bits [7:4], so the SWAR masks of
; the ctzll expansion — and any symmetric 64-bit constant in an integer-only
; function — silently received a wrong value. M and N are both the bit width:
; result[M+N-1:M] = right[N-1:0] completes the symmetric value.
;
; The SWAR ctzll lowering exercises exactly this: it multiplies by
; 0x0101010101010101 and shifts right by 56, which is 0 when the upper half
; was never installed.

define dso_local void @sym32(i64* %arr) nounwind {
; CHECK-LABEL: sym32:
; CHECK: hl.bfi t#1, t#1, 32, 32, ->t
entry:
  store i64 u0x0101010101010101, i64* %arr
  ret void
}

define dso_local void @sym24(i64* %arr) nounwind {
; CHECK-LABEL: sym24:
; CHECK: hl.bfi t#1, t#1, 24, 24, ->t
entry:
  store i64 u0x0000123456123456, i64* %arr
  ret void
}

define dso_local void @symneg(i64* %arr) nounwind {
; CHECK-LABEL: symneg:
; CHECK: hl.bfi t#1, t#1, 32, 32, ->t
entry:
  store i64 u0xFFFF0000FFFF0000, i64* %arr
  ret void
}

; The issue's failing pattern: ctzll's SWAR mask in an integer-only function.
