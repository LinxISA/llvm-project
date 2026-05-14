; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM
target triple = "linx64v5-unknown-linux-musl"

; ASM: loadi64:
; ASM: v.ld [ri0.sd, zero.sd], ->vt.d
define void @loadi64(ptr %p) #1 {
  %a = load volatile i64, ptr %p
  ret void
}

; ASM: loadi32:
; ASM: v.lw [ri0.sd, zero.sd], ->vt.w
define void @loadi32(ptr %p) #1 {
  %a = load volatile i32, ptr %p
  ret void
}

; ASM: loadi16:
; ASM: v.lh [ri0.sd, zero.sd], ->vt.h
define void @loadi16(ptr %p) #1 {
  %a = load volatile i16, ptr %p
  ret void
}

; ASM: loadi8:
; ASM: v.lb [ri0.sd, zero.sd], ->vt.b
define void @loadi8(ptr %p) #1 {
  %a = load volatile i8, ptr %p
  ret void
}

; ASM: loadf64:
; ASM: v.ld [ri0.sd, zero.sd], ->vt.d
define void @loadf64(ptr %p) #1 {
  %a = load volatile double, ptr %p
  ret void
}

; ASM: loadf32:
; ASM: v.lw [ri0.sd, zero.sd], ->vt.w
define void @loadf32(ptr %p) #1 {
  %a = load volatile float, ptr %p
  ret void
}

; ASM: loadf16:
; ASM: v.lh [ri0.sd, zero.sd], ->vt.h
define void @loadf16(ptr %p) #1 {
  %a = load volatile half, ptr %p
  ret void
}

; ASM: loadbf16:
; ASM: v.lh [ri0.sd, zero.sd], ->vt.h
define void @loadbf16(ptr %p) #1 {
  %a = load volatile bfloat, ptr %p
  ret void
}

; COMM: Extend load below.

; ASM: sextloadi32toi64:
; ASM: v.lw [ri0.sd, zero.sd], ->vt.d
define void @sextloadi32toi64(ptr %p, ptr %q) #1 {
  %a = load i32, ptr %p
  %ext = sext i32 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: sextloadi16toi64:
; ASM: v.lh [ri0.sd, zero.sd], ->vt.d
define void @sextloadi16toi64(ptr %p, ptr %q) #1 {
  %a = load i16, ptr %p
  %ext = sext i16 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: sextloadi16toi32:
; ASM: v.lh [ri0.sd, zero.sd], ->vt.w
define void @sextloadi16toi32(ptr %p, ptr %q) #1 {
  %a = load i16, ptr %p
  %ext = sext i16 %a to i32
  store i32 %ext, ptr %q
  ret void
}

; ASM: sextloadi8toi64:
; ASM: v.lb [ri0.sd, zero.sd], ->vt.d
define void @sextloadi8toi64(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = sext i8 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: sextloadi8toi32:
; ASM: v.lb [ri0.sd, zero.sd], ->vt.w
define void @sextloadi8toi32(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = sext i8 %a to i32
  store i32 %ext, ptr %q
  ret void
}

; ASM: sextloadi8toi16:
; ASM: v.lb [ri0.sd, zero.sd], ->vt.h
define void @sextloadi8toi16(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = sext i8 %a to i16
  store i16 %ext, ptr %q
  ret void
}

; ASM: zextloadi32toi64:
; ASM: v.lwu [ri0.sd, zero.sd], ->vt.d
define void @zextloadi32toi64(ptr %p, ptr %q) #1 {
  %a = load i32, ptr %p
  %ext = zext i32 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: zextloadi16toi64:
; ASM: v.lhu [ri0.sd, zero.sd], ->vt.d
define void @zextloadi16toi64(ptr %p, ptr %q) #1 {
  %a = load i16, ptr %p
  %ext = zext i16 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: zextloadi16toi32:
; ASM: v.lhu [ri0.sd, zero.sd], ->vt.w
define void @zextloadi16toi32(ptr %p, ptr %q) #1 {
  %a = load i16, ptr %p
  %ext = zext i16 %a to i32
  store i32 %ext, ptr %q
  ret void
}

; ASM: zextloadi8toi64:
; ASM: v.lbu [ri0.sd, zero.sd], ->vt.d
define void @zextloadi8toi64(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = zext i8 %a to i64
  store i64 %ext, ptr %q
  ret void
}

; ASM: zextloadi8toi32:
; ASM: v.lbu [ri0.sd, zero.sd], ->vt.w
define void @zextloadi8toi32(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = zext i8 %a to i32
  store i32 %ext, ptr %q
  ret void
}

; ASM: zextloadi8toi16:
; ASM: v.lbu [ri0.sd, zero.sd], ->vt.h
define void @zextloadi8toi16(ptr %p, ptr %q) #1 {
  %a = load i8, ptr %p
  %ext = zext i8 %a to i16
  store i16 %ext, ptr %q
  ret void
}

