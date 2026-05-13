// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef __fp8_e4m3 te4m3 tile_size(1024);
typedef __fp8_e5m2 te5m2 tile_size(1024);

// CHECK: l.cvt.e5m22e4m3
// CHECK: l.cvt.e4m32e5m2
void __vec__ vcast(te4m3 __out__ out, te5m2 __in__ in) {
  auto *po = blkv_get_tile_ptr(out);
  auto *pi = blkv_get_tile_ptr(in);
  int i = blkv_get_index_x();
  po[i] = pi[i];
  int j = blkv_get_index_y();
  pi[j] = po[j];
}
