// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -dM -E %s | FileCheck %s --check-prefix=NOSIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu janus -dM -E %s | FileCheck %s --check-prefix=NOSIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu janus -target-feature +simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-cpu simt -target-feature -simt -dM -E %s | FileCheck %s --check-prefix=NOSIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-feature +simt -target-feature -simt -dM -E %s | FileCheck %s --check-prefix=NOSIMT
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -target-feature -simt -target-feature +simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// RUN: %clang_cc1 -triple linx64v5be-unknown-linux-musl -target-cpu simt -dM -E %s | FileCheck %s --check-prefix=SIMT
// NOSIMT-NOT: #define __linx_simt 1
// NOSIMT: #define __linx_simt_contract 1
// NOSIMT-NOT: #define __linx_simt 1
// SIMT: #define __linx_simt 1
// SIMT-NEXT: #define __linx_simt_contract 1
