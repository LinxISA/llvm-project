; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/mul.ll 2>&1 | FileCheck %s --check-prefix=MUL
; MUL: GPR-masked element binary requires an exact admitted logical32 or dense64 CUBE_M32 Tile
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/cmp-layout.ll 2>&1 | FileCheck %s --check-prefix=CMPLAYOUT
; CMPLAYOUT: TCMP GPR requires exact 32x1
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/cmp-unmasked.ll 2>&1 | FileCheck %s --check-prefix=CMPUNMASKED
; CMPUNMASKED: TCMP GPR requires exact 32x1
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/expand-f64.ll 2>&1 | FileCheck %s --check-prefix=EXPANDF64
; EXPANDF64: FP32 TEXPANDS requires exact 32x1 CUBE_M32 f32 Tile and scalar

;--- mul.ll
define void @bad(ptr %ap, ptr %bp, ptr %out, i64 %mask) {
  %a = load <32 x float>, ptr %ap
  %b = load <32 x float>, ptr %bp
  %r = call <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 29, i64 2, <32 x float> %a, <32 x float> %b, i64 %mask, i64 0, i64 0, i64 1)
  store <32 x float> %r, ptr %out
  ret void
}
declare <32 x float> @llvm.linx.experimental.ew.tbinary.gpr.masked.v32f32(i64, i64, i64, i64, i64, <32 x float>, <32 x float>, i64, i64, i64, i64)

;--- cmp-layout.ll
define i64 @bad(ptr %ap, ptr %bp, i64 %mask) {
  %a = load <32 x float>, ptr %ap
  %b = load <32 x float>, ptr %bp
  %r = call i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 0, <32 x float> %a, <32 x float> %b, i64 2, i64 %mask)
  ret i64 %r
}
declare i64 @llvm.linx.experimental.ew.tcmp.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, <32 x float>, i64, i64)

;--- cmp-unmasked.ll
define i64 @bad(ptr %ap, ptr %bp) {
  %a = load <32 x float>, ptr %ap
  %b = load <32 x float>, ptr %bp
  %r = call i64 @llvm.linx.experimental.ew.tcmp.gpr.v32f32(i64 32, i64 1, i64 1, i64 29, <32 x float> %a, <32 x float> %b, i64 2)
  ret i64 %r
}
declare i64 @llvm.linx.experimental.ew.tcmp.gpr.v32f32(i64, i64, i64, i64, <32 x float>, <32 x float>, i64)

;--- expand-f64.ll
define void @bad(ptr %out, double %scalar) {
  %r = call <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f64(i64 32, i64 1, i64 1, i64 29, double %scalar)
  store <32 x float> %r, ptr %out
  ret void
}
declare <32 x float> @llvm.linx.experimental.ew.texpands.v32f32.f64(i64, i64, i64, i64, double)
