// RUN: %clang_cc1 -triple linx64-unknown-linux-gnu -O2 \
// RUN:   -vectorize-loops -vectorize-slp -S -o /dev/null %s

// Kernel bring-up regression: generic vectorization must not synthesize
// unsupported fixed-width bridge vectors for plain scalar bit-twiddling code.
int hex_to_bin(unsigned char ch) {
  unsigned char cu = ch & 0xdf;
  return -1 +
         ((ch - '0' + 1) &
          (unsigned)((ch - '9' - 1) & ('0' - 1 - ch)) >> 8) +
         ((cu - 'A' + 11) &
          (unsigned)((cu - 'F' - 1) & ('A' - 1 - cu)) >> 8);
}
