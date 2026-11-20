// RUN: %clang_cc1 -triple linx64v5 -mlinx-runtime-intra-abi -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -mlinx-runtime-intra-abi -O2 -emit-obj -o %t.o %s
// RUN: %clang --target=linx64v5 -mcpu=janus -mlinx-runtime-intra-abi -O2 -c %s -o %t.driver.o
// Call-site contracts cover the runtime's constructors and external libc
// calls. The option is an ABI assertion by the library build, not inference.
extern void external(int *);
void invoke(void (*constructor)(void), int *p) {
  constructor();
  external(p);
}
// CHECK: call void %{{.*}}() #[[ABI:[0-9]+]]
// CHECK: call void @external(ptr {{.*}}) #[[ABI]]
// CHECK: attributes #[[ABI]] = { "linx-scalar-shareability"="intra" }
