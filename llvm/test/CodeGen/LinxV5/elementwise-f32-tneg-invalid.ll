; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/f64.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/integer.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/layout.ll 2>&1 | FileCheck %s
; CHECK: GPR-masked TNEG requires exact 32x1 FP32 CUBE_M32 Tiles and one low mask word

;--- f64.ll
define void @bad(ptr %input, ptr %output, i64 %mask) {
  %source = load <32 x double>, ptr %input
  %neg = call <32 x double> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f64(i64 32, i64 1, i64 0, i64 29, <32 x double> %source, i64 %mask)
  store <32 x double> %neg, ptr %output
  ret void
}
declare <32 x double> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f64(i64, i64, i64, i64, <32 x double>, i64)

;--- integer.ll
define void @bad(ptr %input, ptr %output, i64 %mask) {
  %source = load <32 x i32>, ptr %input
  %neg = call <32 x i32> @llvm.linx.experimental.ew.tneg.gpr.masked.v32i32(i64 32, i64 1, i64 1, i64 29, <32 x i32> %source, i64 %mask)
  store <32 x i32> %neg, ptr %output
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tneg.gpr.masked.v32i32(i64, i64, i64, i64, <32 x i32>, i64)

;--- layout.ll
define void @bad(ptr %input, ptr %output, i64 %mask) {
  %source = load <32 x float>, ptr %input
  %neg = call <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64 32, i64 1, i64 1, i64 0, <32 x float> %source, i64 %mask)
  store <32 x float> %neg, ptr %output
  ret void
}
declare <32 x float> @llvm.linx.experimental.ew.tneg.gpr.masked.v32f32(i64, i64, i64, i64, <32 x float>, i64)
