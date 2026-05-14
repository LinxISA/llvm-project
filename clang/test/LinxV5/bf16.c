// RUN: %clang --target=linx64 -emit-llvm -S -o - %s | FileCheck %s

__bf16 test_bf16(__bf16 a, __bf16 b) {
// CHECK: fadd bfloat
    return a + b;
}