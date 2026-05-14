; RUN: llc < %s -linxv5-enable-compress-inst=false --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

define void @shiftext1(ptr %p, i64 %a, i64 %b) {
; CHECK-LABEL: shiftext1:

; DAG:      slli a2, 1, ->t
; DAG-DAG:  addi t#{{[1-4]}}, 7, ->t
; DAG-NEXT: sd t#1, [a0, zero<<3]
; DAG-DAG:  sub t#{{[1-4]}}, a1, ->t
; DAG-NEXT: sdi t#1, [a0, 8]

  %sll = shl i64 %b, 1
  %extuse = add i64 7, %sll
  %idx1 = getelementptr inbounds i64, ptr %p, i64 0
  store i64 %extuse, ptr %idx1
  %nonext = sub i64 %sll, %a
  %idx2 = getelementptr inbounds i64, ptr %p, i64 1
  store i64 %nonext, ptr %idx2
  ret void
}

define void @shiftext2(ptr %p, i64 %a, i64 %b) {
; CHECK-LABLE: shiftext2:

; DAG:      add a1, a2<<1, ->t
; DAG-NEXT: sd t#1, [a0, zero<<3]
  %sll = shl i64 %b, 1
  %extuse = add i64 %a, %sll
  %idx1 = getelementptr inbounds i64, ptr %p, i64 0
  store i64 %extuse, ptr %idx1
  ret void
}

define void @shiftext3(ptr %p, i64 %a, i64 %b) {
; CHECK-LABEL: shiftext3:

; DAG:      slli a2, 1, ->t
; DAG-DAG:  addi t#{{[1-4]}}, 7, ->t
; DAG-NEXT: sd t#1, [a0, zero<<3]
; DAG-DAG:  add a1, a2<<1, ->t
; DAG-NEXT: sdi t#1, [a0, 8]

  %sll = shl i64 %b, 1
  %extuse = add i64 7, %sll
  %idx1 = getelementptr inbounds i64, ptr %p, i64 0
  store i64 %extuse, ptr %idx1
  %nonext = add i64 %sll, %a
  %idx2 = getelementptr inbounds i64, ptr %p, i64 1
  store i64 %nonext, ptr %idx2
  ret void
}
