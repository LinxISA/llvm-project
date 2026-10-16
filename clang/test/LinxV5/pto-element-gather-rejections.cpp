// RUN: %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DNO_RESTRICT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DALIASED_VIEW %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DVOLATILE_SOURCE %s 2>&1 | FileCheck %s --check-prefix=VOLATILE
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DVOLATILE_SOURCE_POINTER %s 2>&1 | FileCheck %s --check-prefix=VOLATILE
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DCROSS_ELEMENT %s 2>&1 | FileCheck %s --check-prefix=CROSS-ELEMENT
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DNO_ZERO_ELSE %s 2>&1 | FileCheck %s --check-prefix=NO-ZERO
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DMIXED_MEMORY %s 2>&1 | FileCheck %s --check-prefix=MIXED
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DCALL_SOURCE %s 2>&1 | FileCheck %s --check-prefix=CALL
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DIF_INIT %s 2>&1 | FileCheck %s --check-prefix=CALL
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DCONDITION_VARIABLE %s 2>&1 | FileCheck %s --check-prefix=CALL
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DUNANNOTATED_VIEW %s 2>&1 | FileCheck %s --check-prefix=UNPROVED
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DWRONG_VIEW_MARKER %s 2>&1 | FileCheck %s --check-prefix=WRONG-VIEW
// RUN: not %clang++ --target=linx64v5 -mlxbc -std=c++17 -O2 -c -o /dev/null -DGLOBAL_VIEW %s 2>&1 | FileCheck %s --check-prefix=UNPROVED

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
  for (unsigned int e = 0; e < 32; ++e)
    out[e] = source[indices[e]];
}
#endif

#ifdef ALIASED_VIEW
// A C++ reference alias does not change the logical view identity.
void aliased_view(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source, unsigned int valid) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &original_indices = element_view(index_storage);
  auto &indices = original_indices;
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
  out_part = output;
}
#endif

#ifdef GLOBAL_VIEW
element_part global_out_part;
element_part global_index_part;
auto &global_out = element_view(global_out_part);
auto &global_indices = element_view(global_index_part);
void global_view(const unsigned int *__restrict source) {
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e)
    global_out[e] = source[global_indices[e]];
}
#endif

#ifdef WRONG_VIEW_MARKER
void wrong_view_marker(element_part &out_part, element_part &index_part,
                       const unsigned int *__restrict source) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = wrong_element_view(output);
  auto &indices = wrong_element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e)
    out[e] = source[indices[e]];
  out_part = output;
}
#endif

#ifdef NO_RESTRICT
// Stack-backed Tile views are distinct from the external gather source without
// requiring a source-spelling restrict qualifier.
void no_restrict(element_part &out_part, element_part &index_part,
                 const unsigned int *source, unsigned int valid) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
  out_part = output;
}
#endif

#ifdef IF_INIT
void if_init(element_part &out_part, element_part &index_part,
             const unsigned int *__restrict source, unsigned int valid) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (unsigned int side = bump(); e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
  out_part = output;
}
#endif

#ifdef CONDITION_VARIABLE
void condition_variable(element_part &out_part, element_part &index_part,
                        const unsigned int *__restrict source) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (unsigned int active = bump())
      out[e] = source[indices[e]];
    else
      out[e] = 0;
  }
  out_part = output;
}
#endif

#ifdef VOLATILE_SOURCE
void volatile_source(element_part &out_part, element_part &index_part,
                     const volatile unsigned int *__restrict source) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e)
    out[e] = source[indices[e]];
  out_part = output;
}
#endif

#ifdef VOLATILE_SOURCE_POINTER
void volatile_source_pointer(element_part &out_part, element_part &index_part,
                     const unsigned int *__restrict volatile source) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e)
    out[e] = source[indices[e]];
  out_part = output;
}
#endif

#ifdef CROSS_ELEMENT
void cross_element(element_part &out_part, element_part &index_part,
                   const unsigned int *__restrict source, unsigned int valid) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (e < valid)
      out[e] = source[indices[e + 1]];
    else
      out[e] = 0;
  }
  out_part = output;
}
#endif

#ifdef NO_ZERO_ELSE
void no_zero_else(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source, unsigned int valid) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    if (e < valid)
      out[e] = source[indices[e]];
    else
      out[e] = 1;
  }
  out_part = output;
}
#endif

#ifdef MIXED_MEMORY
void mixed_memory(element_part &out_part, element_part &index_part,
                  const unsigned int *__restrict source,
                  const unsigned int *__restrict other) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e) {
    out[e] = source[indices[e]];
    out[e] += other[indices[e]];
  }
  out_part = output;
}
#endif

#ifdef CALL_SOURCE
void call_source(element_part &out_part, element_part &index_part) {
  element_part output{};
  element_part index_storage = index_part;
  auto &out = element_view(output);
  auto &indices = element_view(index_storage);
#pragma pto element for
  for (unsigned int e = 0; e < 32; ++e)
    out[e] = get_source()[indices[e]];
  out_part = output;
}
#endif

// VOLATILE: error: PTO element region: volatile and atomic effects are not in P1a
// CROSS-ELEMENT: error: PTO element region: requires an induction equivalent to 0..31 step 1
// NO-ZERO: error: PTO element region: P1b requires one proved zero-inactive gather diamond
// MIXED: error: PTO element region: P1a requires one complete carrier publication
// CALL: error: PTO element region: calls are not in the P1a arithmetic region
// UNPROVED: error: PTO element region: unpromoted or unproved memory access in arithmetic region
// WRONG-VIEW: error: PTO element region: unsupported logical view contract 'pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=row_major'
