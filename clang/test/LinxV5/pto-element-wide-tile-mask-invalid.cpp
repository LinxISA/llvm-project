// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -mllvm -linxv5-force-element-tile-mask=true -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=DIV

using u32 = unsigned int;
using u32x512 = u32 tile_size(512);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=16;layout=cube_m32"),
               always_inline))
inline u32x512 &u32_view512(u32x512 &value) {
  return value;
}

__attribute__((always_inline)) inline void consume(u32x512 &tile) {
  asm volatile("" : : "Tr"(tile) : "memory");
}

extern "C" void partial_division512(const u32 *__restrict numerator,
                                     const u32 *__restrict denominator,
                                     u32 valid) {
  u32x512 result;
  auto &out = u32_view512(result);
#pragma pto element for
  for (u32 element = 0; element < 512; ++element) {
    if (element < valid)
      out[element] = numerator[element] / denominator[element];
    else
      out[element] = 0;
  }
  consume(result);
}

// DIV: error: PTO element region: Tile legalization: PredicateCell arithmetic
// DIV-SAME: requires native masked add/sub, a non-trapping integer operation,
// DIV-SAME: a constant in-range shift, or a fully active domain
