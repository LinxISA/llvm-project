; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.sin.f64(double) nounwind readnone

; CHECK-LABEL: fsin.fd:
; DAG: CALL, sin
define double @fsin.fd(double %a) {
  %sin = call double @llvm.sin.f64(double %a)
  ret double %sin
}