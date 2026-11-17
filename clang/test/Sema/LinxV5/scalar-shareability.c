// RUN: %clang_cc1 -triple linx64v5 -fsyntax-only -verify %s
void invalid(void *p, int scope) {
  __builtin_linx_scalar_load(p, 3, 1); // expected-error {{scalar access width must be 1, 2, 4, or 8 bytes}}
  __builtin_linx_scalar_load(p, 4, 3); // expected-error {{outside the valid range}}
  __builtin_linx_scalar_load(p, 4, scope); // expected-error {{must be a constant integer}}
  __builtin_linx_scalar_atomic(p, 0, 0, 8, 1, 1, 3); // expected-error {{LR cannot release and SC cannot acquire}}
  __builtin_linx_scalar_atomic(p, 0, 0, 8, 1, 2, 4); // expected-error {{LR cannot release and SC cannot acquire}}
  __builtin_linx_scalar_atomic(p, 0, 0, 1, 1, 0, 0); // expected-error {{scalar arithmetic atomics require 4 or 8 bytes}}
}
__attribute__((linx_scalar_shareability("bad"))) void bad(void); // expected-error {{shareability must be private, intra, or inter}}
