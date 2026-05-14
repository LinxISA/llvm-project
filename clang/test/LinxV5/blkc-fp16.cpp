// RUN: %clang++ --target=linx64 -O2 -mlxbc -S -emit-llvm -o - %s\
// RUN:   | FileCheck %s --dump-input always -vv

// CHECK-LABEL: foo
// CHECK: fadd half %a, %b
__half foo(__half a, __half b) { return a + b; }
