// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -DCASE_B64_HIST %s -o - 2>&1 | FileCheck %s --check-prefix=ATOMIC32
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -c -DCASE_SIGNED_HIST %s -o %t
// RUN: llvm-objdump -d --no-show-raw-insn --disassembler-options=no-tile-macros %t | FileCheck %s --check-prefix=SIGNED
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -DCASE_VOLATILE_HIST %s -o - 2>&1 | FileCheck %s --check-prefix=ATOMIC32
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -DCASE_INDEX_SELECTED %s -o - 2>&1 | FileCheck %s --check-prefix=PREDICATE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -DCASE_SIDE_EFFECT_VALID %s -o - 2>&1 | FileCheck %s --check-prefix=CALL
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -DCASE_ELSE_INDEX %s -o - 2>&1 | FileCheck %s --check-prefix=INDUCTION

using tile32 = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline tile32 &element_view(tile32 &value) {
  return value;
}

#if defined(CASE_B64_HIST)
void rejected(tile32 &old_values, unsigned long long *hist,
              const tile32 &indices, unsigned int valid_lanes) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      output_elements[i] = __atomic_fetch_add(&hist[index_elements[i]], 1ull,
                                              __ATOMIC_RELAXED);
    else output_elements[i] = 0;
  }
  old_values = local_output;
}
#elif defined(CASE_SIGNED_HIST)
void rejected(tile32 &old_values, int *hist, const tile32 &indices,
              unsigned int valid_lanes) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      output_elements[i] = __atomic_fetch_add(&hist[index_elements[i]], 1,
                                              __ATOMIC_RELAXED);
    else output_elements[i] = 0;
  }
  old_values = local_output;
}
#elif defined(CASE_VOLATILE_HIST)
void rejected(tile32 &old_values, volatile unsigned int *hist,
              const tile32 &indices, unsigned int valid_lanes) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      output_elements[i] = __atomic_fetch_add(&hist[index_elements[i]], 1u,
                                              __ATOMIC_RELAXED);
    else output_elements[i] = 0;
  }
  old_values = local_output;
}
#elif defined(CASE_INDEX_SELECTED)
void rejected(tile32 &old_values, unsigned int *hist, const tile32 &low,
              const tile32 &high, unsigned int selected,
              unsigned int valid_lanes) {
  tile32 local_low = low;
  tile32 local_high = high;
  tile32 local_output{};
  auto &low_elements = element_view(local_low);
  auto &high_elements = element_view(local_high);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && high_elements[i] == selected + i)
      output_elements[i] = __atomic_fetch_add(&hist[low_elements[i]], 1u,
                                              __ATOMIC_RELAXED);
    else output_elements[i] = 0;
  }
  old_values = local_output;
}
#elif defined(CASE_SIDE_EFFECT_VALID)
unsigned int next_valid();
void rejected(tile32 &old_values, unsigned int *hist,
              const tile32 &indices) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < next_valid())
      output_elements[i] = __atomic_fetch_add(&hist[index_elements[i]], 1u,
                                              __ATOMIC_RELAXED);
    else output_elements[i] = 0;
  }
  old_values = local_output;
}
#elif defined(CASE_ELSE_INDEX)
void rejected(tile32 &old_values, unsigned int *hist,
              const tile32 &indices, unsigned int valid_lanes) {
  tile32 local_indices = indices;
  tile32 local_output{};
  auto &index_elements = element_view(local_indices);
  auto &output_elements = element_view(local_output);
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      output_elements[i] = __atomic_fetch_add(&hist[index_elements[i]], 1u,
                                              __ATOMIC_RELAXED);
    else output_elements[(i + 1u) & 31u] = 0;
  }
  old_values = local_output;
}
#endif

// ATOMIC32: error: PTO element region: requires non-volatile system-scope monotonic atomic add of exact i32 value one
// SIGNED-LABEL: <{{.*}}rejected
// SIGNED: BSTART.TLSU MGATHER.ADD, U32
// SIGNED: BSTART.TLSU TSTORE, U32
// PREDICATE: error: PTO element region: unsupported atomic predicate; expected element < valid, key[element] == selected, or an invariant boolean
// CALL: error: PTO element region: calls are not in the P1a arithmetic region
// INDUCTION: error: PTO element region: requires an induction equivalent to 0..31 step 1
