// RUN: %clang -target linx64v5-unknown-linux-musl -O2 -S %s -o - | FileCheck %s

using u32 = unsigned int;
using u64 = unsigned long;

extern "C" u64 pack_u32(u32 value) {
  return (static_cast<u64>(value) << 32) | value;
}

// CHECK-LABEL: pack_u32:
// CHECK: slli a0, 32, ->t
// CHECK-NEXT: or t#1, a0.uw, ->a0

extern "C" u64 topk_pair_constant() {
  return 0x0000f1230000f123UL;
}

// CHECK-LABEL: topk_pair_constant:
// CHECK: hl.bfi t#1, t#1, 32, 63, ->a0
