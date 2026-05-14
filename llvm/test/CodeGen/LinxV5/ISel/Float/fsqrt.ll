; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.sqrt.f64(double) nounwind readnone

; CHECK-LABEL: fsqrt.fd:
; DAG: CALL, sqrt
define double @fsqrt.fd(double %a) {
  %sqrt = call double @llvm.sqrt.f64(double %a)
  ret double %sqrt
}