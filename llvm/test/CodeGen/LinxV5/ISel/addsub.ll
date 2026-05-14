; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv

; CHECK: addreg:
; CHECK: add a0, a1, ->a0
define i64 @addreg(i64 %a, i64 %b) {
  %add = add i64 %a, %b
  ret i64 %add
}

; CHECK: addimm:
; CHECK: addi a0, 4095, ->a0
define i64 @addimm(i64 %a) {
  %add = add i64 %a, 4095
  ret i64 %add
}

; CHECK: addimmoverflow:
; CHECK: add a0, t#1, ->a0
define i64 @addimmoverflow(i64 %a) {
  %add = add i64 %a, 4096
  ret i64 %add
}

; CHECK: addshift:
; CHECK: add a0, a1<<31, ->a0
define i64 @addshift(i64 %a, i64 %b) {
  %sll = shl i64 %b, 31
  %add = add i64 %a, %sll
  ret i64 %add
}

; CHECK: addzext:
; CHECK: add a0, a1.uw, ->a0
define i64 @addzext(i64 %a, i32 %b) {
  %ext = zext i32 %b to i64
  %add = add i64 %a, %ext
  ret i64 %add
}

; CHECK: addsext:
; CHECK: add a0, a1.sw, ->a0
define i64 @addsext(i64 %a, i32 %b) {
  %ext = sext i32 %b to i64
  %add = add i64 %a, %ext
  ret i64 %add
}

; CHECK: addsextsll:
; CHECK: add a0, a1.sw<<3, ->a0
define i64 @addsextsll(i64 %a, i32 %b) {
  %ext = sext i32 %b to i64
  %sll = shl i64 %ext, 3
  %add = add i64 %a, %sll
  ret i64 %add
}

; CHECK: addsextsllcommutative:
; CHECK: add a1, a0.sw<<3, ->a0
define i64 @addsextsllcommutative(i32 %a, i64 %b) {
  %ext = sext i32 %a to i64
  %sll = shl i64 %ext, 3
  %add = add i64 %sll, %b
  ret i64 %add
}

; CHECK: addw:
; CHECK: addw a0, a1, ->a0
define i32 @addw(i64 %a, i32 %b) {
  %trunc = trunc i64 %a to i32
  %add = add i32 %trunc, %b
  ret i32 %add
}

; CHECK: subzextsll:
; CHECK: sub a0, a1.uw<<7, ->a0
define i64 @subzextsll(i64 %a, i32 %b) {
  %ext = zext i32 %b to i64
  %sll = shl i64 %ext, 7
  %sub = sub i64 %a, %sll
  ret i64 %sub
}

; CHECK: neg:
; CHECK: sub zero, a0, ->a0
define i64 @neg(i64 %a) {
  %neg = sub i64 0, %a
  ret i64 %neg
}

; CHECK-LABEL: addneg:
; CHECK: subi a0, 8, ->a0
define i64 @addneg(i64 %a) {
  %const = bitcast i64 -8 to i64 ; Opaque const to escape from DAGCombine
  %add = add i64 %a, %const
  ret i64 %add
}

; CHECK-LABEL: subneg:
; CHECK: addi a0, 9, ->a0
define i64 @subneg(i64 %a) {
  %const = bitcast i64 -9 to i64 ; Opaque const to escape from DAGCombine
  %sub = sub i64 %a, %const
  ret i64 %sub
}