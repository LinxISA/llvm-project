// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -Werror=unknown-pragmas -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s

__attribute__((always_inline)) inline void bump(int *__restrict__ hist,
    const int *__restrict__ bins, unsigned valid, int threshold) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    if (element < valid && bins[element] > threshold)
      __atomic_fetch_add(&hist[bins[element]], 1, __ATOMIC_RELAXED);
}

extern "C" void helper_twice(int *__restrict__ hist0, int *__restrict__ hist1,
    const int *__restrict__ bins0, const int *__restrict__ bins1,
    unsigned valid, int threshold) {
  bump(hist0, bins0, valid, threshold);
  bump(hist1, bins1, valid, threshold);
}

// CHECK-LABEL: define {{.*}}@helper_twice(
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK-NOT: call void @llvm.linx.experimental.element.region

extern "C" void helper_conditional(int *__restrict__ hist0,
    int *__restrict__ hist1, const int *__restrict__ bins0,
    const int *__restrict__ bins1, unsigned valid, int threshold, bool enabled) {
  if (enabled)
    bump(hist0, bins0, valid, threshold);
  else
    bump(hist1, bins1, valid, threshold);
  bump(hist0, bins0, valid, threshold);
}

// CHECK-LABEL: define {{.*}}@helper_conditional(
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK-NOT: call void @llvm.linx.experimental.element.region
