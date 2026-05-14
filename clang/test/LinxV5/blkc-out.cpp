// RUN: %clang++ --target=linx64 -O2 -mlxbc -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef double tile tile_size(1024);

// CHECK: define linkonce_odr dso_local void
// CHECK:   @_Z4vfooIdEvDv1024_dPT_(
// CHECK:     <1024 x double> __out__ noundef %out,
template <class T> void __vec__ vfoo(tile __out__ out, T *p) {}

void foo(double *p) {
  tile out;
  vfoo<<<1, 1, 1>>>(out, p);
}
