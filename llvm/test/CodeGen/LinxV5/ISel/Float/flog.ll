; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

declare double @llvm.log.f64(double) nounwind readnone

; CHECK-LABEL: flog.fd:
; DAG: CALL, log
define double @flog.fd(double %a) {
  %log = call double @llvm.log.f64(double %a)
  ret double %log
}