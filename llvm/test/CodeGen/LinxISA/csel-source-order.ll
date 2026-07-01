; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define i64 @select_i64(i64 %pred, i64 %truev, i64 %falsev) {
entry:
  %cmp = icmp ne i64 %pred, 0
  %sel = select i1 %cmp, i64 %truev, i64 %falsev
  ret i64 %sel
}

; CHECK-LABEL: select_i64:
; CHECK: cmp.nei a0, 0,{{[[:space:]]+}}->t
; CHECK-NEXT: csel t#1, a1, a2,{{[[:space:]]+}}->a0
