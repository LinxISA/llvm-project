; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

; Inline-asm tile operands name architectural queue positions, not physical
; TILE register encodings.  The second producer is the newest value (t#1).
; After publishing it, the older value also becomes t#1.
define void @relative_tile_queue_rank() {
entry:
  %older = call <1024 x i32> asm sideeffect "BSTART.VEC TEXPANDS, FP32\0AB.DATR Null\0AB.IOT mask=1111, last, ->${0:q}<256B>", "=r"()
  %newest = call <1024 x i32> asm sideeffect "BSTART.VEC TEXPANDS, FP32\0AB.DATR Null\0AB.IOT mask=1111, last, ->${0:q}<256B>", "=r"()
  %shared = call i64 asm sideeffect "BSTART.TMOV FP32\0AB.IOT $1, mask=1111\0AB.IOS mask=1111, ->${0:S}<256B>", "=S,r"(<1024 x i32> %newest)
  %shared2 = call i64 asm sideeffect "BSTART.TMOV FP32\0AB.IOT $1, mask=1111\0AB.IOS mask=1111, ->${0:S}<256B>", "=S,r"(<1024 x i32> %older)
  ret void
}

; CHECK-LABEL: relative_tile_queue_rank:
; CHECK:      BSTART.VEC TEXPANDS, FP32
; CHECK:      B.IOT mask=1111, last, ->t<256B>
; CHECK:      BSTART.VEC TEXPANDS, FP32
; CHECK:      B.IOT mask=1111, last, ->t<256B>
; CHECK:      BSTART.TMOV FP32
; CHECK:      B.IOT t#1, mask=1111
; CHECK:      BSTART.TMOV FP32
; CHECK:      B.IOT t#1, mask=1111
