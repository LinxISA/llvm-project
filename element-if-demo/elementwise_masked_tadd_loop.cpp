using tile = float tile_size(128);

void elementwise_masked_tadd_loop(float *lhs_ptr, float *rhs_ptr,
                                  float *out_ptr, unsigned long mask_low,
                                  unsigned long mask_high, int iterations) {
  for (int i = 0; i < iterations; ++i) {
    float *lhs_base = lhs_ptr + i * 16;
    float *rhs_base = rhs_ptr + i * 16;
    float *out_base = out_ptr + i * 16;

    tile lhs;
    tile rhs;
    tile result;

    blk_tload(16, 1, 1, 1, 3, 4, lhs, lhs_base, 16);
    blk_tload(16, 1, 1, 1, 3, 4, rhs, rhs_base, 16);

    ew_tadd_masked(16, 16, 1, 31, result, lhs, rhs,
                   mask_low, mask_high, 0, 1);

    blk_tstore(16, 1, 1, 1, 3, out_base, 16, result);
  }
}
