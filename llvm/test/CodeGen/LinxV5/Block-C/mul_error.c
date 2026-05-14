// RUN: clang++ %s --target=linx64 -S -mlxbc -O2 -o  - | FileCheck %s --check-prefixes=CHECK

// CHECK-LABEL: _Z6vshiftDv1024_f:
// CHECK: mul
using tile = float tile_size(1024);
void __vec__ vshift(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned short j = blkv_get_index_y();
  unsigned idx = i * j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}