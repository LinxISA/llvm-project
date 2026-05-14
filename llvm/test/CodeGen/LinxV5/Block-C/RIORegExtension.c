// RUN: clang++ %s --target=linx64 -S -mllvm -linxv5-enable-Reg-Extension-Opt=true -mlxbc -O2 -o  - | FileCheck %s --dump-input always -vv --check-prefixes=CHECK-OPEN
// RUN: clang++ %s --target=linx64 -S -mllvm -linxv5-enable-Reg-Extension-Opt=false -mlxbc -O2 -o  - | FileCheck %s --dump-input always -vv --check-prefixes=CHECK-CLOSE

// CHECK-OPEN-LABEL: _Z4vaddDv1024_fss:
// CHECK-OPEN: l.add   ri1.sh, ri0.sh,         ->t.d
// CHECK-OPEN: l.bxu   t#1.sd, 0, 32,  ->t.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE-LABEL: _Z4vaddDv1024_fss:
// CHECK-CLOSE: l.icvt.s162s64  ri0.sh,         ->t.d
// CHECK-CLOSE: l.icvt.s162s64  ri1.sh,         ->t.d
// CHECK-CLOSE: l.add   t#1.sd, t#2.sd,         ->t.d
// CHECK-CLOSE: l.bxu   t#1.sd, 0, 32,  ->t.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: l.sw.local    t#1.sw, [ta, t#2.sd<<2]
// CHECK-CLOSE: L.BSTOP
using tile = float tile_size(1024);
void __vec__ vadd(tile __in__ ta, signed short i, signed short j) {
  unsigned idx = i + j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN-LABEL: _Z4vmulDv1024_ft:
// CHECK-OPEN: l.mul   lc1.uh, ri0.uh,         ->t.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: l.sw.local    t#1.sw, [ta, t#2.sd<<2]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE-LABEL: _Z4vmulDv1024_ft:
// CHECK-CLOSE: l.icvt.u162u64  lc1.uh,         ->t.d
// CHECK-CLOSE: l.icvt.u162u64  ri0.uh,         ->t.d
// CHECK-CLOSE: l.mul   t#2.sd, t#1.sd,        ->t.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: l.sw.local    t#1.sw, [ta, t#2.sd<<2]
// CHECK-CLOSE: L.BSTOP
void __vec__ vmul(tile __in__ ta, unsigned short i) {
  unsigned j = blkv_get_index_y();
  unsigned idx = j * i;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN-LABEL: _Z6vshiftDv1024_ft:
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: l.sw.local      t#1.sw, [ta, ri0.uh<<7]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE-LABEL: _Z6vshiftDv1024_ft:
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: l.sw.local  t#1.sw, [ta, ri0.uh<<7]
// CHECK-CLOSE: L.BSTOP
void __vec__ vshift(tile __in__ ta, unsigned short i) {
  unsigned idx = i * 32;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN-LABEL: _Z5vaddiDv1024_ft:
// CHECK-OPEN: l.addi  ri0.uh, 8,      ->t.d
// CHECK-OPEN: l.mul   lc1.uh, t#1.sd,         ->t.d
// CHECK-OPEN: l.bxu   t#1.sd, 0, 32,         ->t.d
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: l.sw.local t#1.sw, [ta, t#2.sd<<2]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE-LABEL: _Z5vaddiDv1024_ft:
// CHECK-CLOSE: l.icvt.u162u64  ri0.uh,         ->t.d
// CHECK-CLOSE: l.addi  t#1.sd, 8,      ->t.d
// CHECK-CLOSE: l.icvt.u162u64  lc1.uh,         ->t.d
// CHECK-CLOSE: l.mul   t#2.sd, t#1.sd,        ->t.d
// CHECK-CLOSE: l.bxu   t#1.sd, 0, 32,         ->t.d
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: l.sw.local    t#1.sw, [ta, t#2.sd<<2]
// CHECK-CLOSE: L.BSTOP
void __vec__ vaddi(tile __in__ ta, unsigned short i) {
  unsigned short j = blkv_get_index_y();
  unsigned idx = (i + 8) * j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}

// CHECK-OPEN-LABEL: _Z10vadd_uh_uwDv1024_ftj:
// CHECK-OPEN: l.add   ri0.uh, ri1.uw,         ->t.w
// CHECK-OPEN: lui     260096,         ->t
// CHECK-OPEN: l.sw.local    t#1.sw, [ta, t#2.uw<<2]
// CHECK-OPEN: L.BSTOP

// CHECK-CLOSE-LABEL: _Z10vadd_uh_uwDv1024_ftj:
// CHECK-CLOSE: l.icvt.u162u32  ri0.uh,         ->t.w
// CHECK-CLOSE: l.add   t#1.sw, ri1.sw,         ->t.w
// CHECK-CLOSE: lui     260096,         ->t
// CHECK-CLOSE: l.sw.local    t#1.sw, [ta, t#2.uw<<2]
// CHECK-CLOSE: L.BSTOP
void __vec__ vadd_uh_uw(tile __in__ ta, unsigned short i, unsigned j) {
  unsigned idx = i + j;
  __vbuf__ float *pa = blkv_get_tile_ptr(ta);
  pa[idx] = 1.0;
}