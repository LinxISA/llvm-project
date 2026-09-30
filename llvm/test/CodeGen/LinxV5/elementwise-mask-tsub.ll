; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s

; A semantic masked TEPL operation using a PredicateCell carrier.
; ZERO is selected and the layout is CUBE_M16 (31).
; CHECK-LABEL: <elementwise_masked_tsub>:
; CHECK: BSTART.TEPL TSUB, FP32
; CHECK-NEXT: B.DATR CUBE_M16, FP32, Zero, byte0, Eq, RNONE, nosat, 0, 1
; CHECK: B.DIM
; CHECK: B.IOT
; CHECK: B.IOT
; CHECK-NOT: ExecMaskPresent

define void @elementwise_masked_tsub(ptr %a_ptr, ptr %b_ptr) #0 {
entry:
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %a_ptr, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 1, i64 1, i64 1, i64 3, i64 4, ptr %b_ptr, i64 16)
  %result = call <128 x float>
      @llvm.linx.experimental.ew.tsub.masked.v128f32(
          i64 16, i64 16, i64 1, i64 31,
          <128 x float> %a, <128 x float> %b,
          <128 x float> %a, i64 0, i64 1)
  call void @llvm.linx.blk.tstore.v128f32(
      i64 16, i64 16, i64 1, i64 1, i64 0, ptr %a_ptr, i64 16,
      <128 x float> %result)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(
    i64, i64, i64, i64, i64, i64, ptr, i64)
declare <128 x float>
    @llvm.linx.experimental.ew.tsub.masked.v128f32(
        i64, i64, i64, i64, <128 x float>, <128 x float>,
        <128 x float>, i64, i64)
declare void @llvm.linx.blk.tstore.v128f32(
    i64, i64, i64, i64, i64, ptr, i64, <128 x float>)
attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="16" }
