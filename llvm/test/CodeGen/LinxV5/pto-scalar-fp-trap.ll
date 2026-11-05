; RUN: llc -mtriple=linx64v5 -mcpu=janus < %s | FileCheck %s
; RUN: llc -mtriple=linx64v5 -mcpu=janus -filetype=obj < %s -o %t.o
; RUN: llvm-objdump -dr %t.o | FileCheck %s --check-prefix=OBJ

target triple = "linx64v5"
declare void @llvm.trap()
declare void @llvm.debugtrap()
declare double @llvm.sqrt.f64(double)
declare double @llvm.exp.f64(double)
declare half @llvm.fma.f16(half, half, half)
declare double @llvm.linx.scalar.fsqrt.f64(double)
declare float @llvm.linx.scalar.fsqrt.f32(float)
declare double @llvm.linx.scalar.fexp.f64(double)
declare float @llvm.linx.scalar.fexp.f32(float)

; trap and debugtrap fall back to abort, never to a nonblocking BWT.
; CHECK-LABEL: do_trap:
; CHECK-NOT: bwt
; CHECK-NOT: trap zero
; CHECK: CALL, abort
; OBJ-LABEL: <do_trap>:
; OBJ-NOT: bwt
; OBJ: abort
define void @do_trap() {
  call void @llvm.trap()
  unreachable
}
; CHECK-LABEL: do_debugtrap:
; CHECK: CALL, abort
define void @do_debugtrap() {
  call void @llvm.debugtrap()
  ret void
}

; Generic intrinsics retain their library semantics. PTO FEXP's fixed
; 18-term Taylor profile is not the generic exponential contract.
; CHECK-LABEL: generic_sqrt:
; CHECK: CALL, sqrt
define double @generic_sqrt(double %x) {
  %r = call double @llvm.sqrt.f64(double %x)
  ret double %r
}
; CHECK-LABEL: generic_exp:
; CHECK: CALL, exp
define double @generic_exp(double %x) {
  %r = call double @llvm.exp.f64(double %x)
  ret double %r
}

; CHECK-LABEL: sqrt64:
; CHECK: C.BSTART.FP
; CHECK: fsqrt.fd a0, ->a0
; OBJ-LABEL: <sqrt64>:
; OBJ: fsqrt.fd
define double @sqrt64(double %x) {
  %r = call double @llvm.linx.scalar.fsqrt.f64(double %x)
  ret double %r
}

; CHECK-LABEL: sqrt32:
; CHECK: C.BSTART.FP
; CHECK: fsqrt.fs a0, ->a0
; OBJ-LABEL: <sqrt32>:
; OBJ: fsqrt.fs
define float @sqrt32(float %x) {
  %r = call float @llvm.linx.scalar.fsqrt.f32(float %x)
  ret float %r
}

; CHECK-LABEL: exp64:
; CHECK: C.BSTART.FP
; CHECK: fexp.fd a0, ->a0
; OBJ-LABEL: <exp64>:
; OBJ: fexp.fd
define double @exp64(double %x) {
  %r = call double @llvm.linx.scalar.fexp.f64(double %x)
  ret double %r
}

; CHECK-LABEL: exp32:
; CHECK: C.BSTART.FP
; CHECK: fexp.fs a0, ->a0
; OBJ-LABEL: <exp32>:
; OBJ: fexp.fs
define float @exp32(float %x) {
  %r = call float @llvm.linx.scalar.fexp.f32(float %x)
  ret float %r
}

; An unused profile result must still execute and update sticky FP flags.
; CHECK-LABEL: unused_profile:
; CHECK: fsqrt.fd
; CHECK: fexp.fd
define void @unused_profile(double %x) {
  %a = call double @llvm.linx.scalar.fsqrt.f64(double %x)
  %b = call double @llvm.linx.scalar.fexp.f64(double %x)
  ret void
}

