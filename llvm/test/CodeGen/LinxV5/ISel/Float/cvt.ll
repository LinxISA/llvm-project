; RUN: llc < %s --march=linx64v5 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK,DAG

; CHECK-LABEL: u64tof64:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  ucvtf.ud2fd a1, ->t
; DAG-NEXT: fadd.fd a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define double @u64tof64(double %a, i64 %b) {
  %cvt = uitofp i64 %b to double
  %add = fadd double %a, %cvt
  ret double %add
}

; CHECK-LABEL: s64tof32:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  scvtf.sd2fs a1, ->t
; DAG-NEXT: fadd.fs a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define float @s64tof32(float %a, i64 %b) {
  %cvt = sitofp i64 %b to float
  %add = fadd float %a, %cvt
  ret float %add
}

; CHECK-LABEL: s32tof32:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  scvtf.sw2fs a1, ->t
; DAG-NEXT: fadd.fs a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define float @s32tof32(float %a, i32 %b) {
  %cvt = sitofp i32 %b to float
  %add = fadd float %a, %cvt
  ret float %add
}

; CHECK-LABEL: s16tof16:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  scvtf.sh2fh a1, ->t
; DAG-NEXT: fadd.fh a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define half @s16tof16(half %a, i16 %b) {
  %cvt = sitofp i16 %b to half
  %add = fadd half %a, %cvt
  ret half %add
}

; CHECK-LABEL: u16tof32:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  ucvtf.uh2fs a1, ->t
; DAG-NEXT: fadd.fs a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define float @u16tof32(float %a, i16 %b) {
  %cvt = uitofp i16 %b to float
  %add = fadd float %a, %cvt
  ret float %add
}

; CHECK-LABEL: s32tof16:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  scvtf.sw2fh a1, ->t
; DAG-NEXT: fadd.fh a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define half @s32tof16(half %a, i32 %b) {
  %cvt = sitofp i32 %b to half
  %add = fadd half %a, %cvt
  ret half %add
}

; CHECK-LABEL: f32tos64:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  fcvtz.fs2sd a0, ->t
; DAG-NEXT: add t#1, a1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define i64 @f32tos64(float %a, i64 %b) {
  %cvt = fptosi float %a to i64
  %add = add i64 %cvt, %b
  ret i64 %add
}

; CHECK-LABEL: f16tou32:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  fcvtz.fh2ud a0, ->t
; DAG-NEXT: addw t#1, a1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define i32 @f16tou32(half %a, i32 %b) {
  %cvt = fptoui half %a to i32
  %add = add i32 %cvt, %b
  ret i32 %add
}

; CHECK-LABEL: f32tof64:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  fcvt.fs2fd a1, ->t
; DAG-NEXT: fadd.fd a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define double @f32tof64(double %a, float %b) {
  %cvt = fpext float %b to double
  %add = fadd double %a, %cvt
  ret double %add
}

; CHECK-LABEL: f64tof32:
; DAG:      C.BSTART.FP RET
; DAG-DAG:  fcvt.fd2fs a1, ->t
; DAG-NEXT: fadd.fs a0, t#1, ->a0
; DAG-DAG:  c.setc.tgt ra
; DAG-NEXT: C.BSTART.FP
define float @f64tof32(float %a, double %b) {
  %cvt = fptrunc double %b to float
  %add = fadd float %a, %cvt
  ret float %add
}

; CHECK-LABEL: f64toi32tof64:
; DAG:      C.BSTART.FP RET
; DAG-NEXT: fcvtz.fd2sd a0, ->t
; DAG-NEXT: scvtf.sw2fd t#1, ->t
; DAG-NEXT: fadd.fd t#1, a1, ->a0
define double @f64toi32tof64(double %a, double %b) {
  %cvt1 = fptosi double %a to i32
  %cvt2 = sitofp i32 %cvt1 to double
  %add = fadd double %cvt2, %b
  ret double %add
}
