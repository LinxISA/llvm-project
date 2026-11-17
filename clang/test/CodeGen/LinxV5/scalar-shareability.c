// RUN: %clang_cc1 -triple linx64v5 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -O2 -emit-llvm -o - %s | FileCheck %s
unsigned long long read_count(const void *p) {
  // CHECK: call i64 @llvm.linx.scalar.load(ptr {{.*}}, i32 4, i32 1)
  return __builtin_linx_scalar_load(p, 4, 1);
}
void initialize(void *p, unsigned long long v) {
  // CHECK: call void @llvm.linx.scalar.store(ptr {{.*}}, i64 {{.*}}, i32 8, i32 1)
  __builtin_linx_scalar_store(p, v, 8, 1);
}
unsigned long long add(void *p, unsigned long long v) {
  // CHECK: call i64 @llvm.linx.scalar.atomic(ptr {{.*}}, i64 {{.*}}, i64 0, i32 8, i32 2, i32 3, i32 0)
  return __builtin_linx_scalar_atomic(p, v, 0, 8, 2, 3, 0);
}
// CHECK: define {{.*}}@ordinary
// CHECK: "linx-scalar-shareability"="intra"
__attribute__((linx_scalar_shareability("intra")))
int ordinary(int *p) { *p = 7; return *p; }
