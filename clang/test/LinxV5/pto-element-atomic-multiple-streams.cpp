// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -Werror=unknown-pragmas -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s

extern "C" void two_streams(int *__restrict__ hist, int *__restrict__ num,
                            int *__restrict__ out, int *__restrict__ cand,
                            const int *__restrict__ bins,
                            const int *__restrict__ indices,
                            unsigned valid, int threshold) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element < valid) {
      const int bin = bins[element];
      if (bin > threshold) {
        const int slot = __atomic_fetch_add(hist, 1, __ATOMIC_RELAXED);
        out[slot] = indices[element];
      }
      if (bin == threshold) {
        const int slot = __atomic_fetch_add(num, 1, __ATOMIC_RELAXED);
        if (slot < 65536)
          cand[slot] = indices[element];
      }
    }
  }
}

// CHECK-LABEL: define {{.*}}@two_streams(
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
// CHECK-NOT: call void @llvm.linx.experimental.element.region

extern "C" void three_streams(int *__restrict__ counter0,
                              int *__restrict__ counter1,
                              int *__restrict__ counter2,
                              int *__restrict__ output0,
                              int *__restrict__ output1,
                              int *__restrict__ output2) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    int slot0 = __atomic_fetch_add(counter0, 1, __ATOMIC_RELAXED);
    output0[slot0] = element;
    int slot1 = __atomic_fetch_add(counter1, 1, __ATOMIC_RELAXED);
    output1[slot1] = element;
    int slot2 = __atomic_fetch_add(counter2, 1, __ATOMIC_RELAXED);
    output2[slot2] = element;
  }
}

// CHECK-LABEL: define {{.*}}@three_streams(
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked
// CHECK: call void @llvm.linx.experimental.ew.mscatter.gpr.masked
