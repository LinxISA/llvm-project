// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef double tile tile_size(1024);

// CHECK: %0 = tail call ptr addrspace(6)
// CHECK:    @llvm.blkv.get.tile.ptr.p6.v1024f64(<1024 x double> %out)
// CHECK-NEXT: %1 = tail call i16 @llvm.blkv.get.index.x()
void __vec__ vfoo(tile __out__ out, double *p) {
  __vbuf__ double *pout = blkv_get_tile_ptr(out);
  short x = blkv_get_index_x();
  p[x] = pout[x];
}
