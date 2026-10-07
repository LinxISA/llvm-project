// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

void histogram_atomic(elements *output_pointer, const elements *index_pointer,
                      unsigned *histogram, unsigned valid, bool active) {
  elements indices = *index_pointer;
  elements output{};
  auto &index_elements = element_view(indices);
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element != 32; ++element) {
    if (active && element < valid)
      output_elements[element] = __atomic_fetch_add(
          &histogram[index_elements[element]], 1u, __ATOMIC_RELAXED);
    else
      output_elements[element] = 0u;
  }
  *output_pointer = output;
}

void selected_atomic(elements *output_pointer, const elements *index_pointer,
                     const elements *key_pointer, unsigned *histogram,
                     unsigned valid, unsigned selected) {
  elements indices = *index_pointer;
  elements keys = *key_pointer;
  elements output{};
  auto &index_elements = element_view(indices);
  auto &key_elements = element_view(keys);
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element != 32; ++element) {
    if (element < valid && key_elements[element] == selected)
      output_elements[element] = __atomic_fetch_add(
          &histogram[index_elements[element]], 1u, __ATOMIC_RELAXED);
    else
      output_elements[element] = 0u;
  }
  *output_pointer = output;
}

// IR-LABEL: define{{.*}}histogram_atomic
// IR-NOT: llvm.linx.experimental.element.region
// IR-NOT: atomicrmw
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked
// IR-LABEL: define{{.*}}selected_atomic
// IR-NOT: llvm.linx.experimental.element.region
// IR-NOT: atomicrmw
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: and i64
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.add.masked

// OBJ-LABEL: <{{.*}}histogram_atomic
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: B.DATR CUBE_M32
// OBJ: ExecMaskPresent
// OBJ-LABEL: <{{.*}}selected_atomic
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: and
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: BSTART.TLSU MGATHER.ADD, U32
// OBJ: B.DATR CUBE_M32
// OBJ: ExecMaskPresent
