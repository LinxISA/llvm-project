; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define i32 @linx_f64_bounds(double %x) {
entry:
  %lo = fcmp ole double %x, 0xC3E0000000000000
  %hi = fcmp ogt double %x, 0x43E0000000000000
  %bad = or i1 %lo, %hi
  %z = zext i1 %bad to i32
  ret i32 %z
}

; CHECK-LABEL: linx_f64_bounds:
; CHECK-NOT: lwu.pcr
; CHECK: ld.pcr
; CHECK-NEXT: flt.fd
; CHECK: ld.pcr
; CHECK-NEXT: fge.fd
