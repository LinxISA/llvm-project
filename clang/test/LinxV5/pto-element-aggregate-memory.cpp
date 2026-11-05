// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_MULTI_DYNAMIC -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_SAME_INDEX_TWICE -c %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BAD

using u32 = unsigned int;
using s32 = int;
using u64 = unsigned long;
using s64 = long;

struct TableEntry {
  s64 key;
  s32 value;
  s32 padding;
};
static_assert(sizeof(TableEntry) == 16, "TableEntry stride");

struct NestedPayload {
  s32 tag;
  struct {
    s32 padding;
    s64 key;
  } payload;
  s32 tail;
};
static_assert(sizeof(NestedPayload) == 32, "NestedPayload stride");

struct Stride12 {
  u32 first;
  u32 selected;
  u32 last;
};
static_assert(sizeof(Stride12) == 12, "Stride12 stride");

struct __attribute__((packed)) PackedStride5 {
  unsigned char tag;
  u32 selected;
};
static_assert(sizeof(PackedStride5) == 5, "PackedStride5 stride");

extern "C" void table_fields(const TableEntry *__restrict table,
                              const u32 *__restrict indices,
                              s64 *__restrict keys, s32 *__restrict values,
                              u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    if (element < valid) {
      const u32 position = indices[element];
      keys[element] = table[position].key;
      values[element] = table[position].value;
    }
  }
}

// IR-LABEL: define{{.*}} @table_fields(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, {{.*}}, i64 8)
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 2,
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64(i64 32, i64 1, i64 24,
// IR: getelementptr i8, ptr %table, i64 8
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i32.v32i64(i64 32, i64 1, i64 25,
// OBJ-LABEL: <table_fields>:
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U64
// OBJ-NOT: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U32

extern "C" void signed_entry_index(const TableEntry *__restrict middle,
                                    const s32 *__restrict indices,
                                    s64 *__restrict output, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = middle[indices[element]].key;
}

// IR-LABEL: define{{.*}} @signed_entry_index(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 17, i64 29, {{.*}}, i64 8)
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 16, i64 29, i64 2,
// OBJ-LABEL: <signed_entry_index>:
// OBJ: BSTART.TEPL TLEA, S32
// OBJ: BSTART.TEPL TMUL, S64

extern "C" void same_address_masks(const TableEntry *__restrict table,
                                    const u32 *__restrict indices,
                                    s64 *__restrict first,
                                    s64 *__restrict second, u32 first_valid,
                                    u32 second_valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const s64 *address = &table[indices[element]].key;
    if (element < first_valid)
      first[element] = *address;
    if (element < second_valid)
      second[element] = *address;
  }
}

// IR-LABEL: define{{.*}} @same_address_masks(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64{{.*}}i64 2,
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64{{.*}}i64 2,
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked.v32i64.v32i64
// OBJ-LABEL: <same_address_masks>:
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U64
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U64

extern "C" void nested_field(const NestedPayload *__restrict records,
                              const u32 *__restrict indices,
                              s64 *__restrict output, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = records[indices[element]].payload.key;
}

// IR-LABEL: define{{.*}} @nested_field(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 2,
// IR: getelementptr i8, ptr %records, i64 16
// OBJ-LABEL: <nested_field>:
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U64

extern "C" void stride12_field(const Stride12 *__restrict records,
                                const u32 *__restrict indices,
                                u32 *__restrict output, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = records[indices[element]].selected;
}

// IR-LABEL: define{{.*}} @stride12_field(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, {{.*}}, i64 8)
// IR: freeze i64 12
// IR: @llvm.linx.experimental.ew.tci.v32i64(i64 32, i64 1, i64 24, i64 29,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 2,
// IR: getelementptr i8, ptr %records, i64 4
// OBJ-LABEL: <stride12_field>:
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U32

extern "C" void packed_stride5_field(
    const PackedStride5 *__restrict records, const u32 *__restrict indices,
    u32 *__restrict output, u32 valid) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    if (element < valid)
      output[element] = records[indices[element]].selected;
}

// IR-LABEL: define{{.*}} @packed_stride5_field(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25, i64 29, {{.*}}, i64 8)
// IR: freeze i64 5
// IR: @llvm.linx.experimental.ew.tci.v32i64(i64 32, i64 1, i64 24, i64 29,
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64(i64 32, i64 1, i64 24, i64 29, i64 2,
// IR: getelementptr i8, ptr %records, i64 1
// OBJ-LABEL: <packed_stride5_field>:
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TEPL TMUL, U64
// OBJ: BSTART.TLSU MGATHER, U32

#if defined(TEST_MULTI_DYNAMIC) || defined(TEST_SAME_INDEX_TWICE)
struct Rows { s64 values[4]; };
extern "C" void rejected_dynamic_field(const Rows *rows, const u32 *first,
                                        const u32 *second, s64 *output) {
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const u32 row = first[element];
#ifdef TEST_SAME_INDEX_TWICE
    output[element] = rows[row].values[row];
#else
    output[element] = rows[row].values[second[element] & 3];
#endif
  }
}
#endif

// BAD: one dynamic aggregate index with constant field suffixes
