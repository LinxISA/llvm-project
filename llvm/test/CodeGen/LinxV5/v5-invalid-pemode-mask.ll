; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true \
; RUN:   -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o /dev/null 2>&1 | \
; RUN:   FileCheck %s

; PTO-ISA ADR 0069: PE participation encodes a 3-bit PEMode; only the masks
; 0000/1000/0100/0010/0001/1100/1110/1111 are encodable. mask=0011 (value 3)
; has no PEMode and must be rejected, not silently folded into a PE mask.

; CHECK: PE mask has no PEMode encoding

target triple = "linx64v5"

define void @invalid_mask_0011(ptr %in, ptr %out) {
  %src = call <256 x float> @llvm.linx.blk.tload.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, i64 0, ptr %in, i64 0)
  ; PE mask operand = 3 (0011) has no PEMode -> must fail.
  %shared = call i64 @llvm.linx.v5.shared.l2s.publish.v256f32(i64 1, i64 3, <256 x float> %src)
  call void @llvm.linx.blk.tstore.v256f32(i64 1, i64 1, i64 1, i64 1, i64 0, ptr %out, i64 0, <256 x float> %src)
  ret void
}

declare <256 x float> @llvm.linx.blk.tload.v256f32(i64, i64, i64, i64, i64, i64, ptr, i64)
declare void @llvm.linx.blk.tstore.v256f32(i64, i64, i64, i64, i64, ptr, i64, <256 x float>)
declare i64 @llvm.linx.v5.shared.l2s.publish.v256f32(i64, i64, <256 x float>)