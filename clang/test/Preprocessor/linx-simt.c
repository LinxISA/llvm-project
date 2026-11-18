// RUN: %clang_cc1 -triple linx64v5 -dM -E %s | FileCheck %s --check-prefix=JANUS
// RUN: %clang_cc1 -triple linx64v5 -target-cpu janus -dM -E %s | FileCheck %s --check-prefix=JANUS
// RUN: %clang_cc1 -triple linx64v5 -target-cpu simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5 -target-feature +simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5 -target-cpu simt -target-feature -simt -dM -E %s | FileCheck %s --check-prefix=JANUS
// JANUS-NOT: #define __linx_simt
// JANUS: #define __linx_xlen 64
// SIMT: #define __linx_simt 1
