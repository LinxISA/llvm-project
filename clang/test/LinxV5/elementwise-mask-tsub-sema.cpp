// RUN: %clang++ --target=linx64v5 -mlxbc -fsyntax-only -Xclang -verify %s

using tile = float tile_size(128);

void too_few(tile out, tile lhs, tile rhs) {
  ew_tsub_masked(16, 16, 1, 31, out, lhs, rhs, lhs, 0);
  // expected-error@-1 {{too few arguments to function call, expected 11, have 10}}
}

void bad_immediate(tile out, tile lhs, tile rhs, int rows) {
  ew_tsub_masked(rows, 16, 1, 31, out, lhs, rhs, lhs, 0, 1);
  // expected-error@-1 {{require a immediate number for register id}}
}

void bad_mask_type(tile out, tile lhs, tile rhs, float mask) {
  ew_tsub_masked(16, 16, 1, 31, out, lhs, rhs, lhs, 0, 1);
  // expected-error@-1 {{used type 'float' where integer is required}}
}

void bad_tile_type(tile out, tile lhs, float rhs) {
  ew_tsub_masked(16, 16, 1, 31, out, lhs, rhs, lhs, 0, 1);
  // expected-error@-1 {{Linx builtin argument types are inconsistent}}
}
