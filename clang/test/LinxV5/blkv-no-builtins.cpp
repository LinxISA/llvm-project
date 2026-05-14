// RUN: %clang++ --target=linx64 -O2 -mlxbc -emit-llvm -S -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef __fp8_e5m2 te5m2 tile_size(1024);

// CHECK-NOT: @llvm.memset
void __vec__ vloop(te5m2 __in__ in, __fp8_e5m2 max) {
  auto *pi = blkv_get_tile_ptr(in);
  int i = blkv_get_index_x();
  for (unsigned id = 0; id < 32; ++id) {
    unsigned idx = i * 32 + id * 1;
    pi[idx] = max;
  }
}
