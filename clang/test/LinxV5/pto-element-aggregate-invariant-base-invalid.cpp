// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DTWO_VARIANT -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=VARIANT
// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALIAS

// VARIANT: requires a scalar leaf GEP or one dynamic aggregate index with constant field suffixes
// ALIAS: memory streams may overlap across elements

#ifdef TWO_VARIANT
extern "C" void two_variant(int (*__restrict__ hist)[256],
                             const int *__restrict__ rows,
                             const int *__restrict__ bins) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    __atomic_fetch_add(&hist[rows[element]][bins[element]], 1,
                       __ATOMIC_RELAXED);
}
#else
extern "C" void alias_base(int (*hist)[256], unsigned row, const int *bins) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    __atomic_fetch_add(&hist[row][bins[element]], 1, __ATOMIC_RELAXED);
}
#endif
