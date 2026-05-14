
// RUN: %clang++ --target=linx64 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv


// CHECK-LABEL: entry
// CHECK: tail call void (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.0d0u(ptr nonnull @_Z5vbar1Pd, i64 1, i64 1, i64 1, ptr %p)
void __mtc__ vbar1(double *p);

void foo(double *p) {
  vbar1<<<1, 1, 1>>>(p);
}
