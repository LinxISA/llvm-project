; RUN: llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t
; RUN: llvm-objdump -d --no-show-raw-insn %t | FileCheck %s
;
; Provisional PR #352 carrier binding. The logical mask has already been
; packed into two Predicate-GPR words; this path binds those words through
; B.IOR before the CUBE TileOp and carries PredInv/Zero in B.DATR.

; CHECK-LABEL: <elementwise_masked_cube_matmul>:
; CHECK: BSTART.CUBE TMATMUL
; CHECK: B.DATR
; CHECK: B.FPATR
; CHECK: B.IOT
; CHECK: B.IOR [a2,a3,zero], ExecMaskPresent
; CHECK-LABEL: <elementwise_masked_cube_m32_controls>:
; CHECK: BSTART.CUBE TMATMUL
; CHECK: B.DATR
; CHECK: B.FPATR
; CHECK: B.IOT
; CHECK: B.IOR [a2,a3,zero], ExecMaskPresent
; CHECK-NOT: v.
; CHECK-NOT: l.
; CHECK-NOT: ri[0-9]
; CHECK-NOT: vt[0-9]
; CHECK-NOT: vu[0-9]
; CHECK-NOT: vm[0-9]
; CHECK-NOT: vn[0-9]

define void @elementwise_masked_cube_matmul(
    ptr %a_ptr, ptr %b_ptr, i64 %mask_low, i64 %mask_high) #0 {
entry:
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a_ptr, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %b_ptr, i64 16)
  call <128 x float>
      @llvm.linx.experimental.ew.cube.matmul.masked.v128f32.v128f32.v128f32(
          i64 16, i64 16, i64 16, i64 1, i64 1,
          <128 x float> %a, <128 x float> %b,
          i64 %mask_low, i64 %mask_high, i64 0, i64 0)
  ret void
}

define void @elementwise_masked_cube_m32_controls(
    ptr %a_ptr, ptr %b_ptr, i64 %mask_low, i64 %mask_high) #0 {
entry:
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 32, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a_ptr, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %b_ptr, i64 16)
  call <128 x float>
      @llvm.linx.experimental.ew.cube.matmul.masked.v128f32.v128f32.v128f32(
          i64 32, i64 16, i64 16, i64 1, i64 1,
          <128 x float> %a, <128 x float> %b,
          i64 %mask_low, i64 %mask_high, i64 1, i64 1)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64, i64, ptr, i64)

declare <128 x float>
    @llvm.linx.experimental.ew.cube.matmul.masked.v128f32.v128f32.v128f32(
        i64, i64, i64, i64, i64, <128 x float>, <128 x float>, i64, i64, i64, i64)

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="128" }
