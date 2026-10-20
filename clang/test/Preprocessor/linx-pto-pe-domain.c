// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -dM -E %s | FileCheck %s
// CHECK: #define __LINX_PTO_PE_DOMAIN__ 1
