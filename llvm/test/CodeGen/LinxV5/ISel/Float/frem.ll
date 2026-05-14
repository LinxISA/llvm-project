; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: frem.fd:
; DAG: CALL, fmod
define double @frem.fd(double %a, double %b) {
  %rem = frem double %a, %b
  ret double %rem
}