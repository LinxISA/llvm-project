// RUN: %clang++ --target=linx64 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

typedef double tile tile_size(1024);

template<const int l>
extern void __vec__ tadd(tile __out__ out, tile __in__ in1, tile __in__ in2);
extern void __mtc__ copyin(tile __out__ out, double *p);
extern void __mtc__ copyout(tile __in__ in, double *p);

// CHECK: _Z11tile_callerPdS_S_(ptr noundef %p1, ptr noundef %p2, ptr noundef %p3)
// CHECK-LABEL: entry
// CHECK-NEXT: %0 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinDv1024_dPd, i64 1, i64 1, i64 1, ptr %p1)
// CHECK-NEXT: %1 = tail call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z6copyinDv1024_dPd, i64 1, i64 1, i64 1, ptr %p2)
// CHECK-NEXT: %2 = tail call <1024 x double> (ptr, i64, i64, i64, <1024 x double>, <1024 x double>, ...) @llvm.linx.vcall.par.1d2u.v1024f64.v1024f64.v1024f64(ptr nonnull @_Z4taddILi1EEvDv1024_dS0_S0_, i64 1, i64 1, i64 1, <1024 x double> %0, <1024 x double> %1)
// CHECK-NEXT: tail call void (ptr, i64, i64, i64, <1024 x double>, ...) @llvm.linx.mcall.par.0d1u.v1024f64(ptr nonnull @_Z7copyoutDv1024_dPd, i64 1, i64 1, i64 1, <1024 x double> %2, ptr %p3)
void tile_caller(double *p1, double *p2, double *p3) {
  tile out;
  tile in1;
  tile in2;
  __linx_vcall_par(copyin, 1, 1, 1, in1, p1);
  __linx_vcall_par(copyin, 1, 1, 1, in2, p2);
  __linx_vcall_par(tadd<1>, 1, 1, 1, out, in1, in2);
  __linx_vcall_par(copyout, 1, 1, 1, out, p3);
}
