; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: fmul_fd:
; DAG: fmul.fd a0, a1, ->a0
define double @fmul_fd(double %a, double %b) {
  %mul = fmul double %a, %b
  ret double %mul
}

; CHECK-LABEL: fdiv_fs:
; CHECK: fdiv.fs a0, a1, ->a0
define float @fdiv_fs(float %a, float %b) {
  %div = fdiv float %a, %b
  ret float %div
}