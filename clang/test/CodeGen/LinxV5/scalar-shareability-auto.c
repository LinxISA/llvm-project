// RUN: %clang_cc1 -triple linx64v5 -O0 -emit-llvm -o - %s | FileCheck %s --check-prefixes=ALL,O0
// RUN: %clang_cc1 -triple linx64v5 -O2 -emit-llvm -o - %s | FileCheck %s --check-prefixes=ALL,OPT
// RUN: %clang_cc1 -triple linx64v5 -O3 -emit-llvm -o - %s | FileCheck %s --check-prefixes=ALL,OPT
// Ordinary pointers, including restrict parameters, use Private semantics.
// ALL-LABEL: define{{.*}} @automatic
// ALL-NOT: @llvm.linx.scalar
// O0: store i32 42
// O0: load i32
// OPT: store i32 42
// OPT-NEXT: ret i32 42
int automatic(int *restrict p) { *p = 42; return *p; }

// ALL-LABEL: define{{.*}} @local
// ALL-NOT: @llvm.linx.scalar
// ALL: store volatile i32
// ALL: load volatile i32
// ALL: ret i32
int local(int n) { volatile int v = n; return v; }

// Explicit initialization matches the IntraCore Tile request contract.
// ALL-LABEL: define{{.*}} @tile_scratch
// ALL: call void @llvm.linx.scalar.store({{.*}}i32 4, i32 1)
// ALL: asm sideeffect
// ALL: call i64 @llvm.linx.scalar.load({{.*}}i32 4, i32 1)
int tile_scratch(void) {
  int v;
  __builtin_linx_scalar_store(&v, 0, 4, 1);
  __asm__ volatile("" : : "r"(&v) : "memory");
  return __builtin_linx_scalar_load(&v, 4, 1);
}

// ALL-LABEL: define{{.*}} @fill
// ALL-NOT: @llvm.linx.scalar
// ALL: call void @llvm.memset
void fill(void *p, unsigned long n) { __builtin_memset(p, 255, n); }
