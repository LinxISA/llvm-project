; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

; v5 FPATR contract: every supported TMATMUL/TMATMULMX CUBE bundle carries
; exactly one B.FPATR after B.DATR (when present), not just the .FIXP
; variants covered by v5-matmul-fixp.ll. This test pins:
;   * <plain>     - non-FIXP base TMATMUL now emits B.FPATR.
;   * <bias_fixp> - a previously-uncovered .FIXP variant (PseudoMAMULB_BIAS_FIXP)
;                   that emitted neither B.DATR nor B.FPATR before the predicate
;                   was widened from isFixpMatmulPseudo to isMatmulPseudo.
; The non-FIXP MX path (int_linx_blk_matmulmx) has a pre-existing
; RegisterCoalescer abort unrelated to B.FPATR and is not exercised here.

target triple = "linx64v5"

; CHECK-LABEL: <plain>:
; CHECK: BSTART.CUBE TMATMUL, FP32
; CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0
; CHECK-NOT: B.FPATR
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111, last
define void @plain(ptr %in_a, ptr %in_b, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %result = call <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

; CHECK-LABEL: <bias_fixp>:
; CHECK: BSTART.CUBE TMATMUL.BIAS.FIXP, FP32
; CHECK-NEXT: B.FPATR 0, 0, 0, 0, 0, 0, 0
; CHECK-NOT: B.FPATR
; CHECK: B.IOT {{.*}}, {{.*}}, mask=1111
; CHECK: B.IOT mask=1111, last
define void @bias_fixp(ptr %in_a, ptr %in_b, ptr %in_bias, ptr %out) {
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_a, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %in_b, i64 16)
  %bias = call <128 x float> @llvm.linx.blk.tload.v128f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %in_bias, i64 16)
  %result = call <128 x float> @llvm.linx.blk.matmul.bias.fixp.v128f32.v128f32.v128f32.v128f32(i64 16, i64 16, i64 16, i64 1, i64 1, <128 x float> %a, <128 x float> %b, <128 x float> %bias)
  call void @llvm.linx.blk.tstore.v128f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %out, i64 16, <128 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.bias.fixp.v128f32.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>, <128 x float>)
