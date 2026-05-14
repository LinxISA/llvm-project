; RUN: llc < %s -march=linx64 -O2 | FileCheck %s --dump-input always -vv

define i64 @f1(i64 %a) {
; CHECK-LABEL: f1:
; CHECK: sdi a0, [sp, 248]
; CHECK-NEXT: ldi [sp, 248], ->a0
  %v = alloca i64
  store volatile i64 %a, ptr %v
  %r = load volatile i64, ptr %v
  ret i64 %r
}

define i32 @f2(i32 %a) {
; CHECK-LABEL: f2:
; CHECK: swi a0, [sp, 248]
; CHECK-NEXT: lwi [sp, 248], ->a0
  %v = alloca i32
  store volatile i32 %a, ptr %v
  %r = load volatile i32, ptr %v
  ret i32 %r
}

define i16 @f3(i16 %a) {
; CHECK-LABEL: f3:
; CHECK: shi a0, [sp, 248]
; CHECK-NEXT: lhi [sp, 248], ->a0
  %v = alloca i16
  store volatile i16 %a, ptr %v
  %r = load volatile i16, ptr %v
  ret i16 %r
}

define i8 @f4(i8 %a) {
; CHECK-LABEL: f4:
; CHECK: sbi a0, [sp, 248]
; CHECK-NEXT: lbi [sp, 248], ->a0
  %v = alloca i8
  store volatile i8 %a, ptr %v
  %r = load volatile i8, ptr %v
  ret i8 %r
}

define i64 @f5(i64 %in, ptr %out) {
; CHECK-LABEL: f5:
; CHECK: sd a0, [sp, t#1<<3]
; CHECK: sd a0, [sp, t#1<<3]
; CHECK: ld [sp, t#1<<3]
; CHECK: ld [sp, t#1<<4]
  %a = alloca i64
  %b = alloca i64
  %v = alloca [4095 x i64]
  store ptr %v, ptr %out
  store volatile i64 %in, ptr %a
  store volatile i64 %in, ptr %b
  %a.0 = load volatile i64, ptr %a
  %b.0 = load volatile i64, ptr %b
  %add = add nsw i64 %b.0, %a.0
  ret i64 %add
}

define i32 @f6(i32 %in, ptr %out) {
; CHECK-LABEL: f6:
; CHECK:      lui 2, ->t
; CHECK-NEXT: addi t#1, 62, ->t
; CHECK-NEXT: sw a0, [sp, t#1<<2]
; CHECK:      lui 2, ->t
; CHECK-NEXT: addi t#1, 60, ->t
; CHECK-NEXT: sw a0, [sp, t#1<<2]
; CHECK:      lui 1, ->t
; CHECK-NEXT: addi t#1, 31, ->t
; CHECK-NEXT: lw [sp, t#1<<3]
; CHECK:      addi zero, 2063, ->t
; CHECK-NEXT: lw [sp, t#1<<4]
  %a = alloca i32
  %b = alloca i32
  %v = alloca [4095 x i64]
  store ptr %v, ptr %out
  store volatile i32 %in, ptr %a
  store volatile i32 %in, ptr %b
  %a.0 = load volatile i32, ptr %a
  %b.0 = load volatile i32, ptr %b
  %add = add nsw i32 %b.0, %a.0
  ret i32 %add
}

define i16 @f7(i16 %in, ptr %out) {
; CHECK-LABEL: f7:
; CHECK:      lui 4, ->t
; CHECK-NEXT: addi t#1, 124, ->t
; CHECK-NEXT: sh a0, [sp, t#1<<1]
; CHECK:      lui 4, ->t
; CHECK-NEXT: addi t#1, 120, ->t
; CHECK-NEXT: sh a0, [sp, t#1<<1]
; CHECK:      lui 1, ->t
; CHECK-NEXT: addi t#1, 31, ->t
; CHECK-NEXT: lh [sp, t#1<<3]
; CHECK:      addi zero, 2063, ->t
; CHECK-NEXT: lh [sp, t#1<<4]
  %a = alloca i16
  %b = alloca i16
  %v = alloca [4095 x i64]
  store ptr %v, ptr %out
  store volatile i16 %in, ptr %a
  store volatile i16 %in, ptr %b
  %a.0 = load volatile i16, ptr %a
  %b.0 = load volatile i16, ptr %b
  %add = add nsw i16 %b.0, %a.0
  ret i16 %add
}

define i8 @f8(i8 %in, ptr %out) {
; CHECK-LABEL: f8:
; CHECK:      lui 8, ->t
; CHECK-NEXT: addi t#1, 248, ->t
; CHECK-NEXT: sb a0, [sp, t#1]
; CHECK:      lui 8, ->t
;; CHECK:     addi t#1, 240, ->t
; CHECK-NEXT: sb a0, [sp, t#1]
; CHECK:      lui 1, ->t
; CHECK-NEXT: addi t#1, 31, ->t
; CHECK-NEXT: lb [sp, t#1<<3]
; CHECK:      addi zero, 2063, ->t
; CHECK-NEXT: lb [sp, t#1<<4]
  %a = alloca i8
  %b = alloca i8
  %v = alloca [4095 x i64]
  store ptr %v, ptr %out
  store volatile i8 %in, ptr %a
  store volatile i8 %in, ptr %b
  %a.0 = load volatile i8, ptr %a
  %b.0 = load volatile i8, ptr %b
  %add = add nsw i8 %b.0, %a.0
  ret i8 %add
}

; COMMENT: TODO: add unscalable test-suites when support unscalable isel

; CHECK-LABEL: storeoff:
; CHECK: sdi a0, [sp, 280]
; CHECK: ldi [sp, 280], ->a0
define i64 @storeoff(i64 %in, ptr %out) {
  %a = alloca [4095 x i64]
  %ptr = getelementptr inbounds i64, ptr %a, i64 3
  store volatile i64 %in, ptr %ptr
  tail call void @bar(ptr %a)
  %load = load volatile i64, ptr %ptr
  ret i64 %load
}

declare void @bar(ptr %p)

define i32 @storelargeif(i32 %in, i64 %sy, ptr %out) {
; CHECK-LABEL: storelargeif:
; CHECK: addi t#1, 35, ->t
; CHECK: lw [sp, t#1<<3], ->t
; CHECK: sdi a1, [sp, 280]
; CHECK: add sp, t#1, ->a0
  %a = alloca [4096 x i32]
  %b = alloca [4096 x i64]
  %ptr = getelementptr inbounds i32, ptr %a, i32 8
  load volatile i32,  ptr %ptr
  %pts = getelementptr inbounds i64, ptr %b, i64 4
  store volatile i64 %sy, ptr %pts
  tail call void @bar(ptr %a)
  tail call void @bar(ptr %b)
  %load = load volatile i32, ptr %ptr
  ret i32 %load
}
