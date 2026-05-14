; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; CHECK: mul:
; CHECK: mul a0, a1, ->a0
define i64 @mul(i64 %a, i64 %b) {
  %mul = mul i64 %a, %b
  ret i64 %mul
}

; CHECK: mulw:
; CHECK: mulw a0, a1, ->a0
define i32 @mulw(i32 %a, i32 %b) {
  %mul = mul i32 %a, %b
  ret i32 %mul
}

; COMM: TODO: add muluw patterns and testcases

; CHECK: div:
; CHECK: div a0, a1, ->a0
define i64 @div(i64 %a, i64 %b) {
  %div = sdiv i64 %a, %b
  ret i64 %div
}

; CHECK: divu:
; CHECK: divu a0, a1, ->a0
define i64 @divu(i64 %a, i64 %b) {
  %div = udiv i64 %a, %b
  ret i64 %div
}

; CHECK: divw:
; CHECK: divw a0, a1, ->a0
define i32 @divw(i32 %a, i32 %b) {
  %div = sdiv i32 %a, %b
  ret i32 %div
}

; CHECK: divuw:
; CHECK: divuw a0, a1, ->a0
define i32 @divuw(i32 %a, i32 %b) {
  %div = udiv i32 %a, %b
  ret i32 %div
}

; CHECK: remuw:
; CHECK: remuw a0, a1, ->a0
define i32 @remuw(i32 %a, i32 %b) {
  %rem = urem i32 %a, %b
  ret i32 %rem
}

; COMM: TODO: add assertzext optimize patterns
