// RUN: %clang++ --target=linx64 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

using TileDType = float tile_size(256);

// CHECK-LABEL: entry
// CHECK:      %0 = tail call <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, <256 x float> %TA, <256 x float> %TB)
// CHECK-NEXT: %1 = tail call <256 x float> @llvm.linx.blk.matmul.ac.v256f32.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, <256 x float> %TA, <256 x float> %TB, <256 x float> %TC)
int test(TileDType TO, TileDType TA, TileDType TB, TileDType TC) {
  blk_matmul(16, 16, 16, 1, TO, TA, TB);
  blk_matmul_ac(16, 16, 16, 1, TO, TA, TB, TC);
  return 0;
}

// CHECK-LABEL: entry
// CHECK:      %0 = tail call <256 x float> @llvm.linx.blk.matmulmx.v256f32.v256f32.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, i64 2, <256 x float> %TA, <256 x float> %TAX, <256 x float> %TB, <256 x float> %TBX)
// CHECK-NEXT: %1 = tail call <256 x float> @llvm.linx.blk.matmulmxb.v256f32.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, i64 2, <256 x float> %TA, <256 x float> %TB, <256 x float> %TBX)
// CHECK-NEXT: %2 = tail call <256 x float> @llvm.linx.blk.matmulmx.ac.v256f32.v256f32.v256f32.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, i64 2, <256 x float> %TA, <256 x float> %TAX, <256 x float> %TB, <256 x float> %TBX, <256 x float> %TC)
// CHECK-NEXT: %3 = tail call <256 x float> @llvm.linx.blk.matmulmxb.ac.v256f32.v256f32.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, i64 2, <256 x float> %TA, <256 x float> %TB, <256 x float> %TBX, <256 x float> %TC)
int test_mx(TileDType TO, TileDType TA, TileDType TAX,
            TileDType TB, TileDType TBX, TileDType TC) {
  blk_matmulmx(16, 16, 16, 1, 2, TO, TA, TAX, TB, TBX);
  blk_matmulmxb(16, 16, 16, 1, 2, TO, TA, TB, TBX);
  blk_matmulmx_ac(16, 16, 16, 1, 2, TO, TA, TAX, TB, TBX, TC);
  blk_matmulmxb_ac(16, 16, 16, 1, 2, TO, TA, TB, TBX, TC);
  return 0;
}
