// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o - %s 2>&1 | FileCheck %s

using tile128 = float tile_size(128);

void unsupported_trip_count(tile128 &out, const tile128 &lhs,
                            const tile128 &rhs, unsigned count) {
#pragma pto element for
  for (unsigned element = 0; element < count; ++element)
    out[element] = lhs[element] + rhs[element];
}

// CHECK: error: PTO element region: requires a proven 32-element iteration domain
