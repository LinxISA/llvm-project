; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; CHECK: orreg:
; CHECK: or a0, a1, ->a0
define i64 @orreg(i64 %a, i64 %b) {
  %or = or i64 %a, %b
  ret i64 %or
}

; CHECK: andi:
; DAG: bic a0, 5, 59, ->a0
define i64 @andi(i64 %a) {
  %and = and i64 %a, 31
  ret i64 %and
}

; CHECK: andioverflow:
; DAG: bic a0, 11, 1, ->a0
define i64 @andioverflow(i64 %a) {
  %and = and i64 %a, -2049
  ret i64 %and
}

; CHECK: xoriw:
; CHECK: xoriw a0, 256, ->t
define i64 @xoriw(i64 %a, i64 %b) {
  %xor = xor i64 %a, 256
  %trunc = trunc i64 %xor to i32
  %ext = sext i32 %trunc to i64
  %sub = sub i64 %ext, %b
  ret i64 %sub
}

; CHECK: ornot:
; CHECK: or a0, a1.not, ->a0
define i64 @ornot(i64 %a, i64 %b) {
  %not = xor i64 %b, -1
  %or = or i64 %a, %not
  ret i64 %or
}

; CHECK: zext:
; DAG: bic a0, 32, 32, ->a0
define i64 @zext(i32 %a) {
  %zext = zext i32 %a to i64
  ret i64 %zext
}

; CHECK: sext:
; DAG: or zero, a0.sw, ->a0
define i64 @sext(i32 %a) {
  %sext = sext i32 %a to i64
  ret i64 %sext
}

; CHECK: not:
; DAG: xori a0, -1, ->a0
define i64 @not(i64 %a) {
  %not = xor i64 %a, -1
  ret i64 %not
}
