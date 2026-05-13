// RUN: %clang --target=linx64v4 -O2 -mlxbc -emit-llvm -S -o - %s | FileCheck %s

unsigned long long test_get_sys_reg() {
// CHECK: call i64 @llvm.linx.get.sysreg
  return __builtin_linx_get_system_reg(16);
}
