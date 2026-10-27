; RUN: opt -passes='gvn,verify<domtree>' -S %s | FileCheck %s
; RUN: opt -gvn -verify-dom-info -S %s | FileCheck %s

define i32 @merge_chain(ptr %address, i1 %condition) {
; CHECK-LABEL: define i32 @merge_chain(
; CHECK: entry:
; CHECK-NEXT: br i1 %condition, label %left, label %right
; CHECK: left:
; CHECK-NEXT: br label %exit
; CHECK: right:
; CHECK-NEXT: br label %exit
; CHECK: exit:
; CHECK-NEXT: %again = load i32, ptr %address
; CHECK-NEXT: ret i32 %again
entry:
  %value = load i32, ptr %address
  br label %first
first:
  br label %second
second:
  br i1 %condition, label %left, label %right
left:
  br label %left_tail
left_tail:
  br label %exit
right:
  br label %right_tail
right_tail:
  br label %exit
exit:
  %again = load i32, ptr %address
  ret i32 %again
}
