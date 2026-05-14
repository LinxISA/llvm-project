// RUN: %clang++ --target=linx64 -O2 -mlxbc -S -emit-llvm \
// RUN:   -o - %s | FileCheck %s --dump-input always -vv

typedef double tile tile_size(1024);

// CHECK: attributes{{.*}}"__vec__"
template <class T> extern void __vec__ vfoo(tile __out__ out, T *p);

// CHECK: attributes{{.*}}"__mtc__"
template <class T> extern void __mtc__ mfoo(tile __out__ out, T *p);

void foo(double *p) {
  tile out;
  vfoo<<<1, 1, 16>>>(out, p);
  mfoo<<<1, 1, 16>>>(out, p);
}
