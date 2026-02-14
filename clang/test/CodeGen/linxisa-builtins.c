// RUN: %clang_cc1 -triple linx64-linx-none-elf -emit-llvm -o - %s | FileCheck %s

void test_vblock_launch(void) {
  __builtin_linx_vblock_launch(0u, (void const *)0, 3ull, 4ull, 5ull, 7u);
}

// CHECK-LABEL: define{{.*}} void @test_vblock_launch
// CHECK: call void @llvm.linx.vblock.launch(i32 0, ptr null, i64 3, i64 4, i64 5, i32 7)

typedef int tile_i32 __attribute__((vector_size(4096)));

tile_i32 test_vpar_tadd(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tadd(a, b, 8u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tadd
// CHECK: call <1024 x i32> @llvm.linx.vpar.tadd

tile_i32 test_vpar_tsub(tile_i32 a, tile_i32 b) {
  return __builtin_linx_vpar_tsub(a, b, 8u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_vpar_tsub
// CHECK: call <1024 x i32> @llvm.linx.vpar.tsub

tile_i32 test_tma_tload_desc(void const *base) {
  return __builtin_linx_tma_tload_desc(base, 3u, 64u, 64u, 64u, 8u);
}

// CHECK-LABEL: define{{.*}} <1024 x i32> @test_tma_tload_desc
// CHECK: call <1024 x i32> @llvm.linx.tma.tload.desc{{.*}}(ptr %{{.*}}, i32 3, i32 64, i32 64, i32 64, i32 8)

void test_tma_tstore_desc(void *base, tile_i32 t) {
  __builtin_linx_tma_tstore_desc(base, t, 2u, 128u, 64u, 32u, 8u);
}

// CHECK-LABEL: define{{.*}} void @test_tma_tstore_desc
// CHECK: call void @llvm.linx.tma.tstore.desc{{.*}}(ptr %{{.*}}, <1024 x i32> %{{.*}}, i32 2, i32 128, i32 64, i32 32, i32 8)
