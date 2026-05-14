; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

; ASM-LABEL: u64tof64:
; ASM: l.icvtf.u642fp64 ri1.ud,  ->t.d
define void @u64tof64(ptr %p, i64 %a) #1 {
  %f64 = uitofp i64 %a to double
  store double %f64, ptr %p
  ret void
}

; ASM-LABEL: s32tof64:
; ASM: l.icvtf.s322fp64 ri1.sw,  ->t.d
define void @s32tof64(ptr %p, i32 %a) #1 {
  %f64 = sitofp i32 %a to double
  store double %f64, ptr %p
  ret void
}

; ASM-LABEL: s64tof16:
; ASM: l.icvtf.s642fp16 ri1.sd,  ->t.h
define void @s64tof16(ptr %p, i64 %a) #1 {
  %f16 = sitofp i64 %a to half
  store half %f16, ptr %p
  ret void
}

; ASM-LABEL: f32tou8:
; ASM: l.fcvti.fp322u8 ri1.fs,  ->t.b
define void @f32tou8(ptr %p, float %a) #1 {
  %u8 = fptoui float %a to i8
  store i8 %u8, ptr %p
  ret void
}

; ASM-LABEL: f16tos64:
; ASM: l.fcvti.fp162s64 ri1.fh,  ->t.d
define void @f16tos64(ptr %p, half %a) #1 {
  %s64 = fptosi half %a to i64
  store i64 %s64, ptr %p
  ret void
}

attributes #1 = {"__vec__"}