// RUN: not %clang++ --target=linx64v5 -mlxbc -fsyntax-only %s 2>&1 | FileCheck %s

void kernel(int);

void caller() {
  kernel<<<1, 1, 1>>>(0);
  // CHECK: error: superscalar不支持simt，请检查代码是否有误使用
}
