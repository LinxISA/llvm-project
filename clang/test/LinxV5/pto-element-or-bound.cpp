// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -S -emit-llvm %s -o - | FileCheck %s
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DPARTS=9 -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=TRAP
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DNONMONOTONIC -S -emit-llvm %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=TRAP

#ifndef PARTS
#define PARTS 8
#endif
using u16 = unsigned short;
using leaf = u16 tile_size(64);
__attribute__((always_inline, annotate("pto.element.view:v1;dtype=u16;rows=32;cols=2;layout=cube_m32")))
inline leaf &elements(leaf &x) { return x; }

void bounded_or(const u16 *source) {
  // Keep the outer induction visible to the range analysis under test.
#pragma clang loop unroll(disable)
  for (unsigned part = 0; part < PARTS; ++part) {
    leaf result;
    auto &dst = elements(result);
#pragma pto element for
    for (unsigned element = 0; element < 64; ++element) {
#ifdef NONMONOTONIC
      // Both operands are bounded by2, but1|2=3 exceeds2|2.
      unsigned index = (element % 3) | ((element / 3) % 3);
      if (index > 2)
        __builtin_trap();
#else
      unsigned index = (((element << 3) & 0x7ffffff0u) + (part << 1)) |
                       (element & 1u);
      if (index >= 512)
        __builtin_trap();
#endif
      dst[element] = source[index];
    }
    asm volatile("" : : "Tr"(result) : "memory");
  }
}
// CHECK-LABEL: define{{.*}}bounded_or
// CHECK-NOT: call void @llvm.trap
// CHECK: @llvm.linx.experimental.ew.mgather.gpr.masked
// TRAP: unsupported opcode or effect in generic IR stage: call llvm.trap
