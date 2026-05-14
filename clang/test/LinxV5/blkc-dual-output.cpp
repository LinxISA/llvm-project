// RUN: %clang --target=linx64 -O2 -mlxbc -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef double tile1 tile_size(1024);
typedef double tile2 tile_size(512);

void __mtc__ vbar1(tile1 __in__ a, double *p);
void __mtc__ vbar2(tile2 __in__ a, double *p);

void __vec__ vfoo(tile1 __out__ out0, tile2 __out__ out1, double *p);
void __vec__ vfoo1(tile1 __out__ out0, tile2 __out__ out1, tile1 __in__ in0, double *p);
void __vec__ vfoo2(tile1 __out__ out0, tile2 __out__ out1, tile1 __in__ in0, tile2 __in__ in1, double *p);
void __vec__ vfoo3(tile1 __out__ out0, tile2 __out__ out1, tile1 __in__ in0, tile2 __in__ in1, tile2 __in__ in2, double *p);

// clang-format off
// CHECK-LABEL: _Z3fooPd
// CHECK: %0 = tail call { <1024 x double>, <512 x double> } (ptr, i64, i64, i64, ...) @llvm.linx.vcall.par.2d0u
// CHECK-NEXT: %1 = extractvalue { <1024 x double>, <512 x double> } %0, 0
// CHECK-NEXT: %2 = extractvalue { <1024 x double>, <512 x double> } %0, 1
// CHECK-NEXT: tail call void (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.mcall.par.0d1u.v1024f64(ptr nonnull @_Z5vbar1Dv1024_dPd, i64 1, i64 1, i64 1, <1024 x double> %1, ptr %p)
// CHECK-NEXT: tail call void (ptr, i64, i64, i64, <512 x double>, ...) @llvm.linx.mcall.par.0d1u.v512f64(ptr nonnull @_Z5vbar2Dv512_dPd, i64 1, i64 1, i64 1, <512 x double> %2, ptr %p)
// clang-format on
void foo(double *p) {
  tile1 a, ao;
  tile2 b, bo;
  vfoo<<<1, 1, 1>>>(a, b, p);
  vbar1<<<1, 1, 1>>>(a, p);
  vbar2<<<1, 1, 1>>>(b, p);
}

// clang-format off
// CHECK-LABEL: _Z4foo1Pd
// CHECK: %0 = tail call { <1024 x double>, <512 x double> } (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.vcall.par.2d1u
// clang-format on
void foo1(double *p) {
  tile1 a;
  tile2 b;
  tile1 in0;
  vbar1<<<1, 1, 1>>>(in0, p);
  vfoo1<<<1, 1, 1>>>(a, b, in0, p);
}

// clang-format off
// CHECK-LABEL: _Z4foo2Pd
// CHECK: %0 = tail call { <1024 x double>, <512 x double> } (ptr, i64, i64, i64, <1024 x double>, <512 x double>, ...) @llvm.linx.vcall.par.2d2u
// clang-format on
void foo2(double *p) {
  tile1 a;
  tile2 b;
  tile1 in0;
  tile2 in1;
  vbar1<<<1, 1, 1>>>(in0, p);
  vbar2<<<1, 1, 1>>>(in1, p);
  vfoo2<<<1, 1, 1>>>(a, b, in0, in1, p);
}

// clang-format off
// CHECK-LABEL: _Z4foo3Pd
// CHECK: %0 = tail call { <1024 x double>, <512 x double> } (ptr, i64, i64, i64, <1024 x double>, <512 x double>, <512 x double>, ...) @llvm.linx.vcall.par.2d3u
// clang-format on
void foo3(double *p) {
  tile1 a;
  tile2 b;
  tile1 in0;
  tile2 in1;
  tile2 in2;
  vbar1<<<1, 1, 1>>>(in0, p);
  vbar2<<<1, 1, 1>>>(in1, p);
  vbar2<<<1, 1, 1>>>(in2, p);
  vfoo3<<<1, 1, 1>>>(a, b, in0, in1, in2, p);
}