// RUN: not %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o - %s 2>&1 | FileCheck %s

using tile32 = unsigned int tile_size(32);

void unsupported_order(tile32 &old_values, unsigned int *hist,
                       const tile32 &indices, unsigned int valid_lanes) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid_lanes)
      old_values[i] =
          __atomic_fetch_add(&hist[indices[i]], 1u, __ATOMIC_SEQ_CST);
    else
      old_values[i] = 0;
  }
}

// CHECK: error: cannot compile this unsupported Linx element-wise loop form yet
