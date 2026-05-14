; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv

; CHECK: srl:
; CHECK: srl a0, a1, ->a0
define i64 @srl(i64 %a, i64 %b) {
  %srl = lshr i64 %a, %b
  ret i64 %srl
}

; CHECK: srai:
; CHECK: srai a0, 44, ->a0
define i64 @srai(i64 %a) {
  %sra = ashr i64 %a, 44
  ret i64 %sra
}

; CHECK: sllw:
; CHECK: sllw a0, a1, ->a0
define i32 @sllw(i32 %a, i32 %b) {
  %sll = shl i32 %a, %b
  ret i32 %sll
}
