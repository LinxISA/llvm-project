; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.exp.f64(double) nounwind readnone

; CHECK-LABEL: fexp.fd:
; DAG: CALL, exp
define double @fexp.fd(double %a) {
  %exp = call double @llvm.exp.f64(double %a)
  ret double %exp
}