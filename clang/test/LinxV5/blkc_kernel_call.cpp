// RUN: %clang++ --target=linx64 -mlxbc -O2 \
// RUN:   -emit-llvm -S -o - %s | FileCheck %s

using ftile_type = float tile_size(1024);
using itile_type = int tile_size(1024);

template <typename T>
void __attribute__((noinline)) __vec__ vadd(T __in__ A, T __in__ B, T __out__ C,
                                            int N) {
  __asm__ __volatile__ (
    "l.madd lc1.uh, lb0.uh, lc0.uh,  ->vu.h\n"  // index = i * col + j
    "l.lw   [TA, vu#1.sh<<2],  ->vt.w\n"        // src0[index]
    "l.lw   [TB, vu#1.sh<<2],  ->vt.w\n"        // src1[index]
    "l.fadd vt#2.fs, vt#1.fs,  ->vt.w\n"        // src0 + src1
    "l.sw   vt#1.sw, [TO, vu#1.sh<<2]\n"        // dst[index] = src0 + src1
    :
    :
    : "memory"
  );
}

void __attribute__((noinline)) __vec__ vadds(itile_type __in__ A,
                                             itile_type __in__ B,
                                             itile_type __out__ C, int N) {
  __asm__ __volatile__ (
    "l.madd lc1.uh, lb0.uh, lc0.uh,  ->vu.h\n"  // index = i * col + j
    "l.lw   [TA, vu#1.sh<<2],  ->vt.w\n"        // src0[index]
    "l.lw   [TB, vu#1.sh<<2],  ->vt.w\n"        // src1[index]
    "l.fadd vt#2.fs, vt#1.fs,  ->vt.w\n"        // src0 + src1
    "l.sw   vt#1.sw, [TO, vu#1.sh<<2]\n"        // dst[index] = src0 + src1
    :
    :
    : "memory"
  );
}

ftile_type A, B, C;

void test_blkc_template_kernel_call() {
  // CHECK: @llvm.linx.vcall.par.1d2u.v1024f32.v1024f32.v1024f32(ptr nonnull @_Z4vaddIDv1024_fEvT_S1_S1_i, i64 16, i64 16, i64 32, <1024 x float> %0, <1024 x float> %1, i32 100)
  vadd<ftile_type><<<16, 16, 32>>>(A, B, C, 100);
  // CHECK: @llvm.linx.vcall.par.1d2u.v1024f32.v1024f32.v1024f32(ptr nonnull @_Z4vaddIDv1024_fEvT_S1_S1_i, i64 16, i64 16, i64 1, <1024 x float> %3, <1024 x float> %4, i32 100)
  vadd<ftile_type><<<16, 16>>>(A, B, C, 100);
}

itile_type IA, IB, IC;
void test_blkc_kernel_call() {
  // CHECK: @llvm.linx.vcall.par.1d2u.v1024i32.v1024i32.v1024i32(ptr nonnull @_Z5vaddsDv1024_iS_S_i, i64 16, i64 16, i64 32, <1024 x i32> %0, <1024 x i32> %1, i32 100)
  // CHECK: @llvm.linx.vcall.par.1d2u.v1024i32.v1024i32.v1024i32(ptr nonnull @_Z5vaddsDv1024_iS_S_i, i64 16, i64 16, i64 1, <1024 x i32> %3, <1024 x i32> %4, i32 100)
  vadds<<<16, 16, 32>>>(IA, IB, IC, 100);
  vadds<<<16, 16>>>(IA, IB, IC, 100);
}