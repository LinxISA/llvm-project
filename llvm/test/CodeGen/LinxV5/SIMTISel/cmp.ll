; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM

target triple = "linx64v5-unknown-linux-musl"

; MIR: name: cmp_eq64
; MIR: SIMT_CMP_EQ 0, {{(killed )?}}[[REG1:%[0-9]+]], 4, {{(killed )?}}[[REG2:%[0-9]+]], 4
; ASM: cmp_eq64:
; ASM: v.cmp.eq [[REG1:vt#(1[0-1]?|[0-9])]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, ->vt.d
define void @cmp_eq64(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %b = load i64, ptr %addrb
  %cmp = icmp eq i64 %a, %b
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eq32
; MIR: SIMT_CMP_EQ 0, {{(killed )?}}[[REG1:%[0-9]+]], 5, {{(killed )?}}[[REG2:%[0-9]+]], 5
; ASM: cmp_eq32:
; ASM: v.cmp.eq [[REG1:vt#(1[0-1]?|[0-9])]].sw, [[REG1:vt#(1[0-1]?|[0-9])]].sw, ->vt.d
define void @cmp_eq32(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i32, ptr %addra
  %b = load i32, ptr %addrb
  %cmp = icmp eq i32 %a, %b
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eq16
; MIR: SIMT_CMP_EQ 0, {{(killed )?}}[[REG1:%[0-9]+]], 6, {{(killed )?}}[[REG2:%[0-9]+]], 6
; ASM: cmp_eq16:
; ASM: v.cmp.eq [[REG1:vt#(1[0-1]?|[0-9])]].sh, [[REG1:vt#(1[0-1]?|[0-9])]].sh, ->vt.d
define void @cmp_eq16(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %b = load i16, ptr %addrb
  %cmp = icmp eq i16 %a, %b
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eq8
; MIR: SIMT_CMP_EQ 0, {{(killed )?}}[[REG1:%[0-9]+]], 7, {{(killed )?}}[[REG2:%[0-9]+]], 7
; ASM: cmp_eq8:
; ASM: v.cmp.eq [[REG1:vt#(1[0-1]?|[0-9])]].sb, [[REG1:vt#(1[0-1]?|[0-9])]].sb, ->vt.d
define void @cmp_eq8(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %b = load i8, ptr %addrb
  %cmp = icmp eq i8 %a, %b
  store i1 %cmp, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }