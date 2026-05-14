; RUN: llc < %s --march=linx64 -O2 | FileCheck %s --dump-input always -vv --check-prefixes=CHECK

declare fp128 @llvm.copysign.f128(fp128, fp128)
declare float @llvm.copysign.f32(float %a, float %b)
declare double @llvm.copysign.f64(double %a, double %b)
declare half @llvm.copysign.f16(half %a, half %b)

; CHECK-LABEL: copysign32:
define float @copysign32(float %a, float %b) {
  %c = call float @llvm.copysign.f32(float %a, float %b)
  ret float %c
}