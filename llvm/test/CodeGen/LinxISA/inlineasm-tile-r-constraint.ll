; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

; A vector value attached to the conventional "r" inline-assembly constraint
; is an architectural tile operand. Source operands print the complete tile
; queue slot, while the %q modifier prints only the destination queue bank
; required by canonical B.IOT syntax.

define void @inlineasm_tile_r() {
entry:
  %src0 = call <1024 x i32> asm sideeffect "BSTART.VEC TEXPANDS, U32\0AB.DATR Null\0AB.IOT mask=1111, last, ->${0:q}<4KB>", "=r"()
  %src1 = call <1024 x i32> asm sideeffect "BSTART.VEC TEXPANDS, U32\0AB.DATR Null\0AB.IOT mask=1111, last, ->${0:q}<4KB>", "=r"()
  %out = call <1024 x i32> asm sideeffect "BSTART.VEC TADD, U32\0AB.DATR Null\0AB.IOT $1, $2, mask=1111, last, ->${0:q}<4KB>", "=r,r,r"(<1024 x i32> %src0, <1024 x i32> %src1)
  call void asm sideeffect "# sink $0", "r"(<1024 x i32> %out)
  ret void
}

define void @inlineasm_tile_dtype() {
entry:
  call void asm sideeffect "B.DATR NORM.normal, ${0:D}, Null", "i"(i32 4)
  ret void
}

; CHECK-LABEL: inlineasm_tile_r:
; CHECK: BSTART.VEC TADD, U32
; CHECK: B.IOT t#{{[1-8]}}, t#{{[1-8]}}, mask=1111, last, ->t<4KB>
; CHECK-LABEL: inlineasm_tile_dtype:
; CHECK: B.DATR NORM, FP16, Null
