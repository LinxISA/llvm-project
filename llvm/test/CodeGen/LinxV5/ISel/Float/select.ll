; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: csel.fs:
; COMM: TODO: actually this `andi` is nosense.
; DAG:      bic a0, 1, 63, ->t
; DAG-NEXT: csel t#1, a1, a2, ->a0
define float @csel.fs(i1 %p, float %a, float %b) {
  %csel = select i1 %p, float %a, float %b
  ret float %csel
}