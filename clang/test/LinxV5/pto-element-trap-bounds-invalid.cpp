// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_UNSAFE -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNSAFE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_STEP3 -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=STEP3

#ifdef TEST_UNSAFE
extern "C" void unsafe_lane_bound(unsigned *output) {
#pragma pto element for
  for (unsigned element = 0; element < 64; ++element) {
    unsigned index = element * 9;
    if (index >= 512)
      __builtin_trap();
    output[element] = index;
  }
}
// UNSAFE: unsupported opcode or effect in generic IR stage: call llvm.trap
#endif

#ifdef TEST_STEP3
extern "C" void nondividing_outer_recurrence(unsigned *output) {
  for (unsigned part = 0; part != 8; part += 3) {
#pragma pto element for
    for (unsigned element = 0; element < 64; ++element) {
      unsigned index = element * 8 + part;
      if (index >= 512)
        __builtin_trap();
      output[element] = index;
    }
  }
}
// STEP3: unsupported opcode or effect in generic IR stage: call llvm.trap
#endif
