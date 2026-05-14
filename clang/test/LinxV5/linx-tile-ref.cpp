// RUN: %clang++ --target=linx64v5 -std=c++20 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

typedef double tile tile_size(1024);

extern void __mtc__ copyinleft(tile __out__ out, double &p);
extern void __mtc__ copyinright(tile __out__ out, double *p);

extern void __mtc__ copyout(tile __out__ out, double &&p);

static inline void Tcopyout(tile &out) { copyout<<<1, 1, 1>>>(out, 3.14); }

static inline void Tcopyinleft(tile &out, double &p) {
  copyinleft<<<1,1,1>>>(out, p);
}

static inline void Tcopyinright(tile &out, double *p) {
  copyinright<<<1,1, 1>>>(out, p);
}

static double global_buf[256];
double* getBuf() { return global_buf; }

// Left-value/Right-value reference handling logic: EmitLValue (alloca + load)
// Right-value temporary pointer handling logic: EmitScalarExpr

// CHECK-LABEL: define dso_local void @_Z6callerPd(ptr noundef %ptr)
// CHECK: entry:
// CHECK:   %x = alloca double, align 8
// CHECK: %0 = load double, ptr %ptr, align 8, !tbaa !4
// CHECK-NEXT: store double %0, ptr %x, align 8, !tbaa !4
// CHECK-NEXT: %1 = call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z10copyinleftDv1024_dRd, i64 1, i64 1, i64 1, ptr nonnull %x)
// CHECK-NEXT: %2 = call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z11copyinrightDv1024_dPd, i64 1, i64 1, i64 1, ptr nonnull @_ZL10global_buf)
// CHECK-NEXT: %add.ptr = getelementptr inbounds double, ptr %ptr, i64 1
// CHECK-NEXT: %3 = call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z11copyinrightDv1024_dPd, i64 1, i64 1, i64 1, ptr nonnull %add.ptr)
// CHECK:  store double 3.140000e+00, ptr %ref.tmp.i, align 8, !tbaa !4
// CHECK-NEXT: %4 = call <1024 x double> (ptr, i64, i64, i64, ...) @llvm.linx.mcall.par.1d0u.v1024f64(ptr nonnull @_Z7copyoutDv1024_dOd, i64 1, i64 1, i64 1, ptr nonnull %ref.tmp.i)

void caller(double *ptr) {
  tile t;
  double x = *ptr;

  Tcopyinleft(t, x);

  Tcopyinright(t, getBuf());

  Tcopyinright(t, ptr + 1);

  Tcopyout(t);
}
