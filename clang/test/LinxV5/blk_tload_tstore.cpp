// RUN: %clang++ --target=linx64 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s --dump-input always -vv

using TileDType = float tile_size(256);
extern void __mtc__ copyin(TileDType __out__ out, float *p);

// CHECK-LABEL: entry
// CHECK:      %0 = tail call <256 x float> @llvm.linx.blk.tload.v256f32(i64 16, i64 16, i64 16, i64 1, i64 78, i64 3, ptr %p, i64 4)
// CHECK-NEXT: tail call void @llvm.linx.blk.tstore.v256f32(i64 16, i64 16, i64 16, i64 1, i64 3, ptr %p, i64 4, <256 x float> %0
void test( float *p) {
  TileDType TO;
  blk_tload(16, 16, 16, 1, 78, 3, TO, p, 4);
  blk_tstore(16, 16, 16, 1, 3, p, 4, TO);
}