; RUN: llc < %s --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=simt
target triple = "linx64-unknown-linux-musl"

; simt: name: fsqrt64
; simt: SIMT_FSQRT_SCAR 0, [[REG1:%[0-9]+]], 0
define void @fsqrt64(double %a, ptr %p) #0 #1 {
entry:
  %result = call double @llvm.sqrt.f64(double %a)
  store double %result, ptr %p
  ret void
}

; simt: name: fsqrt32
; simt: SIMT_FSQRT_SCAR 1, [[REG1:%[0-9]+]], 1
define void @fsqrt32(float %a, ptr %p) #0 #1 {
entry:
  %result = call float @llvm.sqrt.f32(float %a)
  store float %result, ptr %p
  ret void
}

; simt: name: fsqrt16
; simt: SIMT_FSQRT_SCAR 2, [[REG1:%[0-9]+]], 2
define void @fsqrt16(half %a, ptr %p) #0 #1 {
entry:
  %result = call half @llvm.sqrt.f16(half %a)
  store half %result, ptr %p
  ret void
}

declare double @llvm.sqrt.f64(double)
declare float @llvm.sqrt.f32(float)
declare half @llvm.sqrt.f16(half)

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }