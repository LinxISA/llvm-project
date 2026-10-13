// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using u32 = unsigned int;
using s32 = int;
using u16 = unsigned short;
using u64 = unsigned long;
using s64 = long;
using u32_elements = u32 tile_size(32);
using s32_elements = s32 tile_size(32);
using u64_elements = u64 tile_size(32);
using s64_elements = s64 tile_size(32);

__attribute__((annotate(
    "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
    always_inline)) inline u32_elements &u32_view(u32_elements &value) {
  return value;
}
__attribute__((annotate(
    "pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32"),
    always_inline)) inline s32_elements &s32_view(s32_elements &value) {
  return value;
}

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
__attribute__((always_inline)) inline void store_u32(u32 *output,
                                                     u32_elements &tile) {
  asm volatile("BSTART.TLSU TSTORE, U32\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(tile), "r"(output), "r"(4UL) : "memory");
}
__attribute__((always_inline)) inline void store_s32(s32 *output,
                                                     s32_elements &tile) {
  asm volatile("BSTART.TLSU TSTORE, S32\n"
               "B.DATR M322ND, DTYPE_NONE, Null\n"
               "B.DIM zero, 1, ->lb0\n"
               "B.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\n"
               "B.IOR [%1,%2], []\n"
               : : "Tr"(tile), "r"(output), "r"(4UL) : "memory");
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

extern "C" void u32_byte_offsets(const u32 *__restrict input, u64 *output) {
  u64_elements result;
  auto &values = u64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    values[element] = static_cast<u64>(input[element]) * 12u;
  store_u64(output, result);
}

// IR-LABEL: define{{.*}} @u32_byte_offsets(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, {{.*}}, i64 8)
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 2,
// OBJ-LABEL: <u32_byte_offsets>:
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU TSTORE, U64

extern "C" void s32_byte_offsets(const s32 *__restrict input, s64 *output) {
  s64_elements result;
  auto &values = s64_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    values[element] = static_cast<s64>(input[element]) * 12;
  store_s64(output, result);
}

// IR-LABEL: define{{.*}} @s32_byte_offsets(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, {{.*}}, i64 8)
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 16, i64 29, i64 2,
// OBJ-LABEL: <s32_byte_offsets>:
// OBJ: BSTART.TEPL TLEA, S32
// OBJ: BSTART.TEPL TMUL, S64
// OBJ: BSTART.TLSU TSTORE, S64

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

extern "C" void s64_to_s32(const s64 *__restrict input,
                            s32 *__restrict output, u32 valid) {
  s64_elements source;
  auto &source_values = s64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  s32_elements result;
  auto &result_values = s32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<s32>(source_values[element]);
  store_s32(output, result);
}

// IR-LABEL: define{{.*}} @s64_to_s32(
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 16, i64 17, i64 29, i64 0, i64 0,
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tlea.v32i64.v32i64
// OBJ-LABEL: <s64_to_s32>:
// OBJ: BSTART.TEPL TCVT, S64
// OBJ: B.DATR NORM, S32
// OBJ: BSTART.TLSU TSTORE, S32

extern "C" void s64_to_u32(const s64 *__restrict input,
                            u32 *__restrict output, u32 valid) {
  s64_elements source;
  auto &source_values = s64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  u32_elements result;
  auto &result_values = u32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<u32>(source_values[element]);
  store_u32(output, result);
}

// IR-LABEL: define{{.*}} @s64_to_u32(
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 16, i64 25, i64 29, i64 0, i64 0,
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tlea.v32i64.v32i64
// OBJ-LABEL: <s64_to_u32>:
// OBJ: BSTART.TEPL TCVT, S64
// OBJ: B.DATR NORM, U32
// OBJ: BSTART.TLSU TSTORE, U32

extern "C" void u64_to_s32(const u64 *__restrict input,
                            s32 *__restrict output, u32 valid) {
  u64_elements source;
  auto &source_values = u64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  s32_elements result;
  auto &result_values = s32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<s32>(source_values[element]);
  store_s32(output, result);
}

// IR-LABEL: define{{.*}} @u64_to_s32(
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 24, i64 17, i64 29, i64 0, i64 0,
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tlea.v32i64.v32i64
// OBJ-LABEL: <u64_to_s32>:
// OBJ: BSTART.TEPL TCVT, U64
// OBJ: B.DATR NORM, S32
// OBJ: BSTART.TLSU TSTORE, S32

extern "C" void u64_to_u32(const u64 *__restrict input,
                            u32 *__restrict output, u32 valid) {
  u64_elements source;
  auto &source_values = u64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  u32_elements result;
  auto &result_values = u32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<u32>(source_values[element]);
  store_u32(output, result);
}

// IR-LABEL: define{{.*}} @u64_to_u32(
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 24, i64 25, i64 29, i64 0, i64 0,
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tlea.v32i64.v32i64
// OBJ-LABEL: <u64_to_u32>:
// OBJ: BSTART.TEPL TCVT, U64
// OBJ: B.DATR NORM, U32
// OBJ: BSTART.TLSU TSTORE, U32

extern "C" void u64_upper_half_to_u32(const u64 *__restrict input,
                                       u32 *__restrict output, u32 valid) {
  u64_elements source;
  auto &source_values = u64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  u32_elements result;
  auto &result_values = u32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<u32>(source_values[element] >> 32);
  store_u32(output, result);
}

// IR-LABEL: define{{.*}} @u64_upper_half_to_u32(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 9,
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 24, i64 25, i64 29, i64 0, i64 0,
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// OBJ-LABEL: <u64_upper_half_to_u32>:
// OBJ: BSTART.TEPL TSHR, U64
// OBJ: BSTART.TEPL TCVT, U64
// OBJ: B.DATR NORM, U32

extern "C" void s64_shift_to_s32(const s64 *__restrict input,
                                  s32 *__restrict output, u32 valid,
                                  u32 shift) {
  s64_elements source;
  auto &source_values = s64_view(source);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      source_values[element] = input[element];

  s32_elements result;
  auto &result_values = s32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      result_values[element] = static_cast<s32>(source_values[element] >> shift);
  store_s32(output, result);
}

// IR-LABEL: define{{.*}} @s64_shift_to_s32(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 16, i64 29, i64 9,
// IR: @llvm.linx.experimental.ew.tcvt.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 16, i64 17, i64 29, i64 0, i64 0,
// OBJ-LABEL: <s64_shift_to_s32>:
// OBJ: BSTART.TEPL TSHR, S64
// OBJ: BSTART.TEPL TCVT, S64
// OBJ: B.DATR NORM, S32

#ifdef TEST_UNSUPPORTED_I64_TO_U16
extern "C" void rejected_u16(const u64 *__restrict input,
                              u16 *__restrict output) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    output[element] = static_cast<u16>(input[element]);
}
#endif

// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_UNSUPPORTED_I64_TO_U16 -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNSUPPORTED
// UNSUPPORTED: {{PTO element region}}
