; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

; CHECK: andi:
; CHECK: bic a0, 5, 59, ->a0
define i64 @andi(i64 %a) {
  %and = and i64 %a, 31
  ret i64 %and
}

; CHECK: zext:
; CHECK: bic a0, 32, 32, ->a0
define i64 @zext(i32 %a) {
  %zext = zext i32 %a to i64
  ret i64 %zext
}

declare double @llvm.fabs.f64(double) nounwind readnone

; CHECK-LABEL: fabs.fd:
; CHECK: bic a0, 63, 1, ->a0
define double @fabs.fd(double %a) {
  %abs = call double @llvm.fabs.f64(double %a)
  ret double %abs
}

; CHECK: andioverflow:
; CHECK: bic a0, 11, 1, ->a0
define i64 @andioverflow(i64 %a) {
  %and = and i64 %a, -2049
  ret i64 %and
}

; CHECK-LABEL: csel.fs:
; COMM: TODO: actually this `andi` is nosense.
; CHECK:      bic a0, 1, 63, ->t
; CHECK-NEXT: csel t#1, a1, a2, ->a0
define float @csel.fs(i1 %p, float %a, float %b) {
  %csel = select i1 %p, float %a, float %b
  ret float %csel
}

;Test boundary conditions: 0000 0000 1FFF FFFF
; CHECK: bictest:
; CHECK: bic a0, 31, 33, ->a0
define i64 @bictest(i64 %a) {
  %and = and i64 %a, 2147483647
  ret i64 %and
}

;Test boundary conditions: FFFF FFFF 8000 0000
; CHECK: bistest:
; CHECK: bis a0, 31, 33, ->a0
define i64 @bistest(i64 %a) {
  %or = or i64 %a, -2147483648
  ret i64 %or
}

;Test the case where no conversion is performed:0xFFFF0000FFFF0000
; CHECK: bictest2:
; CHECK: and a0, t#1, ->a0
define i64 @bictest2(i64 %a) {
  %and = and i64 %a, 281470681808895
  ret i64 %and
}

;Test the case where no conversion is performed:0xFFFF0000FFFF0000
; CHECK: bistest2:
; CHECK: or a0, t#1, ->a0
define i64 @bistest2(i64 %a) {
  %or = or i64 %a, -281470681808896
  ret i64 %or
}
