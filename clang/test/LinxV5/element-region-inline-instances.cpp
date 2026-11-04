// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o

__attribute__((always_inline)) inline void phase(unsigned *data) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    data[element] += 1;
}

extern "C" void divergent(unsigned *left, unsigned *right, bool flag) {
  if (flag)
    phase(left);
  else
    phase(right);
}

// CHECK-LABEL: define{{.*}} @divergent(
// CHECK-NOT: @llvm.linx.experimental.element.region
// CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked
// CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked
// CHECK: ret void

extern "C" void sequential(unsigned *data) {
  phase(data);
  phase(data);
}

// CHECK-LABEL: define{{.*}} @sequential(
// CHECK-NOT: @llvm.linx.experimental.element.region
// CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked
// CHECK: @llvm.linx.experimental.ew.tbinary.gpr.masked
// CHECK: ret void
