; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.cos.f64(double) nounwind readnone

; CHECK-LABEL: fcos.fd:
; DAG: CALL, cos
define double @fcos.fd(double %a) {
  %cos = call double @llvm.cos.f64(double %a)
  ret double %cos
}