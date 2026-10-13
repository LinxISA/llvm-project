// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using u32 = unsigned int;
using u64 = unsigned long;
using s64 = long;
using u64_elements = u64 tile_size(32);
using s64_elements = s64 tile_size(32);

__attribute__((annotate(
    "pto.element.view:v1;dtype=u64;rows=32;cols=1;layout=cube_m32"),
    always_inline)) inline u64_elements &u64_view(u64_elements &value) {
  return value;
}
__attribute__((annotate(
    "pto.element.view:v1;dtype=s64;rows=32;cols=1;layout=cube_m32"),
    always_inline)) inline s64_elements &s64_view(s64_elements &value) {
  return value;
}

__attribute__((always_inline)) inline void store_u64(u64 *output,
                                                     u64_elements &tile) {
  asm volatile("BSTART.TLSU TSTORE, U64\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(tile), "r"(output), "r"(8UL) : "memory");
}
__attribute__((always_inline)) inline void store_s64(s64 *output,
                                                     s64_elements &tile) {
  asm volatile("BSTART.TLSU TSTORE, S64\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(tile), "r"(output), "r"(8UL) : "memory");
}

__attribute__((always_inline)) inline void consume_u64(u64_elements &tile) {
  asm volatile("" : : "Tr"(tile) : "memory");
}
__attribute__((always_inline)) inline void consume_s64(s64_elements &tile) {
  asm volatile("" : : "Tr"(tile) : "memory");
}

extern "C" void u64_masked_indexed(const u64 *__restrict input,
                                    const u32 *__restrict indices,
                                    u64 *output, u32 valid, u64 fill) {
  u64_elements result;
  auto &values = u64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    values[element] = element < valid ? input[indices[element]] : fill;
  store_u64(output, result);
}

// IR-LABEL: define{{.*}} @u64_masked_indexed(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32>
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 25,
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, <32 x i32>
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 24,
// IR: @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 24,
// IR-NOT: @llvm.vp.
// IR-NOT: <64 x i64>
// OBJ-LABEL: <u64_masked_indexed>:
// OBJ: BSTART.TLSU MGATHER, U32
// OBJ: BSTART.TLSU MGATHER, U64
// OBJ: BSTART.TEPL TSEL, U64
// OBJ: BSTART.TLSU TSTORE, U64

extern "C" void s64_signed_min(const s64 *__restrict left,
                               const s64 *__restrict right, s64 *output,
                               u32 valid) {
  s64_elements result;
  auto &values = s64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    s64 lhs = left[element];
    s64 rhs = right[element];
    values[element] = element < valid && lhs < rhs ? lhs : -1;
  }
  store_s64(output, result);
}

// IR-LABEL: define{{.*}} @s64_signed_min(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 16,
// IR: @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 16,
// IR: @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 16,
// IR-NOT: <64 x i64>
// OBJ-LABEL: <s64_signed_min>:
// OBJ: BSTART.TLSU MGATHER, S64
// OBJ: BSTART.TEPL TCMP, S64
// OBJ: BSTART.TEPL TSEL, S64
// OBJ: BSTART.TLSU TSTORE, S64

extern "C" void u64_unsigned_min(const u64 *__restrict left,
                                 const u64 *__restrict right, u64 *output) {
  u64_elements result;
  auto &values = u64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    u64 lhs = left[element];
    u64 rhs = right[element];
    values[element] = lhs < rhs ? lhs : 0;
  }
  store_u64(output, result);
}

// IR-LABEL: define{{.*}} @u64_unsigned_min(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 24,
// IR: @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 24,
// IR: @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 24,
// IR-NOT: <64 x i64>
// OBJ-LABEL: <u64_unsigned_min>:
// OBJ: BSTART.TEPL TCMP, U64
// OBJ: BSTART.TEPL TSEL, U64

extern "C" void s64_tail_store(const s64 *__restrict input,
                               s64 *__restrict output, u32 valid) {
  s64_elements result;
  auto &values = s64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    s64 value = element < valid ? input[element] : 0;
    values[element] = value;
    if (element < valid)
      output[element] = value;
  }
  consume_s64(result);
}

// IR-LABEL: define{{.*}} @s64_tail_store(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 16,
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 16,
// OBJ-LABEL: <s64_tail_store>:
// OBJ: BSTART.TLSU MGATHER, S64
// OBJ: BSTART.TLSU MSCATTER, S64

extern "C" void u64_tail_store(const u64 *__restrict input,
                               u64 *__restrict output, u32 valid) {
  u64_elements result;
  auto &values = u64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    u64 value = element < valid ? input[element] : 0;
    values[element] = value;
    if (element < valid)
      output[element] = value;
  }
  consume_u64(result);
}

// IR-LABEL: define{{.*}} @u64_tail_store(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 24,
// IR: @llvm.linx.experimental.ew.mscatter.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 24,
// OBJ-LABEL: <u64_tail_store>:
// OBJ: BSTART.TLSU MGATHER, U64
// OBJ: BSTART.TLSU MSCATTER, U64

extern "C" void s64_indexed_address(const s64 *__restrict indices,
                                    const s64 *__restrict input, u32 valid) {
  s64_elements result;
  auto &values = s64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    values[element] = element < valid ? input[indices[element]] : 0;
  consume_s64(result);
}

// IR-LABEL: define{{.*}} @s64_indexed_address(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 16,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 0,
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i64(i64 32, i64 1, i64 24, i64 29, <32 x i64>
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 16,
// OBJ-LABEL: <s64_indexed_address>:
// OBJ: BSTART.TLSU MGATHER, S64
// OBJ: BSTART.TEPL TADD, U64
// OBJ: BSTART.TEPL TLEA, U64
// OBJ: BSTART.TLSU MGATHER, S64

extern "C" void s64_equal_choose(const s64 *__restrict left,
                                 const s64 *__restrict right, s64 *output) {
  s64_elements result;
  auto &values = s64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    s64 lhs = left[element];
    s64 rhs = right[element];
    values[element] = lhs == rhs ? lhs : -1;
  }
  store_s64(output, result);
}

// IR-LABEL: define{{.*}} @s64_equal_choose(
// IR: @llvm.linx.experimental.ew.tcmp.gpr.v32i64(i64 32, i64 1, i64 16, i64 29, {{.*}}, i64 0)
// IR: @llvm.linx.experimental.ew.tsel.gpr.v32i64(i64 32, i64 1, i64 16,
// OBJ-LABEL: <s64_equal_choose>:
// OBJ: BSTART.TEPL TCMP, S64
// OBJ: BSTART.TEPL TSEL, S64
// OBJ: BSTART.TLSU TSTORE, S64

#ifdef TEST_I64_TRUNCATE
extern "C" void rejected_truncate(const u64 *__restrict input,
                                  u32 *__restrict output) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    output[element] = static_cast<u32>(input[element]);
}
#endif

// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_I64_TRUNCATE -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=TRUNC
// TRUNC: PTO element region: Tile legalization: only i32 to i64 index extension is supported
