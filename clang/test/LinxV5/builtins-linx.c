// RUN: %clang_cc1 -triple linx64v5 -O0 -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple linx64v5 -x c++ -O2 -emit-llvm -o - %s | FileCheck %s --check-prefixes=CHECK,CXX
// RUN: %clang_cc1 -triple linx64v5 -x c++ -DINVALID -fsyntax-only -verify %s

#if defined(__cplusplus)
extern "C"
#endif
unsigned long core_id(void) {
  // CHECK-LABEL: define{{.*}} i64 @core_id()
  // CHECK: call i64 @llvm.linx.get.sysreg(i64 33)
  return __builtin_linx_get_system_reg(0x21UL);
}

#if defined(__cplusplus)
template <unsigned long Address> unsigned long read_sysreg() {
  return __builtin_linx_get_system_reg(Address);
}

extern "C" unsigned long read_extended(void) {
  // CXX-LABEL: define{{.*}} i64 @read_extended()
  // CXX: call i64 @llvm.linx.get.sysreg(i64 7938)
  return read_sysreg<0x1f02UL>();
}
#endif

#ifdef INVALID
unsigned long invalid(unsigned long address) {
  __builtin_linx_get_system_reg(address); // expected-error {{immediate number for register id}}
  __builtin_linx_get_system_reg(0x1000000UL); // expected-error {{outside the valid range}}
  __builtin_linx_get_system_reg(-1UL); // expected-error {{outside the valid range}}
  return __builtin_linx_get_system_reg(); // expected-error {{too few arguments}}
}
#endif
