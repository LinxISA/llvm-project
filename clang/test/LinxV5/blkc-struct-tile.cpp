// RUN: %clang++ --target=linx64v5 -O2 -mlxbc -S -emit-llvm -o - %s \
// RUN:   | FileCheck %s --dump-input always -vv

typedef struct __test_fp8 {
  char data;
} __test_fp8;

typedef __test_fp8 tile tile_size(1024);

// clang-format off
// CHECK: define dso_local void @_Z4vfooDv1024_10__test_fp8Pd(<1024 x i8> __out__ %out,
// CHECK: %0 = tail call ptr addrspace(6) @llvm.blkv.get.tile.ptr.p6.v1024i8(<1024 x i8> %out)
void __vec__ vfoo(tile __out__ out, double *p) {
  __vbuf__ __test_fp8  *po = blkv_get_tile_ptr(out);
  p[0] = po[0].data;
}
