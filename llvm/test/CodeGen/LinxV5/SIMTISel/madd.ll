; RUN: llc < %s --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

; MIR: name: fmaf64
; MIR: SIMT_FMADD_SCAR 0, %1, 0, %2, 0, %3, 0
; ASM: fmaf64:
; ASM: l.fmadd ri1.fd, ri2.fd, ri3.fd, ->t.d
define void @fmaf64(ptr %p, double %a, double %b, double %c) #1 {
  %res = tail call double @llvm.fmuladd.f64(double %a, double %b, double %c)
  store double %res, ptr %p
  ret void
}

; MIR: name: fmsf32
; MIR: SIMT_FMSUB_SCAR 1, %1, 1, %2, 1, %3, 1
; ASM: fmsf32:
; ASM: l.fmsub ri1.fs, ri2.fs, ri3.fs, ->t.w
define void @fmsf32(ptr %p, float %a, float %b, float %c) #1 {
  %n = fneg float %c
  %res = tail call float @llvm.fmuladd.f32(float %a, float %b, float %n)
  store float %res, ptr %p
  ret void
}

; MIR: name: fnmaf16
; MIR: SIMT_FNMADD_SCAR 2, %1, 2, %2, 2, %3, 2
; ASM: fnmaf16:
; ASM: l.fnmadd ri1.fh, ri2.fh, ri3.fh, ->t.h
define void @fnmaf16(ptr %p, half %a, half %b, half %c) #1 {
  %res = tail call half @llvm.fmuladd.f16(half %a, half %b, half %c)
  %n = fneg half %res
  store half %n, ptr %p
  ret void
}

; MIR: name: fnms1f64
; MIR: SIMT_FNMSUB_SCAR 0, %1, 0, %2, 0, %3, 0
; ASM: fnms1f64:
; ASM: l.fnmsub ri1.fd, ri2.fd, ri3.fd, ->t.d
define void @fnms1f64(ptr %p, double %a, double %b, double %c) #1 {
  %nb = fneg double %b
  %res = tail call double @llvm.fmuladd.f64(double %a, double %nb, double %c)
  store double %res, ptr %p
  ret void
}

; MIR: name: fnms2f64
; MIR: SIMT_FNMSUB_SCAR 0, %2, 0, %1, 0, %3, 0
; ASM: fnms2f64:
; ASM: l.fnmsub ri2.fd, ri1.fd, ri3.fd, ->t.d
define void @fnms2f64(ptr %p, double %a, double %b, double %c) #1 {
  %na = fneg double %a
  %res = tail call double @llvm.fmuladd.f64(double %na, double %b, double %c)
  store double %res, ptr %p
  ret void
}

; MIR: name: fnma2f64
; MIR: SIMT_FNMADD_SCAR 0, %2, 0, %1, 0, %3, 0
; ASM: fnma2f64:
; ASM: l.fnmadd ri2.fd, ri1.fd, ri3.fd, ->t.d
define void @fnma2f64(ptr %p, double %a, double %b, double %c) #1 {
  %na = fneg double %a
  %nc = fneg double %c
  %res = tail call double @llvm.fmuladd.f64(double %na, double %b, double %nc)
  store double %res, ptr %p
  ret void
}

declare double @llvm.fmuladd.f64(double, double, double)
declare float @llvm.fmuladd.f32(float, float, float)
declare half @llvm.fmuladd.f16(half, half, half)
declare bfloat @llvm.fmuladd.bf16(bfloat, bfloat, bfloat)

attributes #1 = {"__vec__"}