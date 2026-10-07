// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c -o %t %s
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

__attribute__((always_inline)) inline unsigned
arithmetic_helper(unsigned value, unsigned bias) {
  unsigned local = value + bias;
  local = local * 3u;
  return local;
}

void equivalent_source_forms(elements *output_pointer,
                             const elements *input_pointer, unsigned bias) {
  elements input = *input_pointer;
  elements output{};
  auto &input_elements = element_view(input);
  auto &alias = input_elements;
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element != 32; element += 1) {
    unsigned reassigned = arithmetic_helper(alias[element], bias);
    reassigned = reassigned ^ 17u;
    output_elements[element] = reassigned;
  }
  *output_pointer = output;
}

// IR-LABEL: define{{.*}}equivalent_source_forms
// IR-NOT: llvm.linx.experimental.element.region
// IR-NOT: llvm.linx.experimental.element.view
// IR-NOT: extractelement
// IR-NOT: insertelement
// IR: call <32 x i32> @llvm.linx.blk.tload.v32i32(i64 1, i64 32, i64 1, i64 25, i64 0, i64 21,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 0,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 2,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary.v32i32{{.*}}i64 7,
// IR: call void @llvm.linx.blk.tstore.v32i32(i64 1, i64 32, i64 1, i64 25, i64 24,

// OBJ-LABEL: <{{.*}}equivalent_source_forms
// OBJ: BSTART.TLSU TLOAD, U32
// OBJ: B.DATR ND2M32
// OBJ: BSTART.TEPL TADD, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TEPL TMUL, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TEPL TXOR, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TLSU TSTORE, U32
// OBJ: B.DATR M322ND
