using tile = float tile_size(128);

// Scalar-style source: the compiler recognizes the elementwise for-loop and
// lowers the complete body to tile operations.
void elementwise_if_for(tile &out, const tile &lhs, const tile &rhs) {
#pragma linx elementwise
  for (unsigned i = 0; i < 128; ++i) {
    if (lhs[i] > 0.0f)
      out[i] = lhs[i] + rhs[i];
    else
      out[i] = lhs[i] - rhs[i];
  }
}
