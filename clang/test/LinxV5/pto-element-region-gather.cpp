// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

void equivalent_gather(elements *output_pointer,
                       const elements *index_pointer,
                       const unsigned *table, unsigned valid, bool active) {
  elements indices = *index_pointer;
  elements output{};
  auto &index_elements = element_view(indices);
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element != 32; element += 1) {
    if (active && valid > element)
      output_elements[element] = table[index_elements[element]];
    else
      output_elements[element] = 0u;
  }
  *output_pointer = output;
}

// IR-LABEL: define{{.*}}equivalent_gather
// IR-NOT: llvm.linx.experimental.element.region
// IR-NOT: llvm.linx.experimental.element.view
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: call i64 @llvm.linx.experimental.ew.tcmps.gpr
// IR: call <32 x i64> @llvm.linx.experimental.ew.tlea
// IR: call <32 x i32> @llvm.linx.experimental.ew.mgather.gpr.masked
// IR-NOT: zeroinitializer
// IR: store <32 x i32> %linx.elementwise.gather, ptr

// OBJ-LABEL: <{{.*}}equivalent_gather
// OBJ: BSTART.TEPL TCMPS, U32
// OBJ: BSTART.TEPL TLEA, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TLSU MGATHER, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TLSU TSTORE, S32
// OBJ: B.DATR NORM.normal, Null

