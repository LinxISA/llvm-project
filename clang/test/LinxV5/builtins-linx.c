// RUN: %clang --target=linx64v5 -O2 -emit-llvm -S -o - %s | FileCheck %s
// XFAIL: *
// The current JCore_Linxv5.patch implementation crashes in
// EmitLinxV5GetSysReg when lowering this builtin.

unsigned long long test_get_sys_reg() {
// CHECK: call i64 @llvm.linx.get.sysreg
  return __builtin_linx_get_system_reg(16);
}
