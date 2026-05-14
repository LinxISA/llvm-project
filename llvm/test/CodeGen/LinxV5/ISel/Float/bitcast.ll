; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: i64bcf64:
; CHECK:      C.BSTART.FP RET
; CHECK-DAG:  fadd.fd a0, a1, ->a0
; CHECK-DAG:  c.setc.tgt ra
; CHECK-NEXT: C.BSTART.FP
define double @i64bcf64(i64 %a, double %b) {
  %conv = bitcast i64 %a to double
  %add = fadd double %conv, %b
  ret double %add
}

; CHECK-LABEL: i32bcf32:
; CHECK:      C.BSTART.FP RET
; CHECK-DAG:  fadd.fs a0, a1, ->a0
; CHECK-DAG:  c.setc.tgt ra
; CHECK-NEXT: C.BSTART.FP
define float @i32bcf32(i32 %a, float %b) {
  %conv = bitcast i32 %a to float
  %add = fadd float %conv, %b
  ret float %add
}

; CHECK-LABEL: i16bcf16:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  fadd.fh a0, a1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define half @i16bcf16(i16 %a, half %b) {
  %conv = bitcast i16 %a to half
  %add = fadd half %conv, %b
  ret half %add
}

; CHECK-LABEL: f64bci64:
; CHECK:      C.BSTART.STD RET
; CHECK-DAG:  add a0, a1, ->a0
; CHECK-DAG:  c.setc.tgt ra
; CHECK-NEXT: C.BSTART.STD
define double @f64bci64(double %a, i64 %b) {
  %conv = bitcast double %a to i64
  %add = add i64 %conv, %b
  %conv2 = bitcast i64 %add to double
  ret double %conv2
}

; CHECK-LABEL: f32bci32:
; CHECK:      C.BSTART.STD RET
; CHECK-DAG:  addw a0, a1, ->a0
; CHECK-DAG:  c.setc.tgt ra
; CHECK-NEXT: C.BSTART.STD
define float @f32bci32(float %a, i32 %b) {
  %conv = bitcast float %a to i32
  %add = add i32 %conv, %b
  %conv2 = bitcast i32 %add to float
  ret float %conv2
}

; CHECK-LABEL: f16bci16:
; DAG:      C.BSTART
; DAG-DAG:  fadd.fh a0, a1, ->t
; DAG-NEXT: add t#1, a2, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define half @f16bci16(half %a, i16 %b, i16 %c) {
  %conv = bitcast i16 %b to half
  %add = fadd half %a, %conv
  %conv2 = bitcast half %add to i16
  %add2 = add i16 %conv2, %c
  %conv3 = bitcast i16 %add2 to half
  ret half %conv3
}
