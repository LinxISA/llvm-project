// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -emit-llvm -S %s -o %t.ll
// RUN: FileCheck %s --check-prefix=IR < %t.ll
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_WIDE -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=WIDE
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_SHIFT32 -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=SHIFT32
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_UNBOUNDED_JOIN -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNBOUNDED
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_UNSUPPORTED_USER -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BADUSER
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_NEGATIVE_JOIN -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=NEGATIVE
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_HIGH_GEP -emit-llvm -S %s -o - | FileCheck %s --check-prefix=HIGHGEP
// RUN: not %clang++ --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DTEST_BOUNDARY -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=BOUNDARY

using u32 = unsigned;
using u64 = unsigned long;

#if !defined(TEST_WIDE) && !defined(TEST_SHIFT32) && \
    !defined(TEST_UNBOUNDED_JOIN) && !defined(TEST_UNSUPPORTED_USER) && \
    !defined(TEST_NEGATIVE_JOIN) && !defined(TEST_HIGH_GEP) && \
    !defined(TEST_BOUNDARY)
extern "C" void guarded_output(const u32 *__restrict input,
                                u32 *__restrict output) {
  for (u64 row_group = 0; row_group < 4; ++row_group) {
    u64 valid = row_group ? 74 : 128;
#pragma pto element for
    for (u64 element = 0; element < 32; ++element) {
      u64 row = (element << 2) + row_group;
      if (row < valid)
        output[element * 388] = input[element];
    }
  }
}

// IR-LABEL: define{{.*}} @guarded_output(
// IR: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i32
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64
// IR-NOT: @llvm.vp.{{.*}}v32i64

extern "C" void bounded_join(const u32 *__restrict input,
                              u32 *__restrict output, bool choose) {
  u64 joined = choose ? 1 : 2;
#pragma pto element for
  for (u64 element = 0; element < 32; ++element) {
    u64 index = element + joined;
    if (index < 32)
      output[element] = input[index];
  }
}

// IR-LABEL: define{{.*}} @bounded_join(
// IR: @llvm.linx.experimental.ew.tlea.v32i64.v32i32
// IR-NOT: @llvm.linx.experimental.ew.tbinary.gpr.masked.v32i64
// IR-NOT: @llvm.vp.{{.*}}v32i64
#endif

#ifdef TEST_WIDE
extern "C" void wide_counter(const u32 *__restrict input,
                              u32 *__restrict output, u64 wide) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element)
    if (element + wide < 32)
      output[element] = input[element];
}
// WIDE: i64 VP address arithmetic requires
#endif

#ifdef TEST_SHIFT32
extern "C" void shift32(const u32 *__restrict input,
                         u32 *__restrict output) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element)
    output[element] = input[element << 32];
}
// SHIFT32: constant Tile requires a scalar splat or affine sequence
#endif

#ifdef TEST_UNBOUNDED_JOIN
extern "C" void unbounded_join(const u32 *__restrict input,
                                u32 *__restrict output, u64 wide, bool choose) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element) {
    u64 joined = choose ? element : wide;
    if (joined < 32)
      output[element] = input[joined];
  }
}
// UNBOUNDED: select requires an i1 mask and admitted Tile values
#endif

#ifdef TEST_UNSUPPORTED_USER
extern "C" void unsupported_user(const u32 *__restrict input,
                                  u32 *__restrict output) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element) {
    u64 shared = element + 1;
    u64 shifted = shared >> 1;
    output[element] = input[shared] + static_cast<u32>(shifted);
  }
}
// BADUSER: only i32 to i64 index extension is supported
#endif

#ifdef TEST_NEGATIVE_JOIN
extern "C" void negative_join(const u32 *__restrict input,
                               u32 *__restrict output, bool choose) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element) {
    long joined = choose ? static_cast<long>(element) : -1;
    if (joined >= 0)
      output[element] = input[joined];
  }
}
// NEGATIVE: select requires an i1 mask and admitted Tile values
#endif

#ifdef TEST_HIGH_GEP
extern "C" void high_gep(const u32 *__restrict input,
                          u32 *__restrict output) {
#pragma pto element for
  for (u64 element = 0; element < 32; ++element)
    output[element] = input[0x80000000UL + element];
}
// HIGHGEP-LABEL: define{{.*}} @high_gep(
// HIGHGEP: @llvm.linx.experimental.ew.tlea.v32i64.v32i32(i64 32, i64 1, i64 25,
#endif

#ifdef TEST_BOUNDARY
extern "C" void boundary(const u32 *__restrict input,
                          u32 *__restrict output) {
  for (u64 base = 0x7ffffffeUL; base < 0x80000001UL; ++base) {
#pragma pto element for
    for (u64 element = 0; element < 32; ++element) {
      u64 index = base + element;
      if (index < 0x80000001UL)
        output[element] = input[index];
    }
  }
}
// BOUNDARY: i64 VP address arithmetic requires
#endif
