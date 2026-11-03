// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using tile32 = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline tile32 &element_view(tile32 &value) {
  return value;
}

void histogram_tail(tile32 &old_values, unsigned int *hist,
                    const tile32 &indices, unsigned int valid_lanes) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      output_elements[i] =
          __atomic_fetch_add(&hist[index_elements[i]], 1u, __ATOMIC_RELAXED);
    else
      output_elements[i] = 0;
  }
  old_values = local_output;
}

void histogram_if(tile32 &old_values, unsigned int *hist,
                  const tile32 &low8, const tile32 &high8,
                  unsigned int selected_high, unsigned int valid_lanes) {
  tile32 local_low8 = low8;
  tile32 local_high8 = high8;
  tile32 local_output{};
  auto &low_elements = element_view(local_low8);
  auto &high_elements = element_view(local_high8);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && high_elements[i] == selected_high)
      output_elements[i] =
          __atomic_fetch_add(&hist[low_elements[i]], 1u, __ATOMIC_RELAXED);
    else
      output_elements[i] = 0;
  }
  old_values = local_output;
}

// IR-LABEL: define{{.*}}histogram_tail
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: asm sideeffect "BSTART.TEPL TCMP, U32
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR: asm sideeffect "BSTART.{{(TLSU )?}}MGATHER.ADD{{,?}} U32

// IR-LABEL: define{{.*}}histogram_if
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: asm sideeffect "BSTART.TEPL TCMP, U32
// IR: asm sideeffect "BSTART.TEPL TCMP, U32
// IR: asm sideeffect "BSTART.TEPL TAND, U8
// IR: asm sideeffect "BSTART.TEPL TLEA, U32
// IR: asm sideeffect "BSTART.{{(TLSU )?}}MGATHER.ADD{{,?}} U32

// OBJ-LABEL: <_Z14histogram_tail
// OBJ: BSTART.TEPL TCI, U32
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.{{(TLSU )?}}MGATHER.ADD{{,?}} U32
// OBJ-NOT: ExecMaskPresent

// OBJ-LABEL: <_Z12histogram_if
// OBJ: BSTART.TEPL TCI, U32
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TCMP, U32
// OBJ: BSTART.TEPL TAND, U8
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.{{(TLSU )?}}MGATHER.ADD{{,?}} U32
// OBJ-NOT: ExecMaskPresent
