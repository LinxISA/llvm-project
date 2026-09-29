// RUN: not %clang --target=linx64v5 -mlxbc -c -o %t %s 2>&1 | FileCheck %s

using tile = float tile_size(128);

void unsupported_elementwise_if(tile &out, const tile &lhs,
                                const tile &rhs) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) {
    if (lhs[i] > 0.0f)
      out[i] = lhs[i] + rhs[i];
    else
      out[i] = 0.0f;
  }
}

// CHECK: error: cannot compile this unsupported Linx element-wise loop form yet
