; RUN: llc < %s --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

target triple = "linx64-unknown-linux-musl"

; MIR: name: zext32_to_64
; MIR: SIMT_ICVT_U322U64_SCAR 0, [[REG1:%[0-9]+]], 1
; ASM: zext32_to_64:
; ASM: l.icvt.u322u64 [[REG1:ri(1[0-1]?|[0-9])]].uw, ->t.d
define void @zext32_to_64(ptr %p, i32 %a) #0 #1 {
entry:
  %ret = zext i32 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: zext16_to_64
; MIR: SIMT_ICVT_U162U64_SCAR 0, [[REG1:%[0-9]+]], 2
; ASM: zext16_to_64:
; ASM: l.icvt.u162u64 [[REG1:ri(1[0-1]?|[0-9])]].uh, ->t.d
define void @zext16_to_64(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = zext i16 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: zext16_to_32
; MIR: SIMT_ICVT_U162U32_SCAR 1, [[REG1:%[0-9]+]], 2
; ASM: zext16_to_32:
; ASM: l.icvt.u162u32 [[REG1:ri(1[0-1]?|[0-9])]].uh, ->t.w
define void @zext16_to_32(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = zext i16 %a to i32
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: zext8_to_64
; MIR: SIMT_ICVT_U82U64_SCAR 0, [[REG1:%[0-9]+]], 3
; ASM: zext8_to_64:
; ASM: l.icvt.u82u64 [[REG1:ri(1[0-1]?|[0-9])]].ub, ->t.d
define void @zext8_to_64(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = zext i8 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: zext8_to_32
; MIR: SIMT_ICVT_U82U32_SCAR 1, [[REG1:%[0-9]+]], 3
; ASM: zext8_to_32:
; ASM: l.icvt.u82u32 [[REG1:ri(1[0-1]?|[0-9])]].ub, ->t.w
define void @zext8_to_32(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = zext i8 %a to i32
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: zext8_to_16
; MIR: SIMT_ICVT_U82U16_SCAR 2, [[REG1:%[0-9]+]], 3
; ASM: zext8_to_16:
; ASM: l.icvt.u82u16 [[REG1:ri(1[0-1]?|[0-9])]].ub, ->t.h
define void @zext8_to_16(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = zext i8 %a to i16
  store i16 %ret, ptr %p
  ret void
}

; MIR: name: sext32_to_64
; MIR: SIMT_ICVT_S322S64_SCAR 0, [[REG1:%[0-9]+]], 5
; ASM: sext32_to_64:
; ASM: l.icvt.s322s64 [[REG1:ri(1[0-1]?|[0-9])]].sw, ->t.d
define void @sext32_to_64(ptr %p, i32 %a) #0 #1 {
entry:
  %ret = sext i32 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: sext16_to_64
; MIR: SIMT_ICVT_S162S64_SCAR 0, [[REG1:%[0-9]+]], 6
; ASM: sext16_to_64:
; ASM: l.icvt.s162s64 [[REG1:ri(1[0-1]?|[0-9])]].sh, ->t.d
define void @sext16_to_64(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = sext i16 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: sext16_to_32
; MIR: SIMT_ICVT_S162S32_SCAR 1, [[REG1:%[0-9]+]], 6
; ASM: sext16_to_32:
; ASM: l.icvt.s162s32 [[REG1:ri(1[0-1]?|[0-9])]].sh, ->t.w
define void @sext16_to_32(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = sext i16 %a to i32
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: sext8_to_64
; MIR: SIMT_ICVT_S82S64_SCAR 0, [[REG1:%[0-9]+]], 7
; ASM: sext8_to_64:
; ASM: l.icvt.s82s64 [[REG1:ri(1[0-1]?|[0-9])]].sb, ->t.d
define void @sext8_to_64(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = sext i8 %a to i64
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: sext8_to_32
; MIR: SIMT_ICVT_S82S32_SCAR 1, [[REG1:%[0-9]+]], 7
; ASM: sext8_to_32:
; ASM: l.icvt.s82s32 [[REG1:ri(1[0-1]?|[0-9])]].sb, ->t.w
define void @sext8_to_32(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = sext i8 %a to i32
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: sext8_to_16
; MIR: SIMT_ICVT_S82S16_SCAR 2, [[REG1:%[0-9]+]], 7
; ASM: sext8_to_16:
; ASM: l.icvt.s82s16 [[REG1:ri(1[0-1]?|[0-9])]].sb, ->t.h
define void @sext8_to_16(ptr %p, i8 %a) #0 #1 {
entry:
  %ret = sext i8 %a to i16
  store i16 %ret, ptr %p
  ret void
}

; MIR: name: trunc64_to_32
; MIR: SIMT_ICVT_S642S32_SCAR 1, [[REG1:%[0-9]+]], 4
; ASM: trunc64_to_32:
; ASM: l.icvt.s642s32 [[REG1:ri(1[0-1]?|[0-9])]].sd, ->[[REG1:a[0-7]+|t+]].w
define void @trunc64_to_32(ptr %p, i64 %a) #0 #1 {
entry:
  %trun = trunc i64 %a to i32
  %ret = add i32 %trun, 1
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: trunc64_to_16
; MIR: SIMT_ICVT_S642S16_SCAR 2, [[REG1:%[0-9]+]], 4
; ASM: trunc64_to_16:
; ASM: l.icvt.s642s16 [[REG1:ri(1[0-1]?|[0-9])]].sd, ->[[REG1:a[0-7]+|t+]].h
define void @trunc64_to_16(ptr %p, i64 %a) #0 #1 {
entry:
  %trun = trunc i64 %a to i16
  %ret = add i16 %trun, 1
  store i16 %ret, ptr %p
  ret void
}

; MIR: name: trunc32_to_16
; MIR: SIMT_ICVT_S322S16_SCAR 2, [[REG1:%[0-9]+]], 5
; ASM: trunc32_to_16:
; ASM: l.icvt.s322s16 [[REG1:ri(1[0-1]?|[0-9])]].sw, ->[[REG1:a[0-7]+|t+]].h
define void @trunc32_to_16(ptr %p, i32 %a) #0 #1 {
entry:
  %trun = trunc i32 %a to i16
  %ret = add i16 %trun, 1
  store i16 %ret, ptr %p
  ret void
}

; MIR: name: trunc64_to_8
; MIR: SIMT_ICVT_S642S8_SCAR 3, [[REG1:%[0-9]+]], 4
; ASM: trunc64_to_8:
; ASM: l.icvt.s642s8 [[REG1:ri(1[0-1]?|[0-9])]].sd, ->[[REG1:a[0-7]+|t+]].b
define void @trunc64_to_8(ptr %p, i64 %a) #0 #1 {
entry:
  %trun = trunc i64 %a to i8
  %ret = add i8 %trun, 1
  store i8 %ret, ptr %p
  ret void
}

; MIR: name: trunc32_to_8
; MIR: SIMT_ICVT_S322S8_SCAR 3, [[REG1:%[0-9]+]], 5
; ASM: trunc32_to_8:
; ASM: l.icvt.s322s8 [[REG1:ri(1[0-1]?|[0-9])]].sw, ->[[REG1:a[0-7]+|t+]].b
define void @trunc32_to_8(ptr %p, i32 %a) #0 #1 {
entry:
  %trun = trunc i32 %a to i8
  %ret = add i8 %trun, 1
  store i8 %ret, ptr %p
  ret void
}

; MIR: name: trunc16_to_8
; MIR: SIMT_ICVT_S162S8_SCAR 3, [[REG1:%[0-9]+]], 6
; ASM: trunc16_to_8:
; ASM: l.icvt.s162s8 [[REG1:ri(1[0-1]?|[0-9])]].sh, ->[[REG1:a[0-7]+|t+]].b
define void @trunc16_to_8(ptr %p, i16 %a) #0 #1 {
entry:
  %trun = trunc i16 %a to i8
  %ret = add i8 %trun, 1
  store i8 %ret, ptr %p
  ret void
}

; MIR: name: uint_to_fp64
; MIR: SIMT_ICVTF_U642FP64_SCAR 0, [[REG1:%[0-9]+]], 0
; ASM: uint_to_fp64:
; ASM: l.icvtf.u642fp64 [[REG1:ri(1[0-1]?|[0-9])]].ud, ->[[REG1:a[0-7]+|t+]].d
define void @uint_to_fp64(ptr %p, i64 %a) #0 #1 {
entry:
  %ret = uitofp i64 %a to double
  store double %ret, ptr %p
  ret void
}

; MIR: name: uint_to_fp32
; MIR: SIMT_ICVTF_U322FP32_SCAR 1, [[REG1:%[0-9]+]], 1
; ASM: uint_to_fp32:
; ASM: l.icvtf.u322fp32 [[REG1:ri(1[0-1]?|[0-9])]].uw, ->[[REG1:a[0-7]+|t+]].w
define void @uint_to_fp32(ptr %p, i32 %a) #0 #1 {
entry:
  %ret = uitofp i32 %a to float
  store float %ret, ptr %p
  ret void
}