; Preserve half carriers at the ABI and round every arithmetic result back
; to half, using assigned FD arithmetic and conversion opcodes.
; CHECK-LABEL: half_fadd:
; CHECK: fcvt.fh2fd
; CHECK: fcvt.fh2fd
; CHECK: fadd.fd
; CHECK: fcvt.fd2fh
; OBJ-LABEL: <half_fadd>:
; OBJ-NOT: fadd.fh
; OBJ: fadd.fd
; OBJ: fcvt.fd2fh
define half @half_fadd(half %a, half %b) {
  %r = fadd half %a, %b
  ret half %r
}

; Preserve half carriers at the ABI and round every arithmetic result back
; to half, using assigned FD arithmetic and conversion opcodes.
; CHECK-LABEL: half_fsub:
; CHECK: fcvt.fh2fd
; CHECK: fcvt.fh2fd
; CHECK: fsub.fd
; CHECK: fcvt.fd2fh
; OBJ-LABEL: <half_fsub>:
; OBJ-NOT: fsub.fh
; OBJ: fsub.fd
; OBJ: fcvt.fd2fh
define half @half_fsub(half %a, half %b) {
  %r = fsub half %a, %b
  ret half %r
}

; Preserve half carriers at the ABI and round every arithmetic result back
; to half, using assigned FD arithmetic and conversion opcodes.
; CHECK-LABEL: half_fmul:
; CHECK: fcvt.fh2fd
; CHECK: fcvt.fh2fd
; CHECK: fmul.fd
; CHECK: fcvt.fd2fh
; OBJ-LABEL: <half_fmul>:
; OBJ-NOT: fmul.fh
; OBJ: fmul.fd
; OBJ: fcvt.fd2fh
define half @half_fmul(half %a, half %b) {
  %r = fmul half %a, %b
  ret half %r
}

; Preserve half carriers at the ABI and round every arithmetic result back
; to half, using assigned FD arithmetic and conversion opcodes.
; CHECK-LABEL: half_fdiv:
; CHECK: fcvt.fh2fd
; CHECK: fcvt.fh2fd
; CHECK: fdiv.fd
; CHECK: fcvt.fd2fh
; OBJ-LABEL: <half_fdiv>:
; OBJ-NOT: fdiv.fh
; OBJ: fdiv.fd
; OBJ: fcvt.fd2fh
define half @half_fdiv(half %a, half %b) {
  %r = fdiv half %a, %b
  ret half %r
}

; CHECK-LABEL: half_fma:
; CHECK: fmadd.fd
; CHECK: fcvt.fd2fh
define half @half_fma(half %a, half %b, half %c) {
  %r = call half @llvm.fma.f16(half %a, half %b, half %c)
  ret half %r
}
; CHECK-LABEL: half_compare:
; CHECK-NOT: flt.fh
; CHECK: flt.fs
define i1 @half_compare(half %a, half %b) {
  %r = fcmp olt half %a, %b
  ret i1 %r
}
; CHECK-LABEL: half_memory:
; CHECK: lh [
; CHECK: sh
define void @half_memory(ptr %src, ptr %dst) {
  %v = load volatile half, ptr %src, align 2
  store volatile half %v, ptr %dst, align 2
  ret void
}
; CHECK-LABEL: half_round_chain:
; CHECK: fadd.fd
; CHECK: fcvt.fd2fh
; CHECK: fcvt.fh2fd
; CHECK: fmul.fd
; CHECK: fcvt.fd2fh
define half @half_round_chain(half %a, half %b, half %c) {
  %x = fadd half %a, %b
  %r = fmul half %x, %c
  ret half %r
}

; Negate the low half sign bit without arithmetic or stack round trips.
; CHECK-LABEL: half_negate:
; CHECK-NOT: subi sp
; CHECK-NOT: fsub
; CHECK: lui 8,
; CHECK: xor
; CHECK-NOT: shi
; CHECK: c.setc.tgt
define half @half_negate(half %x) {
  %r = fneg half %x
  ret half %r
}
