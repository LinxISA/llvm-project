; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv  --check-prefixes=DAG
; RUN: llc < %s --march=linx64 -linxv5-enable-csel=false -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAGNOCSEL

; CHECK-LABEL: csel:
; TODO: remove this bic
; DAG: bic a0, 1, 63, ->t
; DAG: csel t#1, a1, a2, ->a0
; DAGNOCSEL-LABEL: csel:
; DAGNOCSEL: setc.andi a0, 1
define i64 @csel(i64 %p, i64 %a, i64 %b) {
  %trunc = trunc i64 %p to i1
  %select = select i1 %trunc, i64 %a, i64 %b
  ret i64 %select
}