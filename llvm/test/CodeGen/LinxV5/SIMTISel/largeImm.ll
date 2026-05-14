; RUN: llc < %s --march=linx64 -linxv5-enable-HL-Inst-Opt=false -O2 2>&1 | FileCheck %s --check-prefixes=ASM
; RUN: llc < %s --march=linx64 -linxv5-enable-HL-Inst-Opt=true -O2 2>&1 | FileCheck %s --check-prefixes=ASMOPT
target triple = "linx64-unknown-linux-musl"
target datalayout = "e-m:e-p:64:64-i8:8:64-i16:16:64-i32:32:64-i64:64-i128:128-n64-S128"

; lui + l.addi -> l.addli
; 6144 = 0b0001 1000 0000 0000
; ASM: test_laddli:
; ASM: lui 1, ->t
; ASM: l.addi t#1.sd, 2048, ->t.d
; ASMOPT: test_laddli:
; ASMOPT: l.addli zero, 6144, ->t
define void @test_laddli(ptr %p) #0 {
entry:
  %arrayidx = getelementptr inbounds i64, ptr %p, i64 5
  store i64 6144, ptr %arrayidx
  ret void
}

; l.subi/l.addi + l.slli + l.addi/l.subi -> hl.lui + addi
; -4294963456 = 0xFFFF FFFF 0000 0F00
; ASM: test_hlui_addi:
; ASM: l.subi zero.sd, 1, ->t.d
; ASM: l.slli t#1.sd, 32, ->t.d
; ASM:l.addi t#1.sd, 3840, ->t.d
; ASMOPT: test_hlui_addi:
; ASMOPT: hl.lui -1, ->t
; ASMOPT: addi t#1, 3840, ->t
define void @test_hlui_addi(ptr %p) #0 {
entry:
  %arrayidx = getelementptr inbounds i64, ptr %p, i64 5
  store i64 -4294963456, ptr %arrayidx
  ret void
}

; lui + l.subi/l.addi + l.slli + l.addi/l.subi -> hl.lui + l.addli
; -1095233372416 = 0xFFFF FF00 FF00 FF00
; ASM: test_hlui_laddli:
; ASM: lui -4081, ->t
; ASM: l.addi t#1.sd, 3841, ->t.d
; ASM: l.slli t#1.sd, 16, ->t.d
; ASM: l.subi t#1.sd, 256, ->t.d
; ASMOPT: test_hlui_laddli:
; ASMOPT: hl.lui -256, ->t
; ASMOPT: l.addli t#1, 4278255360, ->t
define void @test_hlui_laddli(ptr %p) #0 {
entry:
  %arrayidx = getelementptr inbounds i64, ptr %p, i64 5
  store i64 -1095233372416, ptr %arrayidx
  ret void
}

attributes #0 = {"__vec__"}
