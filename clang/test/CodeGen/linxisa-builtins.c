// RUN: %clang_cc1 -triple linx64-linx-none-elf -emit-llvm -o - %s | FileCheck %s

void test_vblock_launch(void) {
  __builtin_linx_vblock_launch(0u, (void const *)0, 3ull, 4ull, 5ull, 7u);
}

// CHECK-LABEL: define{{.*}} void @test_vblock_launch
// CHECK: call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 3, i64 4, i64 5, i32 7, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)

typedef int tile_i32 __attribute__((vector_size(4096)));

tile_i32 test_tile_tload(void const *base) {
  return __builtin_linx_tile_tload(base, 6u, 1u, 0ll, 8ll, 8ll, 8ll, 1ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tile_tload
// CHECK: call <1024 x i32> @llvm.linx.tlsu.tload.shape

void test_tile_tstore(void *base, tile_i32 tile) {
  __builtin_linx_tile_tstore(base, tile, 6u, 1u, 0ll, 8ll, 8ll, 8ll, 1ll);
}

// CHECK-LABEL: define{{.*}} void @test_tile_tstore
// CHECK: call void @llvm.linx.tlsu.tstore.shape

tile_i32 test_tile_tmov(tile_i32 src) {
  return __builtin_linx_tile_tmov(src, 6u, 1u, 0ll, 0u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tile_tmov
// CHECK: call <1024 x i32> @llvm.linx.tile.tmov.vec

tile_i32 test_cube_tmatmul(tile_i32 a, tile_i32 b) {
  return __builtin_linx_cube_tmatmul(a, b, 4u, 4u, 4u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_cube_tmatmul
// CHECK: call <1024 x i32> @llvm.linx.cube.tmatmul.vec

tile_i32 test_cube_tmatmul_acc(tile_i32 acc, tile_i32 a, tile_i32 b) {
  return __builtin_linx_cube_tmatmul_acc(acc, a, b, 4u, 4u, 4u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_cube_tmatmul_acc
// CHECK: call <1024 x i32> @llvm.linx.cube.tmatmul.acc.vec

tile_i32 test_tileop_unary(tile_i32 src) {
  return __builtin_linx_tileop_unary(src, 32u, 6u, 1u, 8ll, 8ll, 8ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tileop_unary
// CHECK: call <1024 x i32> @llvm.linx.tileop.unary.shape

tile_i32 test_tileop_binary(tile_i32 a, tile_i32 b) {
  return __builtin_linx_tileop_binary(a, b, 0u, 6u, 1u, 8ll, 8ll, 8ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tileop_binary
// CHECK: call <1024 x i32> @llvm.linx.tileop.binary.shape

tile_i32 test_tileop_binary_scalar(tile_i32 a) {
  return __builtin_linx_tileop_binary_scalar(a, 9ll, 0u, 6u, 1u, 1u, 8ll,
                                              8ll, 8ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tileop_binary_scalar
// CHECK: call <1024 x i32> @llvm.linx.tileop.binary.scalar.shape

tile_i32 test_tileop_splat(void) {
  return __builtin_linx_tileop_splat(11ll, 0u, 6u, 1u, 2u, 8ll, 8ll, 8ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tileop_splat
// CHECK: call <1024 x i32> @llvm.linx.tileop.splat.shape

tile_i32 test_vpar_tadd(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tadd(a, b, 6u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tadd
// CHECK: call <1024 x i32> @llvm.linx.vpar.tadd.vec

tile_i32 test_vpar_tsub(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tsub(a, b, 6u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tsub
// CHECK: call <1024 x i32> @llvm.linx.vpar.tsub.vec
