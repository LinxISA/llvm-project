; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: foeq.fd:
; DAG: feq.fd a0, a1, ->a0
; VBX:      feq.fd a0, a1, ->t
; VBX-NEXT: c.movi 1, ->t
; VBX-NEXT: sub t#1, t#2, ->t
; VBX-NEXT: cmp.eqi t#1, 0, ->a0
define i1 @foeq.fd(double %a, double %b) {
  %cmp = fcmp oeq double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fueq.fh:
; DAG:      fne.fh a0, a1, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX: __gnu_h2f_ieee
define i1 @fueq.fh(half %a, half %b) {
  %cmp = fcmp ueq half %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fune.fs:
; DAG:      feq.fs a0, a1, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX:      feq.fs a0, a1, ->t
; VBX-NEXT: c.movi 1, ->t
; VBX-NEXT: sub t#1, t#2, ->t
; VBX-NEXT: cmp.nei t#1, 0, ->a0
define i1 @fune.fs(float %a, float %b) {
  %cmp = fcmp une float %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fone.fs:
; DAG: fne.fs a0, a1, ->a0
; VBX: __unordsf2
define i1 @fone.fs(float %a, float %b) {
  %cmp = fcmp one float %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: folt.fd:
; DAG: flt.fd a0, a1, ->a0
; VBX:      flt.fd a0, a1, ->t
; VBX-NEXT: sub zero, t#1, ->t
; VBX-NEXT: cmp.lti t#1, 0, ->a0
define i1 @folt.fd(double %a, double %b) {
  %cmp = fcmp olt double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fult.fd:
; DAG:      fge.fd a0, a1, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX:      fge.fd a0, a1, ->t
; VBX-NEXT: subi t#1, 1, ->t
; VBX-NEXT: cmp.lti t#1, 0, ->a0
define i1 @fult.fd(double %a, double %b) {
  %cmp = fcmp ult double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fole.fd:
; DAG: fge.fd a1, a0, ->a0
; VBX:      fge.fd a1, a0, ->t
; VBX-NEXT: c.movi 1, ->t
; VBX-NEXT: sub t#1, t#2, ->t
; VBX-NEXT: cmp.lti t#1, 1, ->a0
define i1 @fole.fd(double %a, double %b) {
  %cmp = fcmp ole double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fule.fd:
; DAG:      flt.fd a1, a0, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX:      flt.fd a1, a0, ->t
; VBX-NEXT: cmp.lti t#1, 1, ->a0
define i1 @fule.fd(double %a, double %b) {
  %cmp = fcmp ule double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: foge.fd:
; DAG: fge.fd a0, a1, ->a0
; VBX:      fge.fd a0, a1, ->t
; VBX-NEXT: subi t#1, 1, ->t
; VBX-NEXT: c.movi -1, ->t
; VBX-NEXT: cmp.lt t#1, t#2, ->a0
define i1 @foge.fd(double %a, double %b) {
  %cmp = fcmp oge double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fuge.fd:
; DAG:      flt.fd a0, a1, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX:      flt.fd a0, a1, ->t
; VBX-NEXT: sub zero, t#1, ->t
; VBX-NEXT: c.movi -1, ->t
; VBX-NEXT: cmp.lt t#1, t#2, ->a0
define i1 @fuge.fd(double %a, double %b) {
  %cmp = fcmp uge double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fogt.fd:
; DAG: flt.fd a1, a0, ->a0
; VBX:      flt.fd a1, a0, ->t
; VBX-NEXT: cmp.lt zero, t#1, ->a0
define i1 @fogt.fd(double %a, double %b) {
  %cmp = fcmp ogt double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: fugt.fd:
; DAG:      fge.fd a1, a0, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX:      fge.fd a1, a0, ->t
; VBX-NEXT: c.movi 1, ->t
; VBX-NEXT: sub t#1, t#2, ->t
; VBX-NEXT: cmp.lt zero, t#1, ->a0
define i1 @fugt.fd(double %a, double %b) {
  %cmp = fcmp ugt double %a, %b
  ret i1 %cmp
}

; CHECK-LABEL: ford.fd:
; DAG:      feq.fd a0, a0, ->t
; DAG-NEXT: feq.fd a1, a1, ->t
; DAG-NEXT: and t#2, t#1, ->a0
; VBX: __unorddf2
define i1 @ford.fd(double %a, double %b) {
  %cmp = fcmp ord double %a, %b
  ret i1 %cmp
}

; CHECK-LABLE: funo.fd:
; DAG:      feq.fd a0, a0, ->t
; DAG-NEXT: feq.fd a1, a1, ->t
; DAG-NEXT: and t#2, t#1, ->t
; DAG-NEXT: xori t#1, 1, ->a0
; VBX: __unorddf2
define i1 @funo.fd(double %a, double %b) {
  %cmp = fcmp uno double %a, %b
  ret i1 %cmp
}