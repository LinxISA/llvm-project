; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; DAG: sd:
; DAG: sd a1, [a0, zero<<3]
; VBX: sd:
; VBX: sdi a1, [a0, 0]
define void @sd(ptr %p, i64 %a) {
  store i64 %a, ptr %p
  ret void
}

; CHECK: swrr:
; CHECK: sw a2, [a0, a1<<2]
define void @swrr(ptr %p, i64 %i, i32 %a) {
  %addr = getelementptr inbounds i32, ptr %p, i64 %i
  store i32 %a, ptr %addr
  ret void
}

; CHECK: shrx:
; CHECK: sh a2, [a0, a1.sw<<1]
define void @shrx(ptr %p, i32 %i, i16 %a) {
  %ext = sext i32 %i to i64
  %addr = getelementptr inbounds i16, ptr %p, i64 %ext
  store i16 %a, ptr %addr
  ret void
}

; CHECK: sdunscale:
; CHECK: sd.u a2, [a0, a1]
define void @sdunscale(ptr %p, i64 %i, i64 %a) {
  %addr = getelementptr inbounds i8, ptr %p, i64 %i
  store i64 %a, ptr %addr
  ret void
}

; CHECK: sbi:
; CHECK: sbi a1, [a0, 99]
define void @sbi(ptr %p, i8 %a) {
  %addr = getelementptr inbounds i8, ptr %p, i64 99
  store i8 %a, ptr %addr
  ret void
}

; CHECK: swiscale:
; CHECK: swi a1, [a0, 4]
define void @swiscale(ptr %p, i32 %a) {
  %addr = getelementptr inbounds i32, ptr %p, i64 1
  store i32 %a, ptr %addr
  ret void
}

; CHECK: shiunscale:
; CHECK: shi.u a1, [a0, -2047]
define void @shiunscale(ptr %p, i16 %a) {
  %addr = getelementptr inbounds i8, ptr %p, i64 -2047
  store i16 %a, ptr %addr
  ret void
}