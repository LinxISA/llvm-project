; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv
; RUN: llc < %s --march=linx64v5 -O2 -filetype=obj | llvm-objdump -d - | FileCheck %s --check-prefix=RAW

; Issue #61: the symmetric 64-bit constant materialization packed M/N wrong
; (bit width divided by 8, then a 3-bit M field). It emitted
; `hl.bfi t, t, 4, 3` which selects a wrapping interval, so the SWAR masks of
; the ctzll expansion — and any symmetric 64-bit constant in an integer-only
; function — silently received a wrong value. HL.BFI directly encodes the raw
; inclusive immr/imms endpoints. A W-bit field at offset W therefore uses
; assembly operands W,2W-1.
;
; The SWAR ctzll lowering exercises exactly this: it multiplies by
; 0x0101010101010101 and shifts right by 56, which is 0 when the upper half
; was never installed.

define dso_local void @sym32(i64* %arr) nounwind {
; CHECK-LABEL: sym32:
; CHECK: hl.bfi t#1, t#1, 32, 63, ->t
entry:
  store i64 u0x0101010101010101, i64* %arr
  ret void
}

define dso_local void @sym24(i64* %arr) nounwind {
; CHECK-LABEL: sym24:
; CHECK: hl.bfi t#1, t#1, 24, 47, ->t
entry:
  store i64 u0x0000123456123456, i64* %arr
  ret void
}

define dso_local void @symneg(i64* %arr) nounwind {
; CHECK-LABEL: symneg:
; CHECK: hl.bfi t#1, t#1, 32, 63, ->t
entry:
  store i64 u0xFFFF0000FFFF0000, i64* %arr
  ret void
}

; This exact constant is emitted when the Top-K input initializer combines two
; adjacent U32 stores.  A wrong wrapping encoding changed 0xf123 into 0x1e246.
define dso_local void @sym32_topk_pair(i64* %arr) nounwind {
; CHECK-LABEL: sym32_topk_pair:
; CHECK: hl.bfi t#1, t#1, 32, 63, ->t
; RAW-LABEL: <sym32_topk_pair>:
; RAW: fe0e 2fcd 018c        hl.bfi t#1, t#1, 32, 63, ->t
entry:
  store i64 u0x0000F1230000F123, i64* %arr
  ret void
}

; The issue's failing pattern: ctzll's SWAR mask in an integer-only function.
