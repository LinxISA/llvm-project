; RUN: llc < %s -linxv5-enable-compress-inst=false --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=DAG,CHECK

; CHECK: i8sexti16:
; DAG: bxs a1, 0, 8, ->t
define i16 @i8sexti16(i16 %a, i8 %b) {
  %ext = sext i8 %b to i16
  %add = add i16 %a, %ext
  ret i16 %add
}

; CHECK: i16zexti32:
; DAG:      bic a1, 16, 48, ->t
; DAG-NEXT: addw a0, t#1, ->a0
define i32 @i16zexti32(i32 %a, i16 %b) {
  %ext = zext i16 %b to i32
  %add = add i32 %a, %ext
  ret i32 %add
}

; CHECK: i32sexti64:
; DAG: or zero, a1.sw, ->t
define i64 @i32sexti64(i64 %a, i32 %b) {
  %ext = sext i32 %b to i64
  %mul = mul i64 %a, %ext
  ret i64 %mul
}

; CHECK: i32zexti64:
; DAG: bic a1, 32, 32, ->t
define i64 @i32zexti64(i64 %a, i32 %b) {
  %ext = zext i32 %b to i64
  %mul = mul i64 %a, %ext
  ret i64 %mul
}
