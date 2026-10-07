// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DNONZERO %s 2>&1 | FileCheck %s --check-prefix=GATHER
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DWIDE %s 2>&1 | FileCheck %s --check-prefix=GATHER

using elements = unsigned int tile_size(32);
__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

#ifdef WIDE
using valid_type = unsigned long long;
#else
using valid_type = unsigned;
#endif

void rejected_gather(elements *output_pointer, const elements *index_pointer,
                     const unsigned *table, valid_type valid) {
  elements indices = *index_pointer;
  elements output{};
  auto &index_elements = element_view(indices);
  auto &output_elements = element_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element < valid)
      output_elements[element] = table[index_elements[element]];
    else
#ifdef NONZERO
      output_elements[element] = 9u;
#else
      output_elements[element] = 0u;
#endif
  }
  *output_pointer = output;
}

// GATHER: PTO element region: {{P1a requires one complete carrier publication|P1b requires one proved zero-inactive gather diamond}}
