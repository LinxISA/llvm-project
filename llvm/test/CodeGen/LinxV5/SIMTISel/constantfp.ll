; RUN: llc < %s --march=linx64v5 -O2 -stop-after=finalize-isel 2>&1 | FileCheck %s --check-prefix=MIR
; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefix=ASM

; MIR-NOT: load (s64) from constant-pool
; ASM-NOT: l.ldi.u [t#1.sd
; ASM: l.addi zero.sd, 1023, ->t.d
; ASM: l.slli t#1.sd, 52, ->t.d
; ASM: l.fadd ri1.fd, t#1.fd, ->t.d
define void @constf64(ptr %p, double %a) #1 {
; COMM: We should not lower the fp-constant 1.00000e+00 as `load (s64) from constant-pool`
; COMM: instead of 'selectImm'
  %b = fadd double %a, 1.00000e+00
  store double %b, ptr %p
  ret void
}

attributes #1 = {"__vec__"}
