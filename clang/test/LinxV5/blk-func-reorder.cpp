// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -fenable-matrix \
// RUN:   -S -emit-llvm -o - %s | FileCheck %s --dump-input always -vv

typedef double tile  tile_size(1024);

extern void __vec__ vfoo(double *p, tile __in__ in);

extern void __mtc__ mfoo(tile __out__ out, double *p);

// clang-format off
// CHECK: tail call void (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.vcall.par.0d1u.v1024f64(ptr nonnull @_Z4vfooPdDv1024_d, i64 1, i64 1, i64 16, <1024 x double> %0, ptr %p)
void foo(double *p) {
  tile out;
  mfoo<<<1, 1, 16>>>(out, p);
  vfoo<<<1, 1, 16>>>(p, out);
}
