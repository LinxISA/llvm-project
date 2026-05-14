; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.pow.f64(double, double) nounwind readnone

; CHECK-LABEL: fpow.fd:
; DAG: CALL, pow
define double @fpow.fd(double %a, double %b) {
  %pow = call double @llvm.pow.f64(double %a, double %b)
  ret double %pow
}