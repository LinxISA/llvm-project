; RUN: llc -mtriple=linx64v5-unknown-linux-gnu -mcpu=janus -O0 < %s | FileCheck %s
;
; PR #352 provisional carrier mapping:
;   cube_bit = row + column * rows
;   logical_bit = row * valid_cols + column.
; This test checks that the logical pack intrinsics disappear before ISel
; without selecting the SIMT register or instruction path.

; CHECK-LABEL: elementwise_mask_gpr_m16:
; CHECK-NOT: llvm.linx.experimental.ew.mask.pack.gpr
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

; CHECK-LABEL: elementwise_mask_gpr_m32:
; CHECK-NOT: llvm.linx.experimental.ew.mask.pack.gpr
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]
; CHECK: C.BSTART.STD RET

define i64 @elementwise_mask_gpr_m16(<8 x i1> %mask) #0 {
entry:
  %low = call i64 @llvm.linx.experimental.ew.mask.pack.gpr.low.v8i1(
      <8 x i1> %mask, i64 2, i64 3, i1 false)
  %high = call i64 @llvm.linx.experimental.ew.mask.pack.gpr.high.v8i1(
      <8 x i1> %mask, i64 2, i64 3, i1 false)
  %result = xor i64 %low, %high
  ret i64 %result
}

define i64 @elementwise_mask_gpr_m32(<8 x i1> %mask) #0 {
entry:
  %low = call i64 @llvm.linx.experimental.ew.mask.pack.gpr.low.v8i1(
      <8 x i1> %mask, i64 4, i64 2, i1 true)
  %high = call i64 @llvm.linx.experimental.ew.mask.pack.gpr.high.v8i1(
      <8 x i1> %mask, i64 4, i64 2, i1 true)
  %result = xor i64 %low, %high
  ret i64 %result
}

declare i64 @llvm.linx.experimental.ew.mask.pack.gpr.low.v8i1(
    <8 x i1>, i64, i64, i1)
declare i64 @llvm.linx.experimental.ew.mask.pack.gpr.high.v8i1(
    <8 x i1>, i64, i64, i1)

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="8" }
