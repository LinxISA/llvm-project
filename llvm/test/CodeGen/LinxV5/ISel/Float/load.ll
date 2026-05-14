; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: load.fs:
; DAG: lwi.u [a0, 3], ->a0
define float @load.fs(ptr %p) {
  %ptr = getelementptr inbounds i8, ptr %p, i64 3
  %load = load float, ptr %ptr
  ret float %load
}