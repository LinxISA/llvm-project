; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s
; RUN: llc -mtriple=linx64 -O2 -filetype=obj < %s -o - | llvm-objdump -d --triple=linx64 - | FileCheck %s --check-prefix=OBJ

define i64 @select_i64(i64 %pred, i64 %truev, i64 %falsev) {
entry:
  %cmp = icmp ne i64 %pred, 0
  %sel = select i1 %cmp, i64 %truev, i64 %falsev
  ret i64 %sel
}

; CHECK-LABEL: select_i64:
; CHECK: cmp.nei a0, 0,{{[[:space:]]+}}->t
; CHECK-NEXT: csel t#1, a1, a2,{{[[:space:]]+}}->a0
; OBJ: 77 81 41 c0{{[[:space:]]+}}csel{{[[:space:]]+}}t#1, a1, a2,{{[[:space:]]+}}->a0
