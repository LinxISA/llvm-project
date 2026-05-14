; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; DAG: ld:
; DAG: ld [a0, zero], ->a0
define i64 @ld(ptr %p) {
  %load = load i64, ptr %p
  ret i64 %load
}

; CHECK: ldrr:
; CHECK: lw [a0, a1], ->a0
define i32 @ldrr(ptr %p, i64 %i) {
  %addr = getelementptr inbounds i8, ptr %p, i64 %i
  %load = load i32, ptr %addr
  ret i32 %load
}

; CHECK: ldrx
; CHECK: lh [a0, a1.sw], ->a0
define i64 @ldrx(ptr %p, i32 %i) {
  %ext = sext i32 %i to i64
  %addr = getelementptr inbounds i8, ptr %p, i64 %ext
  %load = load i16, ptr %addr
  %rext = sext i16 %load to i64
  ret i64 %rext
}

; CHECK: lburxs
; CHECK: lbu [a0, a1.uw<<3], ->a0
define i64 @lburxs(ptr %p, i32 %i) {
  %ext = zext i32 %i to i64
  %addr = getelementptr inbounds i64, ptr %p, i64 %ext
  %load = load i8, ptr %addr
  %rext = zext i8 %load to i64
  ret i64 %rext
}

; CHECK: ldriscale:
; CHECK: ldi [a0, 64], ->a0
define i64 @ldriscale(ptr %p) {
  %addr = getelementptr inbounds i64, ptr %p, i64 8
  %load = load i64, ptr %addr
  ret i64 %load
}

; CHECK: lwriunscale:
; CHECK: lwi.u [a0, 7], ->a0
define i32 @lwriunscale(ptr %p) {
  %addr = getelementptr inbounds i8, ptr %p, i64 7
  %load = load i32, ptr %addr
  ret i32 %load
}

; CHECK: lbi:
; CHECK: lbi [a0, 2047], ->a0
define i8 @lbi(ptr %p) {
  %addr = getelementptr inbounds i8, ptr %p, i64 2047
  %load = load i8, ptr %addr
  ret i8 %load
}