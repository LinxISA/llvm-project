// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DNO_RESTRICT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DVOLATILE_SOURCE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DVOLATILE_SOURCE_POINTER %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DCROSS_ELEMENT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DNO_ZERO_ELSE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DMIXED_MEMORY %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DCALL_SOURCE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -std=c++17 -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DIF_INIT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -std=c++17 -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DCONDITION_VARIABLE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DUNANNOTATED_VIEW %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DWRONG_VIEW_MARKER %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DALIASED_VIEW %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DGLOBAL_VIEW %s

using element_part = unsigned int tile_size(32);
unsigned int *get_source();
unsigned int bump();

__attribute__((annotate(
    "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32")))
element_part &element_view(element_part &part) {
  return part;
}

__attribute__((annotate(
    "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=row_major")))
element_part &wrong_element_view(element_part &part) {
  return part;
}

#ifdef UNANNOTATED_VIEW
void unannotated_view(element_part &out, element_part &indices,
                      const unsigned int *__restrict source) {
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef ALIASED_VIEW
void aliased_view(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source) {
  auto &out = element_view(out_part);
  auto &original_indices = element_view(index_part);
  auto &indices = original_indices;
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef GLOBAL_VIEW
element_part global_out_part;
element_part global_index_part;
auto &global_out = element_view(global_out_part);
auto &global_indices = element_view(global_index_part);
void global_view(const unsigned int *__restrict source) {
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    global_out[e] = source[global_indices[e]];
}
#endif

#ifdef WRONG_VIEW_MARKER
void wrong_view_marker(element_part &out_part, element_part &index_part,
                       const unsigned int *__restrict source) {
  auto &out = wrong_element_view(out_part);
  auto &indices = wrong_element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef NO_RESTRICT
void no_restrict(element_part &out_part, element_part &index_part,
                 const unsigned int *source) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef IF_INIT
void if_init(element_part &out_part, element_part &index_part,
             const unsigned int *__restrict source, unsigned int valid) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    if (unsigned int side = bump(); e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
}
#endif

#ifdef CONDITION_VARIABLE
void condition_variable(element_part &out_part, element_part &index_part,
                        const unsigned int *__restrict source) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    if (unsigned int active = bump())
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
}
#endif

#ifdef VOLATILE_SOURCE
void volatile_source(element_part &out_part, element_part &index_part,
                     const volatile unsigned int *__restrict source) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef VOLATILE_SOURCE_POINTER
void volatile_source_pointer(element_part &out_part, element_part &index_part,
                     const unsigned int *__restrict volatile source) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
}
#endif

#ifdef CROSS_ELEMENT
void cross_element(element_part &out_part, element_part &index_part,
                   const unsigned int *__restrict source) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e + 1]];
}
#endif

#ifdef NO_ZERO_ELSE
void no_zero_else(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source, unsigned int valid) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    if (e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 1;
  }
}
#endif

#ifdef MIXED_MEMORY
void mixed_memory(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source,
                  const unsigned int *__restrict other) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = source[indices[e]];
    out[e] += other[indices[e]];
  }
}
#endif

#ifdef CALL_SOURCE
void call_source(element_part &out_part, element_part &index_part) {
  auto &out = element_view(out_part);
  auto &indices = element_view(index_part);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = get_source()[indices[e]];
}
#endif
