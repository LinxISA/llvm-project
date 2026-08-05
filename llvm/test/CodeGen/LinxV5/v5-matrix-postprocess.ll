; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

; v5 matrix postprocess: the deleted TMATMUL*_FIXP opcodes (Function 9-14)
; are reserved/illegal. Their PostProcess capability now lives on the active
; TMATMUL operations (Function 0,1,2), each of which carries exactly one
; mandatory B.FPATR. This file migrates the old v5-matmul-fixp.ll @fixp case
; to the active TMATMUL opcode.
;
; The old @acc_fixp case (blk_matmul + blk_matmul.acc.fixp chain) is not
; carried here yet: the non-FIXP ACC variant (blk_matmul_ac / TMATMUL.BIAS
; with a C/partial-sum third source) hits a pre-existing RegisterCoalescer
; abort on baseline 36a8c5103097 that is entangled with the physical-ACC
; model (Tile_ACC1 / ACC_TILE_SRC). It will be re-enabled once the physical
; ACC migration (work-package B) lands and the ACC operand stream becomes
; an ordinary Local tile. See CODEX_HANDOFF.md ACC deferral section.

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

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v128f32(i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
declare <128 x float> @llvm.linx.blk.matmul.v128f32.v128f32.v128f32(i64, i64, i64, i64, i64, <128 x float>, <128 x float>)
