; RUN: llc < %s --march=linx64 -linxv5-enable-legacy-isel=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.fabs.f64(double) nounwind readnone

; CHECK-LABEL: fabs.fd:
; DAG: bic a0, 63, 1, ->a0
define double @fabs.fd(double %a) {
  %abs = call double @llvm.fabs.f64(double %a)
  ret double %abs
}