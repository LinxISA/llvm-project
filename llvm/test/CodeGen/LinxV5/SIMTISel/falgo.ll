; RUN: llc < %s --march=linx64 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

; ASM-LABEL: f64sqrt:
; ASM: l.fsqrt ri1.fd, ->t.d
define void @f64sqrt(ptr %p, double %a) #1 {
entry:
  %result = call double @llvm.sqrt.f64(double %a)
  store double %result, ptr %p
  ret void
}

; ASM-LABEL: f32exp:
; ASM: l.fexp ri1.fs, ->t.w
define void @f32exp(ptr %p, float %a) #1 {
entry:
  %result = call float @llvm.exp.f32(float %a)
  store float %result, ptr %p
  ret void
}

; TODO: Support exp2/log/log10 patterns. Now i don't know
; How to handle the floating precision problems. These
; function call in simt will crash now.

declare double @llvm.sqrt.f64(double)
declare float  @llvm.sqrt.f32(float)
declare half   @llvm.sqrt.f16(half)
declare double @llvm.exp.f64(double)
declare float  @llvm.exp.f32(float)
declare half   @llvm.exp.f16(half)
declare double @llvm.exp2.f64(double)
declare float  @llvm.exp2.f32(float)
declare half   @llvm.exp2.f16(half)
declare double @llvm.log.f64(double)
declare float  @llvm.log.f32(float)
declare half   @llvm.log.f16(half)
declare double @llvm.log2.f64(double)
declare float  @llvm.log2.f32(float)
declare half   @llvm.log2.f16(half)
declare double @llvm.log10.f64(double)
declare float  @llvm.log10.f32(float)
declare half   @llvm.log10.f16(half)
declare double @llvm.sin.f64(double)
declare float  @llvm.sin.f32(float)
declare half   @llvm.sin.f16(half)
declare double @llvm.cos.f64(double)
declare float  @llvm.cos.f32(float)
declare half   @llvm.cos.f16(half)

attributes #1 = { noinline "__vec__" }