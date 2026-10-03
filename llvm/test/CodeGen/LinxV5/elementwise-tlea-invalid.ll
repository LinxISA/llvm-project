; RUN: not --crash llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -filetype=null %s 2>&1 | FileCheck %s

; CHECK: TLEA source data type must be S32, U32, S64 or U64
define void @bad_tlea_type(ptr %out, ptr %in) {
  %indices = load <128 x i32>, ptr %in
  %r = call <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32(
      i64 32, i64 4, i64 1, i64 0, <128 x i32> %indices, i64 32)
  store <128 x i64> %r, ptr %out
  ret void
}

declare <128 x i64> @llvm.linx.experimental.ew.tlea.v128i64.v128i32(
    i64, i64, i64, i64, <128 x i32>, i64)
