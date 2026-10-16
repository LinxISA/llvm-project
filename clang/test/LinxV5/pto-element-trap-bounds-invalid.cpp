// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_UNSAFE -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNSAFE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_STEP3 -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=STEP3
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_BYPASSED_EQUALITY -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BYPASSED-EQUALITY
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_MASKED_SAFE -emit-llvm -S %s -o /dev/null
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_MASKED_UNSAFE -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=MASKED-UNSAFE
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_TAIL_SAFE -emit-llvm -S %s -o /dev/null
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_TAIL_UNSAFE -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=TAIL-UNSAFE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_INTRINSIC_FAIL_CLOSED -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=INTRINSIC-FAIL-CLOSED
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_I64_PARTIAL64 -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=I64-PARTIAL64

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

#ifdef TEST_I64_PARTIAL64
extern "C" void partial_i64_address64(const unsigned *__restrict input,
                                       unsigned *__restrict output,
                                       unsigned long offset,
                                       unsigned long valid) {
#pragma pto element for
  for (unsigned long element = 0; element < 64; ++element)
    if (element < valid)
      output[element] = input[offset + (element << 2)];
}
// I64-PARTIAL64: i64 VP address arithmetic requires a constant affine result or a fully active, address-only uniform offset
#endif

#ifdef TEST_INTRINSIC_FAIL_CLOSED
extern "C" void unknown_intrinsic_bound(unsigned *output) {
  for (unsigned part = 0; part < 8; ++part) {
#pragma pto element for
    for (unsigned element = 0; element < 64; ++element) {
      unsigned index = element * 8 + __builtin_popcount(part);
      if (index >= 512)
        __builtin_trap();
      output[element] = index;
    }
  }
}
// INTRINSIC-FAIL-CLOSED: unsupported opcode or effect in generic IR stage: call llvm.trap
#endif

#ifdef TEST_BYPASSED_EQUALITY
extern "C" void bypassed_equality_bound(unsigned *output, bool check_limit) {
  for (unsigned part = 0;; ++part) {
    if (check_limit)
      if (part == 8)
        break;
#pragma pto element for
    for (unsigned element = 0; element < 64; ++element) {
      unsigned index = element * 8 + part;
      if (index >= 512)
        __builtin_trap();
      output[element] = index;
    }
  }
}
// BYPASSED-EQUALITY: unsupported opcode or effect in generic IR stage: call llvm.trap
#endif

#if defined(TEST_MASKED_SAFE) || defined(TEST_MASKED_UNSAFE)
extern "C" void masked_hierarchical_bound(unsigned *output) {
  for (unsigned group = 0; group < 16; ++group) {
    for (unsigned part = 0; part < 2; ++part) {
#pragma pto element for
      for (unsigned element = 0; element < 64; ++element) {
        unsigned lane_base = (element << 5) & 0x7fffffc0u;
        unsigned part_base = (group * 2 + part) << 1;
        unsigned index = lane_base + part_base;
#ifdef TEST_MASKED_UNSAFE
        if (index >= 2046)
#else
        if (index >= 2048)
#endif
          __builtin_trap();
        output[element] = index;
      }
    }
  }
}
// MASKED-UNSAFE: unsupported opcode or effect in generic IR stage: call llvm.trap
#endif

#if defined(TEST_TAIL_SAFE) || defined(TEST_TAIL_UNSAFE)
extern "C" void guarded_tail_bound(unsigned *output) {
  constexpr unsigned logical_count = 1298;
  constexpr unsigned columns = 64;
  constexpr unsigned full_rows = logical_count / columns;
  constexpr unsigned tail = logical_count % columns;
  for (unsigned group = 0; group < 16; ++group) {
    for (unsigned leaf = 0; leaf < 2; ++leaf) {
      unsigned part = 2 * group + leaf;
      unsigned part_column = 2 * part;
      unsigned tail_elements =
          tail <= part_column ? 0 : tail - part_column < 2
                                        ? tail - part_column
                                        : 2;
      unsigned part_count = 2 * full_rows + tail_elements;
#pragma pto element for
      for (unsigned element = 0; element < 64; ++element) {
#ifdef TEST_TAIL_UNSAFE
        if (element <= part_count) {
#else
        if (element < part_count) {
#endif
          unsigned index =
              (element / 2) * columns + 2 * part + element % 2;
          if (index >= logical_count)
            __builtin_trap();
          output[element] = index;
        }
      }
    }
  }
}
// TAIL-UNSAFE: unsupported opcode or effect in generic IR stage: call llvm.trap
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
