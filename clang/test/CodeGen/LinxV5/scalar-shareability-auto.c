// RUN: %clang_cc1 -triple linx64v5 -O0 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -O2 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -O3 -emit-llvm -o - %s | FileCheck %s
// Ordinary pointers require no API annotations, even with restrict/noalias.
// CHECK-LABEL: define{{.*}} @automatic
// CHECK: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// CHECK: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
int automatic(int *restrict p) { *p = 42; return *p; }

// CHECK-LABEL: define{{.*}} @local
// CHECK-NOT: call {{.*}}@llvm.linx.scalar
// CHECK: store volatile i32
// CHECK: load volatile i32
// CHECK: ret i32
int local(int n) { volatile int v = n; return v; }

// CHECK-LABEL: define{{.*}} @tile_scratch
// CHECK: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// CHECK: asm sideeffect
// CHECK: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
int tile_scratch(void) {
  int v = 0;
  __asm__ volatile("" : : "r"(&v) : "memory");
  return v;
}

// CHECK-LABEL: define{{.*}} @fill
// CHECK-NOT: call {{.*}}@llvm.memset
// CHECK: call void @llvm.linx.scalar.store({{.*}}i32 1, i32 1)
void fill(void *p, unsigned long n) { __builtin_memset(p, 255, n); }
