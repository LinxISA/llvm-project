; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64v5 -O2 2>&1 | FileCheck %s --check-prefixes=ASM
target triple = "linx64v5-unknown-linux-musl"

; MIR: name: addi64
; MIR: SIMT_ADDI 0, {{(killed )?}}[[REG1:%[0-9]+]], 4, 1
; ASM: addi64:
; ASM: v.addi [[REG1:vt#(1[0-1]?|[0-9])]].sd, 1, ->vt.d
define void @addi64(ptr %p, ptr %addra) #1 {
  %a = load i64, ptr %addra
  %c = add i64 %a, 1
  store i64 %c, ptr %p
  ret void
}

; MIR: name: addi32
; MIR: SIMT_ADDI 1, {{(killed )?}}[[REG1:%[0-9]+]], 5, 1
; ASM: addi32:
; ASM: v.addi [[REG1:vt#(1[0-1]?|[0-9])]].sw, 1, ->vt.w
define void @addi32(ptr %p, ptr %addra) #1 {
  %a = load i32, ptr %addra
  %c = add i32 %a, 1
  store i32 %c, ptr %p
  ret void
}

; MIR: name: addi16
; MIR: SIMT_ADDI 2, {{(killed )?}}[[REG1:%[0-9]+]], 6, 1
; ASM: addi16:
; ASM: v.addi [[REG1:vt#(1[0-1]?|[0-9])]].sh, 1, ->vt.h
define void @addi16(ptr %p, ptr %addra) #1 {
  %a = load i16, ptr %addra
  %c = add i16 %a, 1
  store i16 %c, ptr %p
  ret void
}

; MIR: name: addi8
; MIR: SIMT_ADDI 3, {{(killed )?}}[[REG1:%[0-9]+]], 7, 1
; ASM: addi8:
; ASM: v.addi [[REG1:vt#(1[0-1]?|[0-9])]].sb, 1, ->vt.b
define void @addi8(ptr %p, ptr %addra) #1 {
  %a = load i8, ptr %addra
  %c = add i8 %a, 1
  store i8 %c, ptr %p
  ret void
}

; MIR: name: cmp_eqi64
; MIR: SIMT_CMP_EQI 0, {{(killed )?}}[[REG1:%[0-9]+]], 4, 1
; ASM: cmp_eqi64:
; ASM: v.cmp.eqi [[REG1:vt#(1[0-1]?|[0-9])]].sd, 1, ->vt.d
define void @cmp_eqi64(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %cmp = icmp eq i64 %a, 1
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eqi32
; MIR: SIMT_CMP_EQI 0, {{(killed )?}}[[REG1:%[0-9]+]], 5, 1
; ASM: cmp_eqi32:
; ASM: v.cmp.eqi [[REG1:vt#(1[0-1]?|[0-9])]].sw, 1, ->vt.d
define void @cmp_eqi32(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i32, ptr %addra
  %cmp = icmp eq i32 %a, 1
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eqi16
; MIR: SIMT_CMP_EQI 0, {{(killed )?}}[[REG1:%[0-9]+]], 6, 1
; ASM: cmp_eqi16:
; ASM: v.cmp.eqi [[REG1:vt#(1[0-1]?|[0-9])]].sh, 1, ->vt.d
define void @cmp_eqi16(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %cmp = icmp eq i16 %a, 1
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: cmp_eqi8
; MIR: SIMT_CMP_EQI 0, {{(killed )?}}[[REG1:%[0-9]+]], 7, 1
; ASM: cmp_eqi8:
; ASM: v.cmp.eqi [[REG1:vt#(1[0-1]?|[0-9])]].sb, 1, ->vt.d
define void @cmp_eqi8(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %cmp = icmp eq i8 %a, 1
  store i1 %cmp, ptr %p
  ret void
}

; MIR: name: shli64
; MIR: SIMT_SLLI 0, {{(killed )?}}[[REG1:%[0-9]+]], 4, 1
; ASM: shli64:
; ASM: v.slli [[REG1:vt#(1[0-1]?|[0-9])]].sd, 1, ->vt.d
define void @shli64(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %shl = shl i64 %a, 1
  store i64 %shl, ptr %p
  ret void
}

; MIR: name: shli32
; MIR: SIMT_SLLI 1, {{(killed )?}}[[REG1:%[0-9]+]], 5, 1
; ASM: shli32:
; ASM: v.slli [[REG1:vt#(1[0-1]?|[0-9])]].sw, 1, ->vt.w
define void @shli32(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i32, ptr %addra
  %shl = shl i32 %a, 1
  store i32 %shl, ptr %p
  ret void
}

; MIR: name: shli16
; MIR: SIMT_SLLI 2, {{(killed )?}}[[REG1:%[0-9]+]], 6, 1
; ASM: shli16:
; ASM: v.slli [[REG1:vt#(1[0-1]?|[0-9])]].sh, 1, ->vt.h
define void @shli16(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %shl = shl i16 %a, 1
  store i16 %shl, ptr %p
  ret void
}

; MIR: name: shli8
; MIR: SIMT_SLLI 3, {{(killed )?}}[[REG1:%[0-9]+]], 7, 1
; ASM: shli8:
; ASM: v.slli [[REG1:vt#(1[0-1]?|[0-9])]].sb, 1, ->vt.b
define void @shli8(ptr %p, ptr %addra) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %shl = shl i8 %a, 1
  store i8 %shl, ptr %p
  ret void
}

attributes #1 = {"__vec__"}
