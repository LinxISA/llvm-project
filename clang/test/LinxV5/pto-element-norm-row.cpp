// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm -DREJECT_GM %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm -DREJECT_DIV %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm -DREJECT_BOOLEAN_COMPARE %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REJECT
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

#if defined(REJECT_GM)
#define EXPRESSION in[i]
#elif defined(REJECT_DIV)
#define EXPRESSION (x[i] / threshold)
#elif defined(REJECT_BOOLEAN_COMPARE)
#define EXPRESSION ((x[i] > threshold) == (x[i] == 1u) ? 1u : 0u)
#else
#define EXPRESSION (x[i] > threshold ? x[i] : 0)
#endif

using u32 = unsigned;
using row128 = u32 tile_size(128);
using row2048 = u32 tile_size(2048);

__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=128;valid_rows=1;valid_cols=128")))
inline row128 &full128(row128 &x) { return x; }
__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=128;valid_rows=1;valid_cols=32")))
inline row128 &tail128(row128 &x) { return x; }
__attribute__((always_inline, annotate("pto.element.view:v2;dtype=u32;layout=norm;physical_rows=1;physical_cols=2048;valid_rows=1;valid_cols=2048")))
inline row2048 &full2048(row2048 &x) { return x; }

#define ROW_CASE(NAME, TYPE, VIEW, VALID, PHYSICAL, SIZE)                     \
void NAME(u32 *out, u32 *in, u32 threshold) {                               \
  TYPE input, output;                                                      \
  asm volatile("BSTART.TLSU TLOAD, U32\n"                                  \
               "B.DIM zero, " #VALID ", ->lb0\n"                          \
               "B.DIM zero, 1, ->lb1\n"                                 \
               "B.DIM zero, " #PHYSICAL ", ->lb2\n"                       \
               "B.IOT mask=1111, last, ->%0<%Z2>\n"               \
               "B.IOR [%1], []\n" : "=Tr"(input) : "r"(in), "i"(SIZE) : "memory"); \
  auto &x = VIEW(input);                                                   \
  auto &y = VIEW(output);                                                  \
  _Pragma("pto element for")                                              \
  for (u32 i = 0; i < VALID; ++i)                                         \
    y[i] = EXPRESSION;                                   \
  asm volatile("BSTART.TLSU TSTORE, U32\n"                                 \
               "B.DIM zero, " #VALID ", ->lb0\n"                          \
               "B.DIM zero, 1, ->lb1\n"                                 \
               "B.DIM zero, " #PHYSICAL ", ->lb2\n"                       \
               "B.IOT %0, mask=1111, last\n"                             \
               "B.IOR [%1], []\n" : : "Tr"(output), "r"(out) : "memory");\
}

ROW_CASE(norm128, row128, full128, 128, 128, 3)
ROW_CASE(norm_tail, row128, tail128, 32, 128, 3)
ROW_CASE(norm2048, row2048, full2048, 2048, 2048, 7)

// IR-LABEL: define{{.*}}norm128
// IR: @llvm.linx.experimental.ew.texpands.row.v128i32.i64(i64 1, i64 128, i64 25, i64 0,
// IR: @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32
// IR: @llvm.linx.experimental.ew.tsel.row.v128i32.v32i32.v128i32.v128i32
// IR-LABEL: define{{.*}}norm_tail
// IR: @llvm.linx.experimental.ew.texpands.row.v128i32.i64(i64 1, i64 32, i64 25, i64 0,
// IR: @llvm.linx.experimental.ew.tcmp.row.v32i32.v128i32.v128i32(i64 1, i64 32, i64 25, i64 0,
// IR-LABEL: define{{.*}}norm2048
// IR: @llvm.linx.experimental.ew.tcmp.row.v512i32.v2048i32.v2048i32
// IR-NOT: tcmp.gpr
// OBJ: BSTART.TEPL{{[ \t]+}}TEXPANDS, U32
// OBJ: BSTART.TEPL{{[ \t]+}}TCMP, U32
// OBJ: BSTART.TEPL{{[ \t]+}}TSEL, U32

// REJECT: error: PTO element region:
