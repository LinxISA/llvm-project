// RUN: %clang --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s
// RUN: %clang++ -x c++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s | FileCheck %s

typedef unsigned int tile_size(32) tile32;

void pto_histogram(tile32 *old_values, unsigned int *hist,
                   const tile32 *indices, unsigned int valid_elements) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element < valid_elements)
      (*old_values)[element] = __atomic_fetch_add(
          &hist[(*indices)[element]], 1u, __ATOMIC_RELAXED);
    else
      (*old_values)[element] = 0;
  }
}

void legacy_histogram(tile32 *old_values, unsigned int *hist,
                      const tile32 *indices, unsigned int valid_elements) {
#pragma linx elementwise
  for (unsigned element = 0; element < 32; ++element) {
    if (element < valid_elements)
      (*old_values)[element] = __atomic_fetch_add(
          &hist[(*indices)[element]], 1u, __ATOMIC_RELAXED);
    else
      (*old_values)[element] = 0;
  }
}

// CHECK-LABEL: define{{.*}} @{{.*}}pto_histogram
// CHECK: call <32 x i32> @llvm.linx.experimental.ew.tci
// CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea
// CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
// CHECK-NOT: atomicrmw
// CHECK-LABEL: define{{.*}} @{{.*}}legacy_histogram
// CHECK: call <32 x i32> @llvm.linx.experimental.ew.tci
// CHECK: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// CHECK: call <32 x i64> @llvm.linx.experimental.ew.tlea
// CHECK: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
// CHECK-NOT: atomicrmw
