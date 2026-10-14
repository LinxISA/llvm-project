// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=OBJ
using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

void arithmetic(elements &output, const elements &a, const elements &b,
                unsigned int shift) {
  elements local_a = a;
  elements local_b = b;
  elements local_output{};
  auto &a_elements = element_view(local_a);
  auto &b_elements = element_view(local_b);
  auto &output_elements = element_view(local_output);
#pragma pto element for
  for (unsigned int element = 0; element < 32; ++element) {
    unsigned int sum = a_elements[element] + b_elements[element];
    const unsigned int shifted = sum >> shift;
    output_elements[element] = (shifted & 255u) ^ 17u;
  }
  output = local_output;
}
// IR-LABEL: define{{.*}}arithmetic
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 0,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 9,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 5,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 7,
// IR-NOT: for.body
// IR-NOT: extractelement
// OBJ-LABEL: <{{.*}}arithmetic
// OBJ: BSTART.TEPL TADD, U32
// OBJ: B.DATR CUBE_M32
// OBJ: BSTART.TEPL TSHR, U32
// OBJ: BSTART.TEPL TAND, U32
// OBJ: BSTART.TEPL TXOR, U32

void locals(elements &output, const elements &input) {
  elements local_input = input;
  elements local_output{};
  auto &input_elements = element_view(local_input);
  auto &output_elements = element_view(local_output);
#pragma pto element for
  for (unsigned int element = 0; element < 32; ++element) {
    unsigned int value = input_elements[element] * 3u;
    value = value - element;
    output_elements[element] = ~value;
  }
  output = local_output;
}
// IR-LABEL: define{{.*}}locals
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci{{.*}}i64 0, i64 4294967296
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 2,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tci{{.*}}i64 4294967295, i64 0
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 7,
// IR: call <32 x i32> @llvm.linx.experimental.ew.tbinary{{.*}}i64 0,
// OBJ-LABEL: <{{.*}}locals
// OBJ: BSTART.TEPL TMUL, U32
// OBJ: BSTART.TEPL TXOR, U32
// OBJ: BSTART.TEPL TADD, U32
