// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_B64_HIST %s -o - 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_SIGNED_HIST %s -o - 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_VOLATILE_HIST %s -o - 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_INDEX_SELECTED %s -o - 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_SIDE_EFFECT_VALID %s -o - 2>&1 | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -DCASE_ELSE_INDEX %s -o - 2>&1 | FileCheck %s

using tile32 = unsigned int tile_size(32);

#if defined(CASE_B64_HIST)
void rejected(tile32 &old_values, unsigned long long *hist,
              const tile32 &indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1ull,
                                         __ATOMIC_RELAXED);
    else old_values[i] = 0;
  }
}
#elif defined(CASE_SIGNED_HIST)
void rejected(tile32 &old_values, int *hist, const tile32 &indices,
              unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1,
                                         __ATOMIC_RELAXED);
    else old_values[i] = 0;
  }
}
#elif defined(CASE_VOLATILE_HIST)
void rejected(tile32 &old_values, volatile unsigned int *hist,
              const tile32 &indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1u,
                                         __ATOMIC_RELAXED);
    else old_values[i] = 0;
  }
}
#elif defined(CASE_INDEX_SELECTED)
void rejected(tile32 &old_values, unsigned int *hist, const tile32 &low,
              const tile32 &high, unsigned int selected,
              unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes && high[i] == selected + i)
      old_values[i] = __atomic_fetch_add(&hist[low[i]], 1u,
                                         __ATOMIC_RELAXED);
    else old_values[i] = 0;
  }
}
#elif defined(CASE_SIDE_EFFECT_VALID)
unsigned int next_valid();
void rejected(tile32 &old_values, unsigned int *hist,
              const tile32 &indices) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < next_valid())
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1u,
                                         __ATOMIC_RELAXED);
    else old_values[i] = 0;
  }
}
#elif defined(CASE_ELSE_INDEX)
void rejected(tile32 &old_values, unsigned int *hist,
              const tile32 &indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] = __atomic_fetch_add(&hist[indices[i]], 1u,
                                         __ATOMIC_RELAXED);
    else old_values[(i + 1u) & 31u] = 0;
  }
}
#endif

// CHECK: error: cannot compile this unsupported Linx element-wise loop form yet
