// RUN: %clang -target linx64v5-unknown-linux-musl -O2 -c %s -o %t.o
// RUN: %clang -target linx64v5-unknown-linux-musl -O2 -S -emit-llvm %s -o - | FileCheck %s
// RUN: not %clang -target linx64v5-unknown-linux-musl -O2 -DRUNTIME -c %s -o %t.bad.o 2>&1 | FileCheck %s --check-prefix=BAD

struct Shape { unsigned long m, n, k; bool group; };
inline Shape resolve() { return {32, 32, 64, false}; }
inline Shape nested() { return resolve(); }
// CHECK-LABEL: define {{.*}}@constant_shape(
// CHECK-NOT: @llvm.linx.scalar
// CHECK: asm sideeffect "", "i,i"(i64 32, i64 64)
extern "C" void constant_shape() {
  auto shape = nested();
  asm volatile("" :: "i"(shape.n), "i"(shape.k));
}
#ifdef RUNTIME
// BAD: invalid operand for inline asm constraint 'i'
extern "C" void runtime_shape(unsigned long n) {
  asm volatile("" :: "i"(n));
}
#endif
