; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: fadd_fd:
; CHECK: fadd.fd a0, a1, ->a0
define double @fadd_fd(double %a, double %b) {
  %add = fadd double %a, %b
  ret double %add
}

; CHECK-LABEL: fadd_fs:
; CHECK: fadd.fs a0, a1, ->a0
define float @fadd_fs(float %a, float %b) {
  %add = fadd float %a, %b
  ret float %add
}

; CHECK-LABEL: fadd_fh:
; DAG: fadd.fh a0, a1, ->a0
define half @fadd_fh(half %a, half %b) {
  %add = fadd half %a, %b
  ret half %add
}

; CHECK-LABEL: fsub_fd:
; CHECK: fsub.fd a0, a1, ->a0
define double @fsub_fd(double %a, double %b) {
  %sub = fsub double %a, %b
  ret double %sub
}

; CHECK-LABLE: fsub_fs:
; CHECK: fsub.fs a0, a1, ->a0
define float @fsub_fs(float %a, float %b) {
  %sub = fsub float %a, %b
  ret float %sub
}

; CHECK-LABEL: fsub_fh:
; DAG: fsub.fh a0, a1, ->a0
define half @fsub_fh(half %a, half %b) {
  %sub = fsub half %a, %b
  ret half %sub
}