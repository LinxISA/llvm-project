// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -enable-all-vector-as-tilereg=true -c %s -o %t.o
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t.o | FileCheck %s --check-prefix=OBJ

using u32 = unsigned;
using u32_elements = u32 tile_size(32);
using s32_elements = int tile_size(32);

__attribute__((annotate("pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline u32_elements &u32_view(u32_elements &value) { return value; }

__attribute__((annotate("pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline s32_elements &s32_view(s32_elements &value) { return value; }

__attribute__((always_inline))
inline u32_elements load_u32(const u32 *pointer) {
  u32_elements result;
  asm("BSTART.TLSU TLOAD, U32\nB.DATR ND2M32, Zero\n"
      "B.DIM zero, 1, ->lb0\nB.DIM zero, 32, ->lb1\n"
      "B.IOT mask=1111, last, ->%0<%Z3>\nB.IOR [%1,%2], []"
      : "=Tr"(result) : "r"(pointer), "r"(4UL), "i"(1) : "memory");
  return result;
}

__attribute__((always_inline))
inline void store_u32(u32 *pointer, u32_elements value) {
  asm volatile("BSTART.TLSU TSTORE, U32\nB.DATR M322ND, Zero\n"
               "B.DIM zero, 1, ->lb0\nB.DIM zero, 32, ->lb1\n"
               "B.IOT %0, mask=1111, last\nB.IOR [%1,%2], []"
               : : "Tr"(value), "r"(pointer), "r"(4UL) : "memory");
}

__attribute__((always_inline))
inline u32_elements zero_u32() {
  u32_elements result;
  u32 scalar = 0;
  asm("" : "+r"(scalar));
  asm("BSTART.TEPL TCI, U32\nB.DATR CUBE_M32, Zero\n"
      "B.DIM zero, 1, ->lb0\nB.DIM zero, 32, ->lb1\n"
      "B.IOT mask=1111, last, ->%0<%Z1>\nB.IOR [%2], []"
      : "=Tr"(result) : "i"(1), "r"(scalar));
  return result;
}

extern "C" void nested_state(u32 *live_pointer, int *value_pointer,
                             int *offset_pointer, const u32 *key_pointer,
                             const u32 *found_pointer,
                             const u32 *loaded_pointer, u32 valid) {
  u32_elements live_input = load_u32(live_pointer);
  u32_elements key_input = load_u32(key_pointer);
  u32_elements found_input = load_u32(found_pointer);
  s32_elements value_input = (s32_elements)load_u32((const u32 *)value_pointer);
  s32_elements offset_input = (s32_elements)load_u32((const u32 *)offset_pointer);
  s32_elements loaded_input = (s32_elements)load_u32(loaded_pointer);
  auto &live = u32_view(live_input);
  auto &key = u32_view(key_input);
  auto &found = u32_view(found_input);
  auto &value = s32_view(value_input);
  auto &offset = s32_view(offset_input);
  auto &loaded = s32_view(loaded_input);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    u32 next_live = live[element];
    int next_value = value[element], next_offset = offset[element];
    if (element < valid && next_live != 0) {
      if (found[element] == ~0u)
        next_live = 0;
      else if (found[element] == key[element]) {
        next_value = loaded[element];
        next_live = 0;
      } else {
        next_offset += 16;
        if (next_offset == 4080)
          next_offset = 0;
      }
    }
    live[element] = next_live;
    value[element] = next_value;
    offset[element] = next_offset;
  }
  store_u32(live_pointer, live_input);
  store_u32((u32 *)value_pointer, (u32_elements)value_input);
  store_u32((u32 *)offset_pointer, (u32_elements)offset_input);
}

// IR-LABEL: define{{.*}} @nested_state(
// IR-COUNT-4: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr
// IR-NOT: call <32 x i32> @llvm.linx.experimental.ew.tsel.gpr
// IR: ret void
// OBJ-LABEL: <nested_state>:
// OBJ-COUNT-4: BSTART.TEPL TSEL
// OBJ-NOT: BSTART.TEPL TSEL

extern "C" void gather_bytes(u32 *__restrict output,
                             const u32 *__restrict data,
                             const u32 *__restrict offsets) {
  u32_elements offset_input = load_u32(offsets);
  u32_elements result = zero_u32();
  auto &offset = u32_view(offset_input);
  auto &value = u32_view(result);
  const char *bytes = reinterpret_cast<const char *>(data);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element)
    value[element] = *reinterpret_cast<const u32 *>(bytes + offset[element]);
  store_u32(output, result);
}

// IR-LABEL: define{{.*}} @gather_bytes(
// IR: @llvm.linx.experimental.ew.tlea{{.*}}i64 8)
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <gather_bytes>:
// OBJ: BSTART.TEPL TLEA
// OBJ: BSTART.MGATHER

extern "C" void frozen_predicate(const u32 *__restrict table,
                                 const u32 *__restrict offsets,
                                 const u32 *__restrict active,
                                 u32 *__restrict output, u32 valid) {
  u32_elements offset_input = load_u32(offsets);
  u32_elements active_input = load_u32(active);
  u32_elements result = zero_u32();
  auto &offset = u32_view(offset_input);
  auto &live = u32_view(active_input);
  auto &value = u32_view(result);
#pragma pto element for
  for (u32 element = 0; element < 32; ++element) {
    const u32 old_offset = offset[element];
    const bool active_lane = element < valid && live[element] != 0;
    u32 found = ~0u;
    if (active_lane)
      found = table[old_offset];
    const bool hit = active_lane && found != ~0u && found == old_offset;
    value[element] = hit ? old_offset : value[element];
  }
  store_u32(output, result);
}

// IR-LABEL: define{{.*}} @frozen_predicate(
// IR: @llvm.linx.experimental.ew.mgather.gpr.masked
// IR-NOT: freeze <32 x i1>
// IR-NOT: @llvm.vp.
// IR: ret void
// OBJ-LABEL: <frozen_predicate>:
// OBJ: BSTART.MGATHER
// OBJ: ExecMaskPresent
