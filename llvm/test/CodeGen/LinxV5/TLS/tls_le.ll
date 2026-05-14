; RUN: llc < %s -march=linx64v5 -O2 2>&1 | FileCheck %s --dump-input always -vv

@a = external hidden thread_local global i32, align 4
define dso_local i32* @getaddr() nounwind {
entry:
; CHECK-LABEL: getaddr:
; CHECK-DAG: ssrget	0
; CHECK-DAG: lui %tprel_hi(a)
; CHECK-DAG-NEXT: addi  t#1, %tprel_lo(a)
; CHECK: add t#{{[1-4]}}, t#{{[1-4]}}, ->a0
  ret i32* @a
}

define dso_local i32 @getval() nounwind {
entry:
; CHECK-LABEL: getval:
; CHECK-DAG: ssrget	0
; CHECK-DAG: lui %tprel_hi(a)
; CHECK-DAG-NEXT: addi t#1, %tprel_lo(a)
; CHECK: lw [t#{{[1-4]}}, t#{{[1-4]}}], ->a0
  %0 = load i32, i32* @a, align 4
  ret i32 %0
}