// RUN: not %clang --target=linx64v5 -mlxbc -std=c11 -O0 -emit-llvm -S -o /dev/null %s 2>&1 | FileCheck %s

#include <stdatomic.h>
typedef unsigned int tile_size(32) tile32;

void rejected(tile32 *out, _Atomic(unsigned int) *hist,
              const tile32 *indices, unsigned int valid) {
#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid)
      (*out)[i] = atomic_fetch_add_explicit(&hist[(*indices)[i]], 1u, memory_order_seq_cst);
    else
      (*out)[i] = 0;
  }
}

// CHECK: error: cannot compile this unsupported Linx element-wise loop form yet