; MIR: name: uint_to_fp16
; MIR: SIMT_ICVTF_U162FP16_SCAR 2, [[REG1:%[0-9]+]], 2
; ASM: uint_to_fp16:
; ASM: l.icvtf.u162fp16 [[REG1:ri(1[0-1]?|[0-9])]].uh, ->[[REG1:a[0-7]+|t+]].h
define void @uint_to_fp16(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = uitofp i16 %a to half
  store half %ret, ptr %p
  ret void
}

; MIR: name: sint_to_fp64
; MIR: SIMT_ICVTF_S642FP64_SCAR 0, [[REG1:%[0-9]+]], 4
; ASM: sint_to_fp64:
; ASM: l.icvtf.s642fp64 [[REG1:ri(1[0-1]?|[0-9])]].sd, ->[[REG1:a[0-7]+|t+]].d
define void @sint_to_fp64(ptr %p, i64 %a) #0 #1 {
entry:
  %ret = sitofp i64 %a to double
  store double %ret, ptr %p
  ret void
}

; MIR: name: sint_to_fp32
; MIR: SIMT_ICVTF_S322FP32_SCAR 1, [[REG1:%[0-9]+]], 5
; ASM: sint_to_fp32:
; ASM: l.icvtf.s322fp32 [[REG1:ri(1[0-1]?|[0-9])]].sw, ->[[REG1:a[0-7]+|t+]].w
define void @sint_to_fp32(ptr %p, i32 %a) #0 #1 {
entry:
  %ret = sitofp i32 %a to float
  store float %ret, ptr %p
  ret void
}

; MIR: name: sint_to_fp16
; MIR: SIMT_ICVTF_S162FP16_SCAR 2, [[REG1:%[0-9]+]], 6
; ASM: sint_to_fp16:
; ASM: l.icvtf.s162fp16 [[REG1:ri(1[0-1]?|[0-9])]].sh, ->[[REG1:a[0-7]+|t+]].h
define void @sint_to_fp16(ptr %p, i16 %a) #0 #1 {
entry:
  %ret = sitofp i16 %a to half
  store half %ret, ptr %p
  ret void
}

; ASM: fpext32_to_64:
; ASM: l.fcvt.fp322fp64 [[REG1:ri(1[0-1]?|[0-9])]].fs, ->t.d
define void @fpext32_to_64(ptr %p, float %a) #0 #1 {
entry:
  %ret = fpext float %a to double
  store double %ret, ptr %p
  ret void
}

; ASM: fptrunc64_to_32:
; ASM: l.fcvt.fp642fp32 [[REG1:ri(1[0-1]?|[0-9])]].fd, ->t.w
define void @fptrunc64_to_32(ptr %p, double %a) #0 #1 {
entry:
  %ret = fptrunc double %a to float
  store float %ret, ptr %p
  ret void
}

; ASM: fpext16_to_32:
; ASM: l.fcvt.fp162fp32 [[REG1:ri(1[0-1]?|[0-9])]].fh, ->t.w
define void @fpext16_to_32(ptr %p, half %a) #0 #1 {
entry:
  %ret = fpext half %a to float
  store float %ret, ptr %p
  ret void
}

; ASM: fptrunc32_to_16:
; ASM: l.fcvt.fp322fp16 [[REG1:ri(1[0-1]?|[0-9])]].fs, ->t.h
define void @fptrunc32_to_16(ptr %p, float %a) #0 #1 {
entry:
  %ret = fptrunc float %a to half
  store half %ret, ptr %p
  ret void
}

; ASM: fpext16_to_64:
; ASM: l.fcvt.fp162fp64 [[REG1:ri(1[0-1]?|[0-9])]].fh, ->t.d
define void @fpext16_to_64(ptr %p, half %a) #0 #1 {
entry:
  %ret = fpext half %a to double
  store double %ret, ptr %p
  ret void
}

; ASM: fptrunc64_to_16:
; ASM: l.fcvt.fp642fp16 [[REG1:ri(1[0-1]?|[0-9])]].fd, ->t.h
define void @fptrunc64_to_16(ptr %p, double %a) #0 #1 {
entry:
  %ret = fptrunc double %a to half
  store half %ret, ptr %p
  ret void
}

; MIR: name: sw_inreg
; MIR: SIMT_BXS_SCAR 0, [[REG1:%[0-9]+]], 4, 0, 32
; ASM: sw_inreg:
; ASM: l.bxs ri1.sd, 0, 32, ->t.d
define void @sw_inreg(ptr %p, i64 %a) #0 #1 {
entry:
  %trunc = trunc i64 %a to i32
  %ext = sext i32 %trunc to i64
  store i64 %ext, ptr %p
  ret void
}

; MIR: name: sh_inreg
; MIR: SIMT_BXS_SCAR 0, [[REG1:%[0-9]+]], 4, 0, 16
; ASM: sh_inreg:
; ASM: l.bxs ri1.sd, 0, 16, ->t.d
define void @sh_inreg(ptr %p, i64 %a) #0 #1 {
entry:
  %trunc = trunc i64 %a to i16
  %ext = sext i16 %trunc to i64
  store i64 %ext, ptr %p
  ret void
}

; MIR: name: sh_inreg32
; MIR: SIMT_BXS_SCAR 1, [[REG1:%[0-9]+]], 5, 0, 16
; ASM: sh_inreg32:
; ASM: l.bxs ri1.sw, 0, 16, ->t.w
define void @sh_inreg32(ptr %p, i32 %a) #0 #1 {
entry:
  %trunc = trunc i32 %a to i16
  %ext = sext i16 %trunc to i32
  store i32 %ext, ptr %p
  ret void
}

; MIR: name: sb_inreg16
; MIR: SIMT_BXS_SCAR 2, [[REG1:%[0-9]+]], 6, 0, 8
; ASM: sb_inreg16:
; ASM: l.bxs ri1.sh, 0, 8, ->t.h
define void @sb_inreg16(ptr %p, i16 %a) #0 #1 {
entry:
  %trunc = trunc i16 %a to i8
  %ext = sext i8 %trunc to i16
  store i16 %ext, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }