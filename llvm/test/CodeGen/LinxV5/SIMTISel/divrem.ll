; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

; ASM-LABEL: sdiv64:
; ASM: l.div ri1.sd, ri2.sd, ->t.d
define void @sdiv64(ptr %p, i64 %a, i64 %b) #1 {
  %div = sdiv i64 %a, %b
  store i64 %div, ptr %p
  ret void
}

; ASM-LABEL: udiv32:
; ASM: l.div ri1.uw, ri2.uw, ->t.w
define void @udiv32(ptr %p, i32 %a, i32 %b) #1 {
  %div = udiv i32 %a, %b
  store i32 %div, ptr %p
  ret void
}

; ASM-LABEL: srem16:
; ASM: l.rem ri1.sh, ri2.sh, ->t.h
define void @srem16(ptr %p, i16 %a, i16 %b) #1 {
  %rem = srem i16 %a, %b
  store i16 %rem, ptr %p
  ret void
}

; ASM-LABEL: urem8:
; ASM: l.rem ri1.ub, ri2.ub, ->t.b
define void @urem8(ptr %p, i8 %a, i8 %b) #1 {
  %rem = urem i8 %a, %b
  store i8 %rem, ptr %p
  ret void
}

attributes #1 = {"__vec__"}