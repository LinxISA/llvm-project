// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DCOUNTER_ALIAS -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=COUNTER
// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DOUTPUT_ALIAS -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=OUTPUT
// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DNON_UNIT -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALLOCATOR
// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DTRUNC_INDEX -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=ALLOCATOR
// RUN: not %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -DCLOBBER_COUNTER -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=COUNTER

// COUNTER: multiple unordered streams require disjoint atomic counters
// OUTPUT: memory streams may overlap across elements
// ALLOCATOR: multiple unordered streams require direct unit-increment atomic slot allocation

#ifdef COUNTER_ALIAS
#define COUNTER_RESTRICT
#else
#define COUNTER_RESTRICT __restrict__
#endif
#ifdef OUTPUT_ALIAS
#define OUTPUT_RESTRICT
#else
#define OUTPUT_RESTRICT __restrict__
#endif

extern "C" void invalid_streams(int *COUNTER_RESTRICT counter0,
                                int *COUNTER_RESTRICT counter1,
                                int *OUTPUT_RESTRICT output0,
                                int *OUTPUT_RESTRICT output1) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
#ifdef NON_UNIT
    int slot0 = __atomic_fetch_add(counter0, 2, __ATOMIC_RELAXED);
#else
    int slot0 = __atomic_fetch_add(counter0, 1, __ATOMIC_RELAXED);
#endif
#ifdef TRUNC_INDEX
    slot0 = static_cast<unsigned short>(slot0);
#endif
    output0[slot0] = element;
#ifdef CLOBBER_COUNTER
    __atomic_fetch_add(counter0, -1, __ATOMIC_RELAXED);
#endif
    int slot1 = __atomic_fetch_add(counter1, 1, __ATOMIC_RELAXED);
    output1[slot1] = element;
  }
}
