// RUN: not %clang --target=linx64v5 -mlxbc -mllvm -linxv5-enable-generic-element=true -std=c11 -O2 -emit-llvm -S -o /dev/null %s 2>&1 | FileCheck %s

typedef unsigned int tile_size(32) tile32;

unsigned int *next_hist(void);
void rejected(tile32 *out, const tile32 *indices, unsigned int valid) {

#pragma linx elementwise
  for (unsigned i = 0; i < 32; ++i) {
    if (i < valid)
      (*out)[i] = __atomic_fetch_add(&next_hist()[(*indices)[i]], 1u,
                                    __ATOMIC_RELAXED);
    else
      (*out)[i] = 0;
  }
}

// CHECK: error: PTO element region: predication: unsupported opcode or effect in generic IR stage: call next_hist
