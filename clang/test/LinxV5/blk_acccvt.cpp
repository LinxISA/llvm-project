// clang-format off
// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

using tile = float tile_size(256);

// CHECK-LABEL: entry
// CHECK:      %0 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 4, ptr %pa, i64 16)
// CHECK-NEXT: %1 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 1, i64 1, i64 3, i64 3, ptr %pb, i64 16)
// CHECK-NEXT: %2 = tail call <256 x float> @llvm.linx.blk.matmul.v256f32.v256f32.v256f32(i64 16, i64 16, i64 16, i64 1, <256 x float> %0, <256 x float> %1)
// CHECK-NEXT: %3 = tail call <256 x float> @llvm.linx.blk.acccvt.v256f32.v256f32(i64 16, i64 16, i64 1, i64 27, i64 1, <256 x float> %2)
// CHECK-NEXT: tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 1, i64 1, i64 0, ptr %pc, i64 16, <256 x float> %3)
void test(float *pa, float *pb, float *pc) {
  constexpr int M = 16;
  constexpr int N = 16;
  constexpr int K = 16;
  constexpr int TY_FLOAT = 1;
  constexpr int PADNull = 3;
  constexpr int ND2NZ = 4;
  constexpr int ND2ZN = 3;
  constexpr int NZ2ND = 27;
  constexpr int NORM = 0;
  tile A;
  tile B;
  tile C;
  tile O;
  blk_tload(K, M, 1, TY_FLOAT, PADNull, ND2NZ, A, pa, K);
  blk_tload(N, K, 1, TY_FLOAT, PADNull, ND2ZN, B, pb, N);
  blk_matmul(M, N, K, TY_FLOAT, C, A, B);
  blk_acccvt(M, N, TY_FLOAT, NZ2ND, true, O, C);
  blk_tstore(N, M, 1, TY_FLOAT, NORM, pc, N, O);
}