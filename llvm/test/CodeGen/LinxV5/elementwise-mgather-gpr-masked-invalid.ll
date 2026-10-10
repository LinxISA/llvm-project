; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-index-type.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-shape.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-mask.ll 2>&1 | FileCheck %s
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-inactive-mode.ll 2>&1 | FileCheck %s

; CHECK: GPR-masked MGATHER requires exact 32x1 U32 values, S64/U64 byte offsets, CUBE_M32, one low GPR mask word and zero inactive lanes

;--- bad-index-type.ll
define void @invalid(ptr %out, ptr %base, ptr %offset.ptr, i64 %mask) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
      i64 32, i64 1, i64 25, i64 0, i64 29, i64 25, ptr %base,
      <32 x i64> %offsets, i64 %mask, i64 0, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, ptr, <32 x i64>, i64, i64, i64, i64)

;--- bad-inactive-mode.ll
define void @invalid(ptr %out, ptr %base, ptr %offset.ptr, i64 %mask) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
      i64 32, i64 1, i64 25, i64 0, i64 29, i64 24, ptr %base,
      <32 x i64> %offsets, i64 %mask, i64 0, i64 0, i64 0)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, ptr, <32 x i64>, i64, i64, i64, i64)

;--- bad-shape.ll
define void @invalid(ptr %out, ptr %base, ptr %offset.ptr, i64 %mask) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
      i64 16, i64 2, i64 25, i64 0, i64 29, i64 24, ptr %base,
      <32 x i64> %offsets, i64 %mask, i64 0, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, ptr, <32 x i64>, i64, i64, i64, i64)

;--- bad-mask.ll
define void @invalid(ptr %out, ptr %base, ptr %offset.ptr, i64 %mask) {
  %offsets = load <32 x i64>, ptr %offset.ptr
  %r = call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
      i64 32, i64 1, i64 25, i64 0, i64 29, i64 24, ptr %base,
      <32 x i64> %offsets, i64 %mask, i64 1, i64 0, i64 1)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(
    i64, i64, i64, i64, i64, i64, ptr, <32 x i64>, i64, i64, i64, i64)
