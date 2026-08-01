// RUN: not %clang++ --target=linx64v5 -mlxbc -fsyntax-only %s 2>&1 | FileCheck %s

void kernel(int);

void caller() {
  kernel<<<1, 1, 1>>>(0);
  // CHECK: error: SIMT constructs are not supported by the superscalar compiler; check the source for unintended SIMT usage
}
