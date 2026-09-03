; RUN: llc < %s -mtriple=linx64v5 -O2 | FileCheck %s

; f16 remainder must be promoted to f32 before the fmodf libcall. There is no
; target-independent f16 remainder libcall name in LLVM 15.

define half @frem_f16(half %a, half %b) {
; CHECK-LABEL: frem_f16:
; CHECK:       L.BSTART.FP{{[ \t]+}}CALL, fmodf
; CHECK:       fcvt.fh2fs
; CHECK:       fcvt.fh2fs
; CHECK:       fcvt.fs2fh
; CHECK:       FRET.STK
  %r = frem half %a, %b
  ret half %r
}
