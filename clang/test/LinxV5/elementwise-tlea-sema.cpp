// RUN: not %clang++ --target=linx64v5 -mlxbc -fsyntax-only %s 2>&1 | FileCheck %s

using s32tile = int tile_size(128);
using u64tile = unsigned long long tile_size(128);

void bad_dtype(u64tile &out, const s32tile &indices) {
  ew_tlea(32, 4, 25, 29, out, indices, 32);
}

// CHECK: error: Linx builtin argument types are inconsistent
