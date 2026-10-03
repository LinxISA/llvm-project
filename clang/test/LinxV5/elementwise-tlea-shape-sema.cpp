// RUN: not %clang++ --target=linx64v5 -mlxbc -fsyntax-only %s 2>&1 | FileCheck %s

using u32x32 = unsigned int tile_size(32);
using u64x32 = unsigned long long tile_size(32);

void bad_shape(u64x32 &out, const u32x32 &indices) {
  ew_tlea(32, 2, 25, 29, out, indices, 32);
}

// CHECK: error: Linx builtin argument types are inconsistent
