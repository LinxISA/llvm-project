// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -c -o /dev/null %s 2>&1 | FileCheck %s

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

void rejected_until_typed_spills_are_supported(unsigned bias) {
  elements input;
  elements output;
  auto &input_elements = element_view(input);
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output_elements[element] = input_elements[element] + bias;
}

// CHECK: error: {{.*}}PTO element region: typed Tile spill/reload is unsupported at -O0

