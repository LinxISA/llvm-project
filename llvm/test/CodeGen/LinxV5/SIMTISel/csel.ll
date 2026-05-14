; RUN: llc < %s --march=linx64v5 -stop-after=finalize-isel -linxv5-enable-simt-clock-hand=true -O2 2>&1 | FileCheck %s --check-prefixes=MIR
; RUN: llc < %s --march=linx64v5 -linxv5-enable-simt-clock-hand=true -O2 2>&1 | FileCheck %s --check-prefixes=ASM

target triple = "linx64v5-unknown-linux-musl"

; MIR: name: csel64
; MIR: SIMT_CSEL 0, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 4, [[REG3:%[0-9]+]], 4, 0
; ASM: csel64:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, ->vt.d
define void @csel64(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i64, ptr %addra
  %b = load i64, ptr %addrb
  %cmp = icmp eq i64 %a, %b
  %ret = select i1 %cmp, i64 %a, i64 %b
  store i64 %ret, ptr %p
  ret void
}

; MIR: name: csel32
; MIR: SIMT_CSEL 1, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 5, [[REG3:%[0-9]+]], 5, 0
; ASM: csel32:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sw, [[REG1:vt#(1[0-1]?|[0-9])]].sw, ->vt.w
define void @csel32(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i32, ptr %addra
  %b = load i32, ptr %addrb
  %cmp = icmp eq i32 %a, %b
  %ret = select i1 %cmp, i32 %a, i32 %b
  store i32 %ret, ptr %p
  ret void
}

; MIR: name: csel16
; MIR: SIMT_CSEL 2, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 6, [[REG3:%[0-9]+]], 6, 0
; ASM: csel16:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sh, [[REG1:vt#(1[0-1]?|[0-9])]].sh, ->vt.h
define void @csel16(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i16, ptr %addra
  %b = load i16, ptr %addrb
  %cmp = icmp eq i16 %a, %b
  %ret = select i1 %cmp, i16 %a, i16 %b
  store i16 %ret, ptr %p
  ret void
}

; MIR: name: csel8
; MIR: SIMT_CSEL 3, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 7, [[REG3:%[0-9]+]], 7, 0
; ASM: csel8:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sb, [[REG1:vt#(1[0-1]?|[0-9])]].sb, ->vt.b
define void @csel8(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
entry:
  %a = load i8, ptr %addra
  %b = load i8, ptr %addrb
  %cmp = icmp eq i8 %a, %b
  %ret = select i1 %cmp, i8 %a, i8 %b
  store i8 %ret, ptr %p
  ret void
}

; MIR: name: cself64
; MIR: SIMT_CSEL 0, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 4, [[REG3:%[0-9]+]], 4, 0
; ASM: cself64:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sd, ->vt.d
define void @cself64(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
  %a = load double, ptr %addra
  %b = load double, ptr %addrb
  %cmp = fcmp oeq double %a, %b
  %sel = select i1 %cmp, double %a, double %b
  store double %sel, ptr %p
  ret void
}

; MIR: name: cself32
; MIR: SIMT_CSEL 1, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 5, [[REG3:%[0-9]+]], 5, 0
; ASM: cself32:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sw, [[REG1:vt#(1[0-1]?|[0-9])]].sw, ->vt.w
define void @cself32(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
  %a = load float, ptr %addra
  %b = load float, ptr %addrb
  %cmp = fcmp oeq float %a, %b
  %sel = select i1 %cmp, float %a, float %b
  store float %sel, ptr %p
  ret void
}

; MIR: name: cself16
; MIR: SIMT_CSEL 2, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 6, [[REG3:%[0-9]+]], 6, 0
; ASM: cself16:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sh, [[REG1:vt#(1[0-1]?|[0-9])]].sh, ->vt.h
define void @cself16(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
  %a = load half, ptr %addra
  %b = load half, ptr %addrb
  %cmp = fcmp oeq half %a, %b
  %sel = select i1 %cmp, half %a, half %b
  store half %sel, ptr %p
  ret void
}

; MIR: name: cselbf16
; MIR: SIMT_CSEL 2, killed [[REG1:%[0-9]+]], 4, [[REG2:%[0-9]+]], 6, [[REG3:%[0-9]+]], 6, 0
; ASM: cselbf16:
; ASM: v.csel [[REG1:a[0-7]+|vt#[1-8]+|vu#[1-8]+]].sd, [[REG1:vt#(1[0-1]?|[0-9])]].sh, [[REG1:vt#(1[0-1]?|[0-9])]].sh, ->vt.h
define void @cselbf16(ptr %p, ptr %addra, ptr %addrb) #0 #1 {
  %a = load bfloat, ptr %addra
  %b = load bfloat, ptr %addrb
  %cmp = fcmp oeq bfloat %a, %b
  %sel = select i1 %cmp, bfloat %a, bfloat %b
  store bfloat %sel, ptr %p
  ret void
}

attributes #0 = { mustprogress nofree norecurse nosync nounwind readnone willreturn "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-features"="+m,+relax" }
attributes #1 = { noinline "__vec__" }