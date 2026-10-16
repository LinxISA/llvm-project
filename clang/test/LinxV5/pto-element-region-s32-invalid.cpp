// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null %s 2>&1 | FileCheck %s --check-prefix=MIXED
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_GATHER %s 2>&1 | FileCheck %s --check-prefix=OUTPUT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_ATOMIC %s 2>&1 | FileCheck %s --check-prefix=OUTPUT
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_INDEX %s 2>&1 | FileCheck %s --check-prefix=INDEX
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -emit-llvm -S -o /dev/null -DTEST_KEY %s 2>&1 | FileCheck %s --check-prefix=KEY
using elements = unsigned tile_size(32);
__attribute__((annotate("pto.element.view:v1;dtype=s32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &signed_view(elements &v) { return v; }
__attribute__((annotate("pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"), always_inline))
inline elements &unsigned_view(elements &v) { return v; }
void rejected_profile(elements *sink, const elements *source,
                      unsigned *memory, unsigned valid, unsigned selected) {
  elements input=*source, output{};
#if defined(TEST_INDEX) || defined(TEST_KEY)
  auto &in=signed_view(input);
  auto &out=unsigned_view(output);
#else
  auto &in=unsigned_view(input);
  auto &out=signed_view(output);
#endif
#ifdef TEST_KEY
  elements index_copy=*source;
  auto &index_elements=unsigned_view(index_copy);
#endif
#pragma pto element for
  for(unsigned e=0;e<32;++e) {
#if defined(TEST_GATHER) || defined(TEST_INDEX)
    if(e<valid) out[e]=memory[in[e]];
    else out[e]=0;
#elif defined(TEST_ATOMIC)
    if(e<valid) out[e]=__atomic_fetch_add(&memory[in[e]],1u,__ATOMIC_RELAXED);
    else out[e]=0;
#elif defined(TEST_KEY)
    if(e<valid && in[e]==selected)
      out[e]=__atomic_fetch_add(&memory[index_elements[e]],1u,__ATOMIC_RELAXED);
    else out[e]=0;
#else
    out[e]=in[e]+1;
#endif
  }
  *sink=output;
}
// MIXED: PTO element region: unsupported scalar expression
// OUTPUT: gather and atomic regions currently require U32 output views
// INDEX: P1b requires one proved zero-inactive gather diamond
// KEY: unsupported atomic predicate
