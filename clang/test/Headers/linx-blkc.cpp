// RUN: %clang -target linx64-unknown-linux-musl -std=c++20 -fsyntax-only %s
// RUN: %clang -target linx32-unknown-linux-musl -std=c++20 -fsyntax-only %s

#ifndef PTO_LINX_COMPAT_TYPES_PROVIDED
#error "linx_blkc.h was not included by the Linx C++ driver"
#endif

static_assert(sizeof(__fp32) == 4);
static_assert(sizeof(__half) == 2);
static_assert(sizeof(__tf32) == 4);
static_assert(sizeof(__hf32) == 4);
static_assert(sizeof(__blkc_bf16) == 2);
static_assert(sizeof(__hif8) == 1);
static_assert(sizeof(__fp8_e4m3) == 1);
static_assert(sizeof(__fp8_e5m2) == 1);
static_assert(sizeof(__fp8_e6m2) == 1);
static_assert(sizeof(__fp6_e3m2) == 1);
static_assert(sizeof(__fp6_e2m3) == 1);
static_assert(sizeof(__fp4_e2m1x2) == 1);
static_assert(sizeof(__fp4_e1m2x2) == 1);
static_assert(sizeof(__fp8_e8m0) == 1);
static_assert(sizeof(__fp4_hif4x2) == 1);
static_assert(sizeof(__int4x2) == 1);
static_assert(sizeof(__uint4x2) == 1);

static_assert(!__is_same(__fp8_e4m3, __fp8_e5m2));
static_assert(!__is_same(__fp8_e8m0, __hif8));
static_assert(!__is_same(__int4x2, __uint4x2));

static_assert(sizeof(__fp16x2) == 4);
static_assert(sizeof(__bf16x2) == 4);
static_assert(sizeof(__uint16x2) == 4);
static_assert(sizeof(__int16x2) == 4);
static_assert(sizeof(__fp8_e4m3x4) == 4);
static_assert(sizeof(__fp8_e5m2x4) == 4);
static_assert(sizeof(__uint8x4) == 4);
static_assert(sizeof(__int8x4) == 4);
static_assert(sizeof(__fp8_e6m2x2) == 2);
static_assert(sizeof(__fp8_e4m3x2) == 2);
static_assert(sizeof(__fp8_e5m2x2) == 2);

using E8M0Tile = __fp8_e8m0 tile_size(4096);
static_assert(sizeof(E8M0Tile) == 4096);

void tile_constraint(E8M0Tile &Dst, const E8M0Tile &Src) {
  asm volatile("" : "=Tr"(Dst) : "Tr"(Src));
}

void vector_register_constraint(unsigned &Dst, unsigned Src) {
  asm volatile("" : "=vr"(Dst) : "vr"(Src));
}

void shared_constraint(unsigned long &Dst, unsigned long Src) {
  asm volatile("" : "=Sr"(Dst) : "Sr"(Src));
}

void storage_lvalues(__tf32 &TF32, __bf16x2 &BF16x2) {
  __tf32_STORAGE(TF32) = 0;
  __blkc_bf16x2_STORAGE(BF16x2) = 0;
}
