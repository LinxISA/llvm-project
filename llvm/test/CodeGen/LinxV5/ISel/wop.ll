; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; CHECK-LABEL: wop1:
; CHECK:      addw a0, a1, ->t
; CHECK-NEXT: add t#1, a2, ->a0
define i64 @wop1(i32 %a, i32 %b, i64 %c) {
  %add1 = add i32 %a, %b
  %ext = sext i32 %add1 to i64
  %add2 = add i64 %ext, %c
  ret i64 %add2
}

; CHECK-LABEL: wop2:
; CHECK:      addw a0, a1, ->t
; CHECK-NEXT: add t#1, a2, ->a0
define i64 @wop2(i64 %a, i64 %b, i64 %c) {
  %add1 = add i64 %a, %b
  %trunc = trunc i64 %add1 to i32
  %ext = sext i32 %trunc to i64
  %add2 = add i64 %ext, %c
  ret i64 %add2
}

; CHECK-LABEL: wxuses:
; COMM: one W use + one X use

; DAG:      add a0, a1, ->t
; DAG-DAG:  add a2, t#{{[1-4]}}.sw, ->t
; DAG-DAG:  add a0, t#{{[1-4]}}, ->t
; DAG-NEXT: add t#{{[1-4]}}, t#{{[1-4]}}, ->a0

define i64 @wxuses(i64 %a, i64 %b, i64 %c) {
  %add1 = add i64 %a, %b
  %trunc = trunc i64 %add1 to i32
  %ext = sext i32 %trunc to i64
  %wuse = add i64 %ext, %c
  %xuse = add i64 %a, %add1
  %add4 = add i64 %wuse, %xuse
  ret i64 %add4
}

; CHECK-LABEL: wwuses:
; COMM: one sw_ext use + one W use

; DAG:      addw a0, a1, ->t
; DAG-DAG:  add t#{{[1-4]}}, a2, ->t
; DAG-DAG:  sub t#{{[1-4]}}, a2, ->t
; DAG-NEXT: add t#{{[1-4]}}, t#{{[1-4]}}, ->a0

define i64 @wwuses(i64 %a, i64 %b, i64 %c) {
  %add1 = add i64 %a, %b
  %trunc = trunc i64 %add1 to i32
  %ext = sext i32 %trunc to i64
  %swextuse = add i64 %ext, %c
  %wuse =     sub i64 %ext, %c
  %add2 = add i64 %swextuse, %wuse
  ret i64 %add2
}