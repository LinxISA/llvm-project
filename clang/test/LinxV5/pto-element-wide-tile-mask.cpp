// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using u32 = unsigned int;
using u64 = unsigned long;
using u32x64 = u32 tile_size(64);
using u32x128 = u32 tile_size(128);
using u32x256 = u32 tile_size(256);
using u32x512 = u32 tile_size(512);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=2;layout=cube_m32"),
               always_inline))
inline u32x64 &u32_view64(u32x64 &value) {
  return value;
}

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=4;layout=cube_m32"),
               always_inline))
inline u32x128 &u32_view128(u32x128 &value) {
  return value;
}

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=8;layout=cube_m32"),
               always_inline))
inline u32x256 &u32_view256(u32x256 &value) { return value; }

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=16;layout=cube_m32"),
               always_inline))
inline u32x512 &u32_view512(u32x512 &value) { return value; }

__attribute__((always_inline)) inline void consume(u32x64 &tile) {
  asm volatile("" : : "Tr"(tile) : "memory");
}

__attribute__((always_inline)) inline void consume(u32x128 &tile) {
  asm volatile("" : : "Tr"(tile) : "memory");
}
__attribute__((always_inline)) inline void consume(u32x256 &tile) { asm volatile("" : : "Tr"(tile) : "memory"); }
__attribute__((always_inline)) inline void consume(u32x512 &tile) { asm volatile("" : : "Tr"(tile) : "memory"); }

extern "C" void wide_mask64(u32 one, u32 zero) {
  u32x64 result;
  auto &out = u32_view64(result);
#pragma pto element for
  for (u32 element = 0; element < 64; ++element) {
    if (element >= 32)
      out[element] = one;
    else
      out[element] = zero;
  }
  consume(result);
}

// IR-LABEL: define{{.*}} @wide_mask64(
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: .gpr
// IR-NOT: <32 x
// The active run includes logical lane 63 and is materialized as PredicateCell logic.
// IR: call <128 x i8> @llvm.linx.experimental.ew.tcmp.v128i8.v64i32.v64i32
// IR: @llvm.linx.experimental.ew.tbinary.v128i8(i64 32, i64 2, i64 27, i64 29, i64 5,
// IR: @llvm.linx.experimental.ew.tbinary.v128i8(i64 32, i64 2, i64 27, i64 29, i64 6,
// IR: @llvm.linx.experimental.ew.tsel.v64i32.v128i8
// IR-NOT: .gpr
// IR-NOT: <32 x
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <wide_mask64>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TOR, U8
// OBJ: BSTART.TEPL TSEL, U32

extern "C" void wide_mask256(u32 one, u32 zero) {
  u32x256 result; auto &out = u32_view256(result);
#pragma pto element for
  for (u32 element = 0; element < 256; ++element)
    out[element] = element >= 128 ? one : zero;
  consume(result);
}
// IR-LABEL: define{{.*}} @wide_mask256(
// IR-NOT: .gpr
// IR-NOT: <32 x
// IR: call <256 x i8> @llvm.linx.experimental.ew.tcmp
// IR: @llvm.linx.experimental.ew.tsel.v256i32.v256i8
// IR: ret void
// OBJ-LABEL: <wide_mask256>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: C.B.DIMI 8, ->lb0
// OBJ: BSTART.TEPL TSEL, U32

extern "C" void wide_mask512(u32 one, u32 zero) {
  u32x512 result; auto &out = u32_view512(result);
#pragma pto element for
  for (u32 element = 0; element < 512; ++element)
    out[element] = element >= 256 ? one : zero;
  consume(result);
}
// IR-LABEL: define{{.*}} @wide_mask512(
// IR-NOT: .gpr
// IR-NOT: <32 x
// IR: call <512 x i8> @llvm.linx.experimental.ew.tcmp
// IR: @llvm.linx.experimental.ew.tsel.v512i32.v512i8
// IR: ret void
// OBJ-LABEL: <wide_mask512>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: C.B.DIMI 16, ->lb0
// OBJ: BSTART.TEPL TSEL, U32

extern "C" void wide_mask128(u32 one, u32 zero) {
  u32x128 result;
  auto &out = u32_view128(result);
#pragma pto element for
  for (u32 element = 0; element < 128; ++element) {
    if (element >= 64)
      out[element] = one;
    else
      out[element] = zero;
  }
  consume(result);
}

// IR-LABEL: define{{.*}} @wide_mask128(
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: .gpr
// IR-NOT: <32 x
// The active run crosses lane 64 and includes lane 127.
// IR: call <128 x i8> @llvm.linx.experimental.ew.tcmp.v128i8.v128i32.v128i32
// IR: @llvm.linx.experimental.ew.tbinary.v128i8(i64 32, i64 4, i64 27, i64 29, i64 5,
// IR: @llvm.linx.experimental.ew.tbinary.v128i8(i64 32, i64 4, i64 27, i64 29, i64 6,
// IR: @llvm.linx.experimental.ew.tsel.v128i32.v128i8
// IR-NOT: .gpr
// IR-NOT: <32 x
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <wide_mask128>:
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TOR, U8
// OBJ: BSTART.TEPL TSEL, U32

extern "C" void wide_indexed_nested128(
    const u64 *__restrict keys, const u32 *__restrict indices,
    const u64 *__restrict table_keys, const u32 *__restrict table_values,
    const u32 *__restrict live, u32 valid) {
  u32x128 result;
  auto &out = u32_view128(result);
#pragma pto element for
  for (u32 element = 0; element < 128; ++element) {
    u32 next = 0;
    if (element < valid && live[element] != 0) {
      const u32 slot = indices[element];
      const u64 found = table_keys[slot];
      if (found == keys[element])
        next = table_values[slot];
      else if (found != ~u64(0))
        next = live[element] + 1;
    }
    out[element] = next;
  }
  consume(result);
}

// IR-LABEL: define{{.*}} @wide_indexed_nested128(
// IR-NOT: @llvm.linx.experimental.element.region
// IR-NOT: @llvm.vp.
// IR-NOT: .gpr
// IR-NOT: <32 x
// Mixed U32/U64 comparisons publish same-shape PredicateCells and compose in U8.
// IR-DAG: call <128 x i8> @llvm.linx.experimental.ew.tcmp.v128i8.v128i32.v128i32
// IR-DAG: call <128 x i8> @llvm.linx.experimental.ew.tcmp.v128i8.v128i64.v128i64
// IR: @llvm.linx.experimental.ew.tbinary.v128i8(i64 32, i64 4, i64 27, i64 29, i64 5,
// Indexed U64 and U32 reads consume the PredicateCell directly.
// IR: @llvm.linx.experimental.ew.mgather.tile.masked.v128i64.v128i64.v128i8(i64 32, i64 4, i64 24, i64 0, i64 29,
// IR: @llvm.linx.experimental.ew.mgather.tile.masked.v128i32.v128i64.v128i8(i64 32, i64 4, i64 25, i64 0, i64 29,
// IR: @llvm.linx.experimental.ew.tadd.masked{{.*}}<128 x i8>
// IR: @llvm.linx.experimental.ew.tsel.v128i32.v128i8
// IR-NOT: .gpr
// IR-NOT: <32 x
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <wide_indexed_nested128>:
// OBJ: BSTART.TEPL TCMP, U64
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TAND, U8
// OBJ: BSTART.TLSU MGATHER.MASK, U64
// OBJ: BSTART.TLSU MGATHER.MASK, U32
// OBJ: BSTART.TEPL TADD, U32
// OBJ: BSTART.TEPL TSEL, U32
