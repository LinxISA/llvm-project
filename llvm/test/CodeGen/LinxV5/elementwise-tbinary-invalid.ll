; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-opcode.ll 2>&1 | FileCheck %s --check-prefix=OPCODE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-layout.ll 2>&1 | FileCheck %s --check-prefix=LAYOUT
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-shape.ll 2>&1 | FileCheck %s --check-prefix=SHAPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/bad-dtype.ll 2>&1 | FileCheck %s --check-prefix=DTYPE
; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %t/dynamic-opcode.ll 2>&1 | FileCheck %s --check-prefix=DYNAMIC

; OPCODE: invalid LinxV5 element binary opcode operand
; LAYOUT: element binary requires U32/S32 CUBE_M32 <32 x i32> Tiles
; SHAPE: element binary requires U32/S32 CUBE_M32 <32 x i32> Tiles
; DTYPE: element binary requires U32/S32 CUBE_M32 <32 x i32> Tiles
; DYNAMIC: LinxV5 element binary opcode operand must be a compile-time constant

;--- bad-opcode.ll
define void @bad_opcode(ptr %ap, ptr %bp, ptr %out) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 10,
      <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- bad-layout.ll
define void @bad_layout(ptr %ap, ptr %bp, ptr %out) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 0, i64 0,
      <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- bad-shape.ll
define void @bad_shape(ptr %ap, ptr %bp, ptr %out) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 16, i64 2, i64 25, i64 29, i64 0,
      <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- bad-dtype.ll
define void @bad_dtype(ptr %ap, ptr %bp, ptr %out) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 18, i64 29, i64 0,
      <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)

;--- dynamic-opcode.ll
define void @dynamic_opcode(ptr %ap, ptr %bp, ptr %out, i64 %op) {
  %a = load <32 x i32>, ptr %ap
  %b = load <32 x i32>, ptr %bp
  %r = call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
      i64 32, i64 1, i64 25, i64 29, i64 %op,
      <32 x i32> %a, <32 x i32> %b)
  store <32 x i32> %r, ptr %out
  ret void
}
declare <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32(
    i64, i64, i64, i64, i64, <32 x i32>, <32 x i32>)
