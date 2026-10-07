// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_ATOMIC_REFERENCE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_CALL %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_CROSS_ELEMENT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_MULTIPLE_STORES %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_SCALAR_EFFECT %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_VOLATILITY %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_UNIFORM_REFERENCE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_CONST_UNIFORM_REFERENCE %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -emit-llvm -S -o /dev/null -Xclang -verify -DTEST_GLOBAL_UNIFORM %s

using elements = unsigned int tile_size(32);
unsigned int effect();

#ifdef TEST_CALL
void call(elements &out) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = effect();
}
#endif
#ifdef TEST_CROSS_ELEMENT
void cross_element(elements &out, const elements &in) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = in[e ^ 1u];
}
#endif
#ifdef TEST_MULTIPLE_STORES
void multiple_stores(elements &out, elements &other, const elements &in) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = in[e] + 1u;
    other[e] = out[e];
  }
}
#endif
#ifdef TEST_SCALAR_EFFECT
void scalar_effect(elements &out, unsigned int &uniform) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    uniform = uniform + 1u;
    out[e] = uniform;
  }
}
#endif
#ifdef TEST_VOLATILITY
void volatility(elements &out, volatile elements &in) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = in[e];
}
#endif
#ifdef TEST_UNIFORM_REFERENCE
void aliasing_uniform(elements &out, unsigned int &uniform) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = uniform + 1u;
}
#endif
#ifdef TEST_CONST_UNIFORM_REFERENCE
void aliasing_uniform(elements &out, const unsigned int &uniform) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = uniform + 1u;
}
#endif
#ifdef TEST_GLOBAL_UNIFORM
unsigned int global_value;
void global_uniform(elements &out) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) // expected-error {{unsupported Linx element-wise loop form}}
    out[e] = global_value + 1u;
}
#endif

#ifdef TEST_ATOMIC_REFERENCE
void atomic_reference(elements &out, const elements &indices,
                      unsigned int *histogram, const unsigned int &valid) {
#pragma pto element for
  for (unsigned int e=0; e<32; ++e) { // expected-error {{unsupported Linx element-wise loop form}}
    if (e < valid)
      out[e] = __atomic_fetch_add(&histogram[indices[e]], 1u, __ATOMIC_RELAXED);
    else
      out[e] = 0u;
  }
}
#endif
