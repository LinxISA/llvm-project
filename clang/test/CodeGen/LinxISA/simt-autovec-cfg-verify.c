// RUN: %clang_cc1 -triple linx64-linx-none-elf -O2 -emit-obj \
// RUN:   -mllvm -linx-simt-autovec=1 \
// RUN:   -mllvm -linx-simt-autovec-mode=auto -o /dev/null %s

// The SIMT pass redirects the preheader around the original scalar loop.  The
// backend verifier must not observe stale incoming edges in the loop PHIs.
void simt_counted_loop(float *restrict dst, const float *restrict src) {
  for (unsigned long i = 0; i != 128; ++i)
    dst[i] = src[i] + 1.0f;
}
