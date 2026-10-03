// RUN: not %clang --target=linx64v5 -mlxbc -std=c11 -O0 -emit-llvm -S -o /dev/null %s 2>&1 | FileCheck %s

typedef unsigned int tile_size(32) tile32;

void different_output(tile32 *out, tile32 *other, unsigned int *hist,
                      const tile32 *indices, unsigned int valid) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid)
      (*out)[i] = __atomic_fetch_add(&hist[(*indices)[i]], 1u,
                                    __ATOMIC_RELAXED);
    else
      (*other)[i] = 0;
  }
}

// CHECK: error: cannot compile this unsupported Linx element-wise loop form yet
