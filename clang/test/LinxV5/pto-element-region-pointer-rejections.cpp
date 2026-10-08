// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_OFFSET %s 2>&1 | FileCheck %s --check-prefix=POINTER
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_DYNAMIC %s 2>&1 | FileCheck %s --check-prefix=POINTER
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_SELECT %s 2>&1 | FileCheck %s --check-prefix=POINTER
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_OUTSIDE %s 2>&1 | FileCheck %s --check-prefix=OUTSIDE

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

#if defined(TEST_OFFSET) || defined(TEST_DYNAMIC) || defined(TEST_SELECT)
void rejected_pointer_flow(elements *sink, const elements *source,
                           unsigned delta, bool choose) {
  elements input = *source;
  elements other{};
  elements output{};
  auto &input_elements = element_view(input);
  auto &output_elements = element_view(output);
  unsigned *base = reinterpret_cast<unsigned *>(&input_elements);
#if defined(TEST_OFFSET)
  unsigned *selected = base + 1;
#elif defined(TEST_DYNAMIC)
  unsigned *selected = base + delta;
#else
  unsigned *alternate = reinterpret_cast<unsigned *>(&other);
  unsigned *selected = choose ? base : alternate;
#endif
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output_elements[element] = selected[element];
  *sink = output;
}
#endif

#ifdef TEST_OUTSIDE
void rejected_shared_view(elements *sink, unsigned *scalar,
                          const elements *source) {
  elements input = *source;
  elements output{};
  auto &input_elements = element_view(input);
  auto &output_elements = element_view(output);
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    output_elements[element] = input_elements[element] + 1u;
  *scalar = input_elements[0];
  *sink = output;
}
#endif

// POINTER: PTO element region: logical view pointer must remain exact
// OUTSIDE: PTO element region: required region lowering did not complete
