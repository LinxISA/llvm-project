; RUN: llc < %s --march=linx64 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

; MIR: name: storei64
; MIR: SIMT_SCAR_SD  %1, 4, %0, 4, $r0, 4
; ASM: storei64:
; ASM: l.sd.local ri1.sd, [ri0.sd, zero.sd<<3]
define void @storei64(ptr %p, i64 %a) #0 {
  store i64 %a, ptr %p
  ret void
}

; MIR: name: storei32
; MIR: SIMT_SCAR_SW %1, 5, %0, 4, $r0, 4
; ASM: storei32:
; ASM: l.sw.local ri1.sw, [ri0.sd, zero.sd<<2]
define void @storei32(ptr %p, i32 %a) #0 {
  store i32 %a, ptr %p
  ret void
}

; MIR: name: storei16
; MIR: SIMT_SCAR_SH %1, 6, %0, 4, $r0, 4
; ASM: storei16:
; ASM: l.sh.local ri1.sh, [ri0.sd, zero.sd<<1]
define void @storei16(ptr %p, i16 %a) #0 {
  store i16 %a, ptr %p
  ret void
}

; MIR: name: storei8
; MIR: SIMT_SCAR_SB %1, 7, %0, 4, $r0, 4
; ASM: storei8:
; ASM: l.sb.local ri1.sb, [ri0.sd, zero.sd]
define void @storei8(ptr %p, i8 %a) #0 {
  store i8 %a, ptr %p
  ret void
}

; MIR: name: storef64
; MIR: SIMT_SCAR_SD %1, 4, %0, 4, $r0, 4
; ASM: storef64:
; ASM: l.sd.local ri1.sd, [ri0.sd, zero.sd<<3]
define void @storef64(ptr %p, double %a) #0 {
  store double %a, ptr %p
  ret void
}

; MIR: name: storef32
; MIR: SIMT_SCAR_SW %1, 5, %0, 4, $r0, 4
; ASM: storef32:
; ASM: l.sw.local ri1.sw, [ri0.sd, zero.sd<<2]
define void @storef32(ptr %p, float %a) #0 {
  store float %a, ptr %p
  ret void
}

; MIR: name: storef16
; MIR: SIMT_SCAR_SH %1, 6, %0, 4, $r0, 4
; ASM: storef16:
; ASM: l.sh.local ri1.sh, [ri0.sd, zero.sd<<1]
define void @storef16(ptr %p, half %a) #0 {
  store half %a, ptr %p
  ret void
}

; MIR: name: storebf16
; MIR: SIMT_SCAR_SH %1, 6, %0, 4, $r0, 4
; ASM: storebf16:
; ASM: l.sh.local ri1.sh, [ri0.sd, zero.sd<<1]
define void @storebf16(ptr %p, bfloat %a) #0 {
  store bfloat %a, ptr %p
  ret void
}

; COMM: Truncate store below

; MIR: name: truncstorei64toi32
; MIR: SIMT_SCAR_SW %1, 4, %0, 4, $r0, 4
; ASM: truncstorei64toi32:
; ASM: l.sw.local ri1.sd, [ri0.sd, zero.sd<<2]
define void @truncstorei64toi32(ptr %p, i64 %a) #0 {
  %trunc = trunc i64 %a to i32
  store i32 %trunc, ptr %p
  ret void
}

; MIR: name: truncstorei64toi16
; MIR: SIMT_SCAR_SH %1, 4, %0, 4, $r0, 4
; ASM: truncstorei64toi16:
; ASM: l.sh.local ri1.sd, [ri0.sd, zero.sd<<1]
define void @truncstorei64toi16(ptr %p, i64 %a) #0 {
  %trunc = trunc i64 %a to i16
  store i16 %trunc, ptr %p
  ret void
}

; MIR: name: truncstorei64toi8
; MIR: SIMT_SCAR_SB %1, 4, %0, 4, $r0, 4
; ASM: truncstorei64toi8:
; ASM: l.sb.local ri1.sd, [ri0.sd, zero.sd]
define void @truncstorei64toi8(ptr %p, i64 %a) #0 {
  %trunc = trunc i64 %a to i8
  store i8 %trunc, ptr %p
  ret void
}

