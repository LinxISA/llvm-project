// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREJECT_SHAPE -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREJECT_DIV -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DREJECT_CONTRACT -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -ffast-math -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT

using f32 = float;
using u32 = unsigned;
using matrix = f32 tile_size(512);
using row = f32 tile_size(32);
using matrix_bits = u32 tile_size(512);

#ifdef REJECT_SHAPE
#define MATRIX_VIEW "pto.element.view:v2;dtype=f32;layout=norm;physical_rows=8;physical_cols=64;valid_rows=8;valid_cols=64"
#else
#define MATRIX_VIEW "pto.element.view:v2;dtype=f32;layout=norm;physical_rows=16;physical_cols=32;valid_rows=16;valid_cols=32"
#endif

__attribute__((always_inline, annotate(MATRIX_VIEW)))
inline matrix &matrix_elements(matrix &x) { return x; }

__attribute__((always_inline, annotate("pto.element.view:v2;dtype=f32;layout=norm;physical_rows=1;physical_cols=32;valid_rows=1;valid_cols=32")))
inline row &row_elements(row &x) { return x; }

__attribute__((always_inline, annotate(MATRIX_VIEW)))
inline matrix &matrix_elements(matrix_bits &x) {
  return reinterpret_cast<matrix &>(x);
}

#define LOAD_TILE(TYPE, VALUE, POINTER, ROWS, SIZE)                          \
  asm volatile("BSTART.TLSU TLOAD, FP32\n"                                \
               "B.DIM zero, 32, ->lb0\n"                                 \
               "B.DIM zero, " #ROWS ", ->lb1\n"                         \
               "B.DIM zero, 32, ->lb2\n"                                 \
               "B.IOT mask=1111, last, ->%0<%Z2>\n"                      \
               "B.IOR [%1], []\n"                                         \
               : "=Tr"(VALUE)                                               \
               : "r"(POINTER), "i"(SIZE)                                   \
               : "memory")

#define STORE_TILE(VALUE, POINTER, ROWS)                                    \
  asm volatile("BSTART.TLSU TSTORE, FP32\n"                               \
               "B.DIM zero, 32, ->lb0\n"                                 \
               "B.DIM zero, " #ROWS ", ->lb1\n"                         \
               "B.DIM zero, 32, ->lb2\n"                                 \
               "B.IOT %0, mask=1111, last\n"                             \
               "B.IOR [%1], []\n"                                         \
               :                                                             \
               : "Tr"(VALUE), "r"(POINTER)                                 \
               : "memory")

void norm_f32_matrix(f32 *out, f32 *in, f32 *scale, f32 *bias) {
#ifndef REJECT_CONTRACT
#pragma clang fp contract(off)
#endif
  matrix x_tile, scale_tile, bias_tile, output_tile;
  LOAD_TILE(matrix, x_tile, in, 16, 5);
  LOAD_TILE(matrix, scale_tile, scale, 16, 5);
  LOAD_TILE(matrix, bias_tile, bias, 16, 5);
  auto &x = matrix_elements(x_tile);
  auto &s = matrix_elements(scale_tile);
  auto &b = matrix_elements(bias_tile);
  auto &y = matrix_elements(output_tile);
#pragma pto element for
  for (unsigned element = 0; element < 512; ++element) {
#ifdef REJECT_DIV
    y[element] = x[element] / s[element];
#else
#ifdef NORM_F32_NO_RELU
    const f32 relu = x[element];
#else
    const f32 relu = x[element] > 0.0f ? x[element] : 0.0f;
#endif
    y[element] = relu * s[element] + b[element];
#endif
  }
  STORE_TILE(output_tile, out, 16);
}

void norm_f32_row(f32 *out, f32 *in, f32 *scale, f32 *bias) {
#pragma clang fp contract(off)
  row x_tile, scale_tile, bias_tile, output_tile;
  LOAD_TILE(row, x_tile, in, 1, 1);
  LOAD_TILE(row, scale_tile, scale, 1, 1);
  LOAD_TILE(row, bias_tile, bias, 1, 1);
  auto &x = row_elements(x_tile);
  auto &s = row_elements(scale_tile);
  auto &b = row_elements(bias_tile);
  auto &y = row_elements(output_tile);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    y[element] = x[element] * s[element] + b[element];
  STORE_TILE(output_tile, out, 1);
}

void norm_f32_public_carrier(f32 *out, f32 *in, f32 *scale, f32 *bias) {
#pragma clang fp contract(off)
  matrix_bits x_tile, scale_tile, bias_tile, output_tile;
  LOAD_TILE(matrix_bits, x_tile, in, 16, 5);
  LOAD_TILE(matrix_bits, scale_tile, scale, 16, 5);
  LOAD_TILE(matrix_bits, bias_tile, bias, 16, 5);
  auto &x = matrix_elements(x_tile);
  auto &s = matrix_elements(scale_tile);
  auto &b = matrix_elements(bias_tile);
  auto &y = matrix_elements(output_tile);
#pragma pto element for
  for (unsigned element = 0; element < 512; ++element) {
    const f32 relu = x[element] > 0.0f ? x[element] : 0.0f;
    y[element] = relu * s[element] + b[element];
  }
  STORE_TILE(output_tile, out, 16);
}

// IR-LABEL: define{{.*}}norm_f32_matrix
// IR: @llvm.linx.experimental.ew.texpands.row.v512f32.f32(i64 16, i64 32, i64 1, i64 0,
// IR: @llvm.linx.experimental.ew.tcmp.row.v128i32.v512f32.v512f32(i64 16, i64 32, i64 1, i64 0,
// IR: @llvm.linx.experimental.ew.tsel.row.v512f32.v128i32.v512f32.v512f32
// IR: @llvm.linx.experimental.ew.tbinary.v512f32(i64 16, i64 32, i64 1, i64 0, i64 2,
// IR: @llvm.linx.experimental.ew.tbinary.v512f32(i64 16, i64 32, i64 1, i64 0, i64 0,
// IR-LABEL: define{{.*}}norm_f32_row
// IR: @llvm.linx.experimental.ew.tbinary.v32f32(i64 1, i64 32, i64 1, i64 0, i64 2,
// IR: @llvm.linx.experimental.ew.tbinary.v32f32(i64 1, i64 32, i64 1, i64 0, i64 0,
// IR-LABEL: define{{.*}}norm_f32_public_carrier
// IR: @llvm.linx.experimental.ew.tbinary.v512f32(i64 16, i64 32, i64 1, i64 0, i64 2,
// IR: @llvm.linx.experimental.ew.tbinary.v512f32(i64 16, i64 32, i64 1, i64 0, i64 0,
// IR-NOT: uitofp
// IR-NOT: fptoui

// OBJ: BSTART.TEPL{{[ \t]+}}TEXPANDS, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TCMP, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TSEL, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TMUL, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TADD, FP32
// OBJ-LABEL: <_Z23norm_f32_public_carrierPfS_S_S_>:
// OBJ: BSTART.TEPL{{[ \t]+}}TCMP, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TSEL, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TMUL, FP32
// OBJ: BSTART.TEPL{{[ \t]+}}TADD, FP32

// REJECT: error: PTO element region:
