// RUN: %clang_cc1 -triple linx64v5 -mlinx-runtime-intra-abi -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -mlinx-runtime-intra-abi -O2 -emit-obj -o %t.o %s
// RUN: %clang --target=linx64v5 -mcpu=janus -mlinx-runtime-intra-abi -O2 -c %s -o %t.driver.o
// The compatibility flag leaves ordinary external and indirect calls Private.
extern void external(int *);
void invoke(void (*constructor)(void), int *p) {
  constructor();
  external(p);
}
// CHECK: call void %{{.*}}()
// CHECK: call void @external(ptr {{.*}})
// CHECK-NOT: "linx-scalar-shareability"="intra"
// CHECK-NOT: "linx-runtime-intra-abi"
