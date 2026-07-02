// RUN: %clang_cc1 -triple linx64-linx-none-elf -emit-llvm -o - %s | FileCheck %s

void test_vblock_launch(void) {
  __builtin_linx_vblock_launch(0u, (void const *)0, 3ull, 4ull, 5ull, 7u);
}

// CHECK-LABEL: define{{.*}} void @test_vblock_launch
// CHECK: call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 3, i64 4, i64 5, i32 7, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)

typedef int tile_i32 __attribute__((vector_size(4096)));

tile_i32 test_tile_tload(void const *base) {
  return __builtin_linx_tile_tload(base, 8u, 1u, 0ll, 8ll, 8ll, 0ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tile_tload
// CHECK: call <1024 x i32> @llvm.linx.tma.tload.desc

void test_tile_tstore(void *base, tile_i32 tile) {
  __builtin_linx_tile_tstore(base, tile, 8u, 1u, 0ll, 8ll, 8ll, 0ll);
}

// CHECK-LABEL: define{{.*}} void @test_tile_tstore
// CHECK: call void @llvm.linx.tma.tstore.desc

tile_i32 test_tile_tmov(tile_i32 src) {
  return __builtin_linx_tile_tmov(src, 0u, 8u, 1u, 0ll, 0u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tile_tmov
// CHECK: call <1024 x i32> @llvm.linx.tile.tmov.legacy

tile_i32 test_cube_mamulb(tile_i32 a, tile_i32 b) {
  return __builtin_linx_cube_mamulb(a, b, 4u, 4u, 4u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_cube_mamulb
// CHECK: call <1024 x i32> @llvm.linx.cube.mamulb.legacy

tile_i32 test_cube_mamulb_acc(tile_i32 acc, tile_i32 a, tile_i32 b) {
  return __builtin_linx_cube_mamulb_acc(acc, a, b, 4u, 4u, 4u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_cube_mamulb_acc
// CHECK: call <1024 x i32> @llvm.linx.cube.mamulb.acc.legacy

tile_i32 test_cube_acccvt(tile_i32 acc) {
  return __builtin_linx_cube_acccvt(acc, 8u, 1u, 0ll, 0ll);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_cube_acccvt
// CHECK: call <1024 x i32> @llvm.linx.cube.acccvt.legacy

tile_i32 test_tepl_unary(tile_i32 src) {
  return __builtin_linx_tepl_unary(src, 32u, 8u, 1u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tepl_unary
// CHECK: call <1024 x i32> @llvm.linx.tepl.unary.legacy

tile_i32 test_tepl_binary(tile_i32 a, tile_i32 b) {
  return __builtin_linx_tepl_binary(a, b, 0u, 8u, 1u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tepl_binary
// CHECK: call <1024 x i32> @llvm.linx.tepl.binary.legacy

tile_i32 test_tepl_binary_scalar(tile_i32 a) {
  return __builtin_linx_tepl_binary_scalar(a, 9ll, 0u, 8u, 1u, 1u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tepl_binary_scalar
// CHECK: call <1024 x i32> @llvm.linx.tepl.binary.scalar.legacy

tile_i32 test_tepl_splat(void) {
  return __builtin_linx_tepl_splat(11ll, 0u, 8u, 1u, 2u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tepl_splat
// CHECK: call <1024 x i32> @llvm.linx.tepl.splat.legacy

tile_i32 test_vpar_tadd(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tadd(a, b, 8u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tadd
// CHECK: call <1024 x i32> @llvm.linx.vpar.tadd.legacy

tile_i32 test_vpar_tsub(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tsub(a, b, 8u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tsub
// CHECK: call <1024 x i32> @llvm.linx.vpar.tsub.legacy
