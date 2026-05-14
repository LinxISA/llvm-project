; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM
target triple = "linx64v5-unknown-linux-musl"

; MIR: name: shl64
; MIR: SIMT_SLL 0, {{(killed )?}}[[REG1:%[0-9]+]], 4, {{(killed )?}}[[REG2:%[0-9]+]], 4
; ASM: shl64:
; ASM: v.sll [[REG1:vt#(1[0-1]?|[0-9])]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, ->vt.d
define void @shl64(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %b = load i64, ptr %addrb
  %shl = shl i64 %a, %b
  store i64 %shl, ptr %p
  ret void
}

; MIR: name: shl32
; MIR: SIMT_SLL 1, {{(killed )?}}[[REG1:%[0-9]+]], 5, {{(killed )?}}[[REG2:%[0-9]+]], 5
; ASM: shl32:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sw, [[REG2:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sw, ->vt.w
define void @shl32(ptr %p, ptr %addra, i32 %b) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %conv = trunc i64 %a to i32
  %shl = shl i32 %conv, %b
  store i32 %shl, ptr %p
  ret void
}

; MIR: name: shl32_32_64
; MIR: SIMT_SLL 1, {{(killed )?}}[[REG1:%[0-9]+]], 5, {{(killed )?}}[[REG2:%[0-9]+]], 4
; ASM: shl32_32_64:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sw, [[REG2:a[0-7]+|vt#[1-8]+|t#[1-8]+]].sd, ->vt.w
define void @shl32_32_64(ptr %p, ptr %addra, i64 %b) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %conv = trunc i64 %a to i32
  %amt = trunc i64 %b to i32
  %shl = shl i32 %conv, %amt
  store i32 %shl, ptr %p
  ret void
}

; MIR: name: shl16
; MIR: SIMT_SLL 2, {{(killed )?}}[[REG1:%[0-9]+]], 6, {{(killed )?}}[[REG2:%[0-9]+]], 6
; ASM: shl16:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sh, [[REG2:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sh, ->vt.h
define void @shl16(ptr %p, ptr %addra, i16 %b) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %shl = shl i16 %a, %b
  store i16 %shl, ptr %p
  ret void
}

; MIR: name: shl16_16_64
; MIR: SIMT_SLL 2, {{(killed )?}}[[REG1:%[0-9]+]], 6, {{(killed )?}}[[REG2:%[0-9]+]], 4
; ASM: shl16_16_64:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sh, [[REG2:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+|t#[1-8]+]].sd, ->vt.h
define void @shl16_16_64(ptr %p, ptr %addra, i64 %b) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %amt = trunc i64 %b to i16
  %shl = shl i16 %a, %amt
  store i16 %shl, ptr %p
  ret void
}

; MIR: name: shl8
; MIR: SIMT_SLL 3, {{(killed )?}}[[REG1:%[0-9]+]], 7, {{(killed )?}}[[REG2:%[0-9]+]], 7
; ASM: shl8:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sb, [[REG2:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sb, ->vt.b
define void @shl8(ptr %p, ptr %addra, i8 %b) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %shl = shl i8 %a, %b
  store i8 %shl, ptr %p
  ret void
}

; MIR: name: shl8_8_64
; MIR: SIMT_SLL 3, {{(killed )?}}[[REG1:%[0-9]+]], 7, {{(killed )?}}[[REG2:%[0-9]+]], 4
; ASM: shl8_8_64:
; ASM: v.sll [[REG1:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sb, [[REG2:a[0-7]+|vt#[1-8]+|ri(1[0-1]?|[0-9])+]].sd, ->vt.b
define void @shl8_8_64(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %b = load i64, ptr %addrb
  %amt = trunc i64 %b to i8
  %shl = shl i8 %a, %amt
  store i8 %shl, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }