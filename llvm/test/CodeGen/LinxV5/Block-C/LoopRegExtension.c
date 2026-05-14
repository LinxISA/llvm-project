// RUN: clang++ %s --target=linx64v5 -S -mllvm -linxv5-enable-Reg-Extension-Opt=true -mlxbc -O2 -o  - | FileCheck %s --check-prefixes=CHECK-OPEN
// RUN: clang++ %s --target=linx64v5 -S -mllvm -linxv5-enable-Reg-Extension-Opt=false -mlxbc -O2 -o  - | FileCheck %s --check-prefixes=CHECK-CLOSE

// CHECK-OPEN: _Z6vshiftDv1024_f:
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: v.sw.local  t#1.sw, [ta, lc0.uh<<7]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: v.sw.local      t#1.sw, [ta, lc0.uh<<7]
// CHECK-CLOSE: L.BSTOP
using tile = float tile_size(1024);
void __vec__ vshift(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned idx = i * 32;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN: _Z5vaddiDv1024_f:
// CHECK-OPEN: v.addi  lc0.uh, 8,      ->vt.d
// CHECK-OPEN: v.mul   lc1.uh, vt#1.sd,        ->vt.d
// CHECK-OPEN: v.bxu   vt#1.sd, 0, 32,         ->vt.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: v.sw.local    t#1.sw, [ta, vt#1.sd<<2]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE: v.icvt.u162u64  lc0.uh,         ->vt.d
// CHECK-CLOSE: v.addi  vt#1.sd, 8,     ->vt.d
// CHECK-CLOSE: l.icvt.u162u64  lc1.uh,         ->t.d
// CHECK-CLOSE: v.mul   vt#1.sd, t#1.sd,       ->vt.d
// CHECK-CLOSE: v.bxu   vt#1.sd, 0, 32,         ->vt.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: v.sw.local    t#1.sw, [ta, vt#1.sd<<2]
// CHECK-CLOSE: L.BSTOP
void __vec__ vaddi(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned short j = blkv_get_index_y();
  unsigned idx = (i + 8) * j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN: _Z4vmulDv1024_f:
// CHECK-OPEN: v.mul   lc1.uh, lc0.uh,         ->vt.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: v.sw.local    t#1.sw, [ta, vt#1.sd<<2]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE: v.icvt.u162u64  lc0.uh,         ->vt.d
// CHECK-CLOSE: l.icvt.u162u64  lc1.uh,         ->t.d
// CHECK-CLOSE: v.mul   t#1.sd, vt#1.sd,       ->vt.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: v.sw.local    t#1.sw, [ta, vt#1.sd<<2]
// CHECK-CLOSE: L.BSTOP
void __vec__ vmul(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned short j = blkv_get_index_y();
  unsigned idx = i * j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN: _Z4vfooDv1024_f:
// CHECK-OPEN: v.add   ta, lc0.uh<<7,  ->vt.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: v.swi.local   t#1.sw, [vt#1.sd, 68]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE: v.icvt.u162u64  lc0.uh,         ->vt.d
// CHECK-CLOSE: v.add ta, vt#1.sd<<7,         ->vt.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: v.swi.local   t#1.sw, [vt#1.sd, 68]
// CHECK-CLOSE: L.BSTOP
void __vec__ vfoo(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned idx = i * 32 + 17;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN: _Z5vfoo2Dv1024_f:
// CHECK-OPEN: v.slli  lc0.sh, 5,      ->vt.h
// CHECK-OPEN: v.icvt.u162u64  vt#1.uh,        ->vt.d
// CHECK-OPEN: v.add   ta, vt#1.sd<<2,         ->vt.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: v.swi.local   t#1.sw, [vt#1.sd, 68]
// CHECK-OPEN: L.BSTOP
void __vec__ vfoo2(tile __in__ ta) {
  unsigned short i = blkv_get_index_x();
  unsigned short idx = i * 32 + 17;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}