; MIR: name: loadrri64
; MIR: %3:simtcgvl = SIMT_LD_GLOBAL 0, %0, 4, %1, 4, 3, implicit $simt_p
; MIR: SIMT_SD_GLOBAL killed %3, 4, %2, 4, $r0, 4, 0, implicit $simt_p
; ASM: loadrri64:
; ASM: v.ld [ri0.sd, ri1.sd<<3], ->vt.d
define void @loadrri64(ptr %p, i64 %i, ptr %q) #1 {
  %addr = getelementptr inbounds i64, ptr %p, i64 %i
  %a = load i64, ptr %addr
  store i64 %a, ptr %q
  ret void
}

; MIR: name: loadrri32
; MIR: %3:simtcgvl = SIMT_LD_GLOBAL 0, %0, 4, %1, 5, 3, implicit $simt_p
; MIR: SIMT_SD_GLOBAL killed %3, 4, %2, 4, $r0, 4, 0, implicit $simt_p
; ASM: loadrri32:
; ASM: v.ld [ri0.sd, ri1.sw<<3], ->vt.d
define void @loadrri32(ptr %p, i32 %i, ptr %q) #1 {
  %addr = getelementptr inbounds i64, ptr %p, i32 %i
  %a = load i64, ptr %addr
  store i64 %a, ptr %q
  ret void
}

; MIR: name: loadrri64shift
; MIR: %3:simtcgvl = SIMT_LD_GLOBAL 0, %0, 4, %1, 4, 4, implicit $simt_p
; MIR: SIMT_SD_GLOBAL killed %3, 4, %2, 4, $r0, 4, 0, implicit $simt_p
; ASM: loadrri64shift:
; ASM: v.ld [ri0.sd, ri1.sd<<4], ->vt.d
define void @loadrri64shift(ptr %p, i64 %i, ptr %q) #1 {
  %off = shl i64 %i, 4
  %addr = getelementptr inbounds i8, ptr %p, i64 %off
  %a = load i64, ptr %addr, align 8
  store i64 %a, ptr %q
  ret void
}

; MIR: name: loadscaled
; MIR: %2:simtcgvl = SIMT_LDI_GLOBAL 0, %0, 4, 3, implicit $simt_p
; MIR: SIMT_SD_GLOBAL killed %2, 4, %1, 4, $r0, 4, 0, implicit $simt_p
; ASM: loadscaled:
; ASM: v.ldi [ri0.sd, 24], ->vt.d
define void @loadscaled(ptr %p, ptr %q) #1 {
  %addr = getelementptr inbounds i64, ptr %p, i64 3
  %a = load i64, ptr %addr, align 8
  store i64 %a, ptr %q
  ret void
}

; MIR: name: loadunscaled
; MIR: %2:simtcgvl = SIMT_LDI_U_GLOBAL 0, %0, 4, 17, implicit $simt_p
; MIR: SIMT_SD_GLOBAL killed %2, 4, %1, 4, $r0, 4, 0, implicit $simt_p
; ASM: loadunscaled:
; ASM: v.ldi.u [ri0.sd, 17], ->vt.d
define void @loadunscaled(ptr %p, ptr %q) #1 {
  %addr = getelementptr inbounds i8, ptr %p, i64 17
  %a = load i64, ptr %addr, align 8
  store i64 %a, ptr %q
  ret void
}

@sym = external global [2048 x i64], align 8

; MIR: name: loadsym
; MIR-NOT: SIMT_LDI_U 0, killed %1, 4, target-flags(linx-tpcrel-lo) <mcsymbol
; ASM: loadsym:
; ASM-NOT: v.ldi.u.local [vt#1.sd, %tpcrel_lo(.Ltmp{{[0-9]+}})], ->vt.d
define void @loadsym(ptr %q) #1 {
  ;%a = load i64, ptr getelementptr inbounds (i64, ptr @sym, i64 0)
  ;store i64 %a, ptr %q
  ret void
}

; ASM: loadrif64:
; ASM: v.ldi [ri0.sd, 8], ->vt.d
define void @loadrif64(ptr %p) #1 {
  %addr = getelementptr inbounds double, ptr %p, i64 1
  %a = load volatile double, ptr %addr
  ret void
}

; ASM: loadrif32:
; ASM: v.lwi [ri0.sd, 4], ->vt.w
define void @loadrif32(ptr %p) #1 {
  %addr = getelementptr inbounds float, ptr %p, i64 1
  %a = load volatile float, ptr %addr
  ret void
}

; ASM: loadrif16:
; ASM: v.lhi [ri0.sd, 2], ->vt.h
define void @loadrif16(ptr %p) #1 {
  %addr = getelementptr inbounds half, ptr %p, i64 1
  %a = load volatile half, ptr %addr
  ret void
}

; ASM: loadribf16:
; ASM: v.lhi [ri0.sd, 2], ->vt.h
define void @loadribf16(ptr %p) #1 {
  %addr = getelementptr inbounds bfloat, ptr %p, i64 1
  %a = load volatile bfloat, ptr %addr
  ret void
}

attributes #1 = {"__vec__"}
