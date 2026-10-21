// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -Werror=unknown-pragmas -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -O2 -std=c++20 -fno-vectorize -fno-slp-vectorize -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s

int hist[4][256];

extern "C" void aggregate_base(unsigned row, const int *__restrict__ bins) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    __atomic_fetch_add(&hist[row][bins[element]], 1, __ATOMIC_RELAXED);
}

// CHECK-LABEL: define {{.*}}@aggregate_base(
// CHECK: pto.aggregate.base = getelementptr [4 x [256 x i32]], ptr @hist, i64 0, i64
// CHECK: [[BASE:%.*]] = freeze ptr %pto.aggregate.base
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked{{.*}}ptr [[BASE]],

struct Bucket {
  int padding;
  int values[256];
};

extern "C" void aggregate_field(Bucket *__restrict__ buckets, long row,
                                const int *__restrict__ bins) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    __atomic_fetch_add(&buckets[row].values[bins[element]], 1,
                       __ATOMIC_RELAXED);
}

// CHECK-LABEL: define {{.*}}@aggregate_field(
// CHECK: pto.aggregate.base = getelementptr %struct.Bucket, ptr {{.*}}, i64 {{.*}}, i32 1
// CHECK: [[FIELD_BASE:%.*]] = freeze ptr %pto.aggregate.base
// CHECK: call {{.*}}@llvm.linx.experimental.ew.mgather.add.masked{{.*}}ptr [[FIELD_BASE]],
