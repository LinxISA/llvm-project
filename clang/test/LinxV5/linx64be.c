// RUN: %clang %s --target=linx64v5be -o test.o -### 2>&1 | FileCheck %s --check-prefixes=CHECK-LINX64BE
// RUN: %clang %s --target=linx64v5   -o test.o -### 2>&1 | FileCheck %s --check-prefixes=CHECK-LINX64LE

// CHECK-LINX64BE: "-EB"
// CHECK-LINX64LE-NOT: "-EB"
