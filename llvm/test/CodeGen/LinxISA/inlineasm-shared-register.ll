; RUN: llc -mtriple=linx64 -O2 < %s | FileCheck %s

define void @shared_publish() {
entry:
  %local = call <1024 x i32> asm sideeffect "BSTART.VEC TEXPANDS, FP32\0AB.DATR Null\0AB.IOT mask=1111, last, ->${0:q}<256B>", "=r"()
  %shared = call i64 asm sideeffect "BSTART.TMOV.L2S.PUBLISH FP32\0AB.IOT $1, mask=1111\0AB.IOS mask=1111, ->${0:S}<256B>", "=S,r"(<1024 x i32> %local)
  call void asm sideeffect "BSTART.TMATMUL FP32\0AB.DATR NORM.normal, FP32, Null\0AB.IOS ${0:S}, mask=1111", "S"(i64 %shared)
  ret void
}

; CHECK-LABEL: shared_publish:
; CHECK: BSTART.TMOV.L2S.PUBLISH FP32
; CHECK: B.IOS mask=1111, ->S[[SHARED:[0-9]+]]<256B>
; CHECK: BSTART.TMATMUL FP32
; CHECK: B.IOS S[[SHARED]], mask=1111
