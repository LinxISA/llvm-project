// RUN: not %clang --target=linx64v5 -mlxbc -fsyntax-only %s 2>&1 | FileCheck %s

void malformed_pragmas(void) {
#pragma pto
  for (;;) {}

#pragma pto element
  for (;;) {}

#pragma pto for
  for (;;) {}

#pragma pto element while
  for (;;) {}

#pragma pto element for extra
  for (;;) {}
}

void non_for_statement(int *value) {
#pragma pto element for
  ++*value;
}

void while_loop(int *value) {
#pragma pto element for
  while (*value)
    --*value;
}

// CHECK: error: expected 'element' after '#pragma pto'
// CHECK: error: expected 'for' after '#pragma pto element'
// CHECK: error: expected 'element' after '#pragma pto'
// CHECK: error: expected 'for' after '#pragma pto element'
// CHECK: error: unexpected token after '#pragma pto element for'
// CHECK: error: '#pragma pto element for' and '#pragma linx elementwise' must precede a for loop
// CHECK: error: '#pragma pto element for' and '#pragma linx elementwise' must precede a for loop
