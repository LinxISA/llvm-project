; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

; v5 unified MATMUL: basic TMATMUL produces an ordinary Local Tile result
; (TILE_DstWithArrow), no dedicated ACC register/destination. The result feeds
; directly into TSTORE, and may be chained into a second TMATMUL as an input.

target triple = "linx64v5"

; CHECK-LABEL: <basic>:
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK-NOT: ->acc
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111, TSize={{[0-9]+}}, last, ->
; CHECK-NOT: ->acc
define void @basic(ptr %in_a, ptr %in_b, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %result = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <chain>:
; The result of the first TMATMUL is used directly as an input to a second
; TMATMUL — no ACCCVT, no implicit ACC def.
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111, TSize={{[0-9]+}}, last, ->
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111, TSize={{[0-9]+}}, last, ->
define void @chain(ptr %in_a, ptr %in_b, ptr %in_c, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %c = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_c, i64 16)
  %r1 = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b)
  %r2 = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %r1, <128 x float> %c)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %r2)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
