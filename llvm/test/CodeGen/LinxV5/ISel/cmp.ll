; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; CHECK: cmp_eq:
; CHECK: cmp.eq a0, a1, ->a0
define i1 @cmp_eq(i64 %a, i64 %b) {
  %eq = icmp eq i64 %a, %b
  ret i1 %eq
}

; CHECK: cmp_ne_sw:
; DAG: cmp.ne a0, a1.sw, ->a0
define i1 @cmp_ne_sw(i64 %a, i32 %b) {
  %ext = sext i32 %b to i64
  %ne = icmp ne i64 %a, %ext
  ret i1 %ne
}

; CHECK: cmp_lti:
; CHECK: cmp.lti a0, 1024, ->a0
define i1 @cmp_lti(i64 %a) {
  %lt = icmp slt i64 %a, 1024
  ret i1 %lt
}

; CHECK: cmp_ltioverflow:
; CHECK: cmp.lt a0, t#1, ->a0
define i1 @cmp_ltioverflow(i64 %a) {
  %lt = icmp slt i64 %a, 2048
  ret i1 %lt
}

; COMM: TODO: optimize to cmp.geui a0, 4095
; CHECK: cmp_geui:
; CHECK: addi zero, 4094, ->t
; CHECK: cmp.ltu t#1, a0, ->a0
define i1 @cmp_geui(i64 %a) {
  %ge = icmp uge i64 %a, 4095
  ret i1 %ge
}

; CHECK: cmp_gt:
; CHECK: cmp.lt a1, a0, ->a0
define i1 @cmp_gt(i64 %a, i64 %b) {
  %gt = icmp sgt i64 %a, %b
  ret i1 %gt
}

; CHECK: cmp_le_uw:
; DAG: cmp.ge a1, a0.uw, ->a0
define i1 @cmp_le_uw(i32 %a, i64 %b) {
  %ext = zext i32 %a to i64
  %le = icmp sle i64 %ext, %b
  ret i1 %le
}
