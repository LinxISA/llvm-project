; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; COMM: TODO: optimize to setc.ne a0, zero
; CHECK: setc:
; DAG: setc.andi a0, 1
define void @setc(i64 %a) {
  %xor = xor i64 %a, -1
  %trunc = trunc i64 %xor to i1
  br i1 %trunc, label %true, label %false
true:
  ret void
false:
  ret void
}

; CHECK: setc_eq:
; CHECK: setc.eq a0, a1
define void @setc_eq(i64 %a, i64 %b) {
  %ne = icmp ne i64 %a, %b
  br i1 %ne, label %true, label %false
true:
  ret void
false:
  ret void
}

; DAG: setc_ne_sw:
; DAG: setc.ne a0, a1.sw
define void @setc_ne_sw(i64 %a, i32 %b) {
  %ext = sext i32 %b to i64
  %eq = icmp eq i64 %a, %ext
  br i1 %eq, label %true, label %false
true:
  ret void
false:
  ret void
}

; CHECK: setc_lti:
; CHECK: setc.lti a0, 1024
define void @setc_lti(i64 %a) {
  %cmp = icmp sge i64 %a, 1024
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}

; CHECK: setc_ltioverflow:
; CHECK: setc.lt a0, t#1
define void @setc_ltioverflow(i64 %a) {
  %cmp = icmp sge i64 %a, 2049
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}

; COMM: TODO: optimize to setc.geui a0, 4095
; CHECK: setc_geui:
; CHECK: addi zero, 4094, ->t
; CHECK: setc.ltu t#1, a0
define void @setc_geui(i64 %a) {
  %cmp = icmp ult i64 %a, 4095
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}

; CHECK: setc_gt:
; CHECK: setc.lt a1, a0
define void @setc_gt(i64 %a, i64 %b) {
  %cmp = icmp sle i64 %a, %b
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}

; DAG: setc_le_uw:
; DAG: setc.ge a1, a0.uw
define void @setc_le_uw(i32 %a, i64 %b) {
  %ext = zext i32 %a to i64
  %cmp = icmp sgt i64 %ext, %b
  br i1 %cmp, label %true, label %false
true:
  ret void
false:
  ret void
}

@addr = global i8* null

; CHECK: brind:
; CHECK: setc.tgt t#1
define void @brind(i64 %a) {
  store volatile i8* blockaddress(@brind, %block), i8** @addr
  %val = load volatile i8*, i8** @addr
  indirectbr i8* %val, [label %block]
block:
  ret void
}