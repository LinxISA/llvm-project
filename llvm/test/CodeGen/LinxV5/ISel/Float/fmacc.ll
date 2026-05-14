; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: fmaf64:
; DAG: fmadd.fd a0, a1, a2, ->a0
define double @fmaf64(double %a, double %b, double %c) {
  %res = tail call double @llvm.fmuladd.f64(double %a, double %b, double %c)
  ret double %res
}

; CHECK-LABEL: fmsf32:
; DAG: fmsub.fs a0, a1, a2, ->a0
define float @fmsf32(float %a, float %b, float %c) {
  %n = fneg float %c
  %res = tail call float @llvm.fmuladd.f32(float %a, float %b, float %n)
  ret float %res
}

; CHECK-LABEL: fnmaf16:
; DAG: fnmadd.fh a0, a1, a2, ->a0
define half @fnmaf16(half %a, half %b, half %c) #1 {
  %res = tail call half @llvm.fmuladd.f16(half %a, half %b, half %c)
  %n = fneg half %res
  ret half %n
}

; CHECK-LABEL: fnms1f64:
; DAG: fnmsub.fd a0, a1, a2, ->a0
define double @fnms1f64(double %a, double %b, double %c) {
  %nb = fneg double %b
  %res = tail call double @llvm.fmuladd.f64(double %a, double %nb, double %c)
  ret double %res
}

; CHECK-LABEL: fnms2f64:
; DAG: fnmsub.fd a1, a0, a2, ->a0
define double @fnms2f64(double %a, double %b, double %c) {
  %na = fneg double %a
  %res = tail call double @llvm.fmuladd.f64(double %na, double %b, double %c)
  ret double %res
}

; CHECK-LABEL: fnma2f64:
; DAG: fnmadd.fd a1, a0, a2, ->a0
define double @fnma2f64(double %a, double %b, double %c) {
  %na = fneg double %a
  %nc = fneg double %c
  %res = tail call double @llvm.fmuladd.f64(double %na, double %b, double %nc)
  ret double %res
}

declare double @llvm.fmuladd.f64(double, double, double)
declare float @llvm.fmuladd.f32(float, float, float)
declare half @llvm.fmuladd.f16(half, half, half)