; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj %s -o %t 2>&1 | FileCheck %s
;
; PR #352 execution-mask carriers are restricted to Local CUBE_M16/CUBE_M32.

; CHECK: LLVM ERROR: masked CUBE execution mask requires M=16 or M=32

define void @invalid_masked_cube_m8(ptr %a_ptr, ptr %b_ptr, i64 %mask_low,
                                    i64 %mask_high) #0 {
entry:
  %a = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 8, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %a_ptr, i64 16)
  %b = call <128 x float> @llvm.linx.blk.tload.v128f32(
      i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %b_ptr, i64 16)
  call <128 x float>
      @llvm.linx.experimental.ew.cube.matmul.masked.v128f32.v128f32.v128f32(
          i64 8, i64 16, i64 16, i64 1, i64 1,
          <128 x float> %a, <128 x float> %b,
          i64 %mask_low, i64 %mask_high, i64 0, i64 0)
  ret void
}

declare <128 x float> @llvm.linx.blk.tload.v128f32(i64, i64, i64, i64, i64,
                                                    i64, ptr, i64)
declare <128 x float>
    @llvm.linx.experimental.ew.cube.matmul.masked.v128f32.v128f32.v128f32(
        i64, i64, i64, i64, i64, <128 x float>, <128 x float>, i64, i64,
        i64, i64)

attributes #0 = { "linx.elementwise" "linx.elementwise.lanes"="128" }
