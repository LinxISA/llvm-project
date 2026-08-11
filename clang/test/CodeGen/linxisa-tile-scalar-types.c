// RUN: %clang_cc1 -triple linx64-linx-none-elf -emit-llvm -o - %s | FileCheck %s

_Float16 load_half(const _Float16 *source) { return *source; }
__bf16 load_bfloat(const __bf16 *source) { return *source; }

// CHECK-LABEL: define{{.*}} half @load_half
// CHECK: load half
// CHECK-LABEL: define{{.*}} bfloat @load_bfloat
// CHECK: load bfloat