; MIR: name: truncstorei32toi16
; MIR: SIMT_SCAR_SH %1, 5, %0, 4, $r0, 4
; ASM: truncstorei32toi16:
; ASM: l.sh.local ri1.sw, [ri0.sd, zero.sd<<1]
define void @truncstorei32toi16(ptr %p, i32 %a) #0 {
  %trunc = trunc i32 %a to i16
  store i16 %trunc, ptr %p
  ret void
}

; MIR: name: truncstorei32toi8
; MIR: SIMT_SCAR_SB %1, 5, %0, 4, $r0, 4
; ASM: truncstorei32toi8:
; ASM: l.sb.local ri1.sw, [ri0.sd, zero.sd]
define void @truncstorei32toi8(ptr %p, i32 %a) #0 {
  %trunc = trunc i32 %a to i8
  store i8 %trunc, ptr %p
  ret void
}

; MIR: name: truncstorei16toi8
; MIR: SIMT_SCAR_SB %1, 6, %0, 4, $r0, 4
; ASM: truncstorei16toi8:
; ASM: l.sb.local ri1.sh, [ri0.sd, zero.sd]
define void @truncstorei16toi8(ptr %p, i16 %a) #0 {
  %trunc = trunc i16 %a to i8
  store i8 %trunc, ptr %p
  ret void
}

; MIR: name: storerri64scaled
; MIR: SIMT_SCAR_SD %2, 4, %0, 4, %1, 4
; ASM: storerri64scaled:
; ASM: l.sd.local ri2.sd, [ri0.sd, ri1.sd<<3]
define void @storerri64scaled(ptr %p, i64 %i, i64 %a) #0 {
  %addr = getelementptr inbounds i64, ptr %p, i64 %i
  store i64 %a, ptr %addr
  ret void
}

; MIR: name: storerrsexti32scaled
; MIR: SIMT_SCAR_SD %2, 4, %0, 4, %1, 5
; ASM: storerrsexti32scaled:
; ASM: l.sd.local ri2.sd, [ri0.sd, ri1.sw<<3]
define void @storerrsexti32scaled(ptr %p, i32 %i, i64 %a) #0 {
  %addr = getelementptr inbounds i64, ptr %p, i32 %i
  store i64 %a, ptr %addr
  ret void
}

; MIR: name: storerrzexti32scaled
; MIR: SIMT_SCAR_SD %2, 4, %0, 4, %1, 1
; ASM: storerrzexti32scaled:
; ASM: l.sd.local ri2.sd, [ri0.sd, ri1.uw<<3]
define void @storerrzexti32scaled(ptr %p, i32 %i, i64 %a) #0 {
  %ext = zext i32 %i to i64
  %addr = getelementptr inbounds i64, ptr %p, i64 %ext
  store i64 %a, ptr %addr
  ret void
}

; MIR: name: storerrzexti16unscaled
; MIR: SIMT_SCAR_SD_U %2, 4, %0, 4, %1, 2
; ASM: storerrzexti16unscaled:
; ASM: l.sd.u.local ri2.sd, [ri0.sd, ri1.uh]
define void @storerrzexti16unscaled(ptr %p, i16 %i, i64 %a) #0 {
  %ext = zext i16 %i to i64
  %addr = getelementptr inbounds i8, ptr %p, i64 %ext
  store i64 %a, ptr %addr
  ret void
}

; MIR: name: storeriscaled
; MIR: SIMT_SCAR_SDI %1, 4, %0, 4, 1
; ASM: storeriscaled:
; ASM: l.sdi.local ri1.sd, [ri0.sd, 8]
define void @storeriscaled(ptr %p, i64 %a) #0 {
  %addr = getelementptr inbounds i64, ptr %p, i64 1
  store i64 %a, ptr %addr
  ret void
}

; MIR: name: storeriunscaled
; MIR: SIMT_SCAR_SDI_U %1, 4, %0, 4, 7
; ASM: storeriunscaled:
; ASM: l.sdi.u.local ri1.sd, [ri0.sd, 7]
define void @storeriunscaled(ptr %p, i64 %a) #0 {
  %addr = getelementptr inbounds i8, ptr %p, i64 7
  store i64 %a, ptr %addr
  ret void
}

attributes #0 = {"__vec__"}
