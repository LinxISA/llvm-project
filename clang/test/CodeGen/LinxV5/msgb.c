// RUN: %clang_cc1 -triple linx64v5 -emit-llvm -O2 -o - %s | FileCheck %s
// CHECK-LABEL: define{{.*}} @message
// CHECK: call void @llvm.linx.msgb.write(i64 2097, i64
// CHECK: call void @llvm.linx.msgb.send(i64
// CHECK: call i64 @llvm.linx.msgb.read(i64 2119)
unsigned long message(unsigned long control, unsigned long word) {
  __builtin_linx_msgb_write(0x831, word);
  __builtin_linx_msgb_send(control);
  return __builtin_linx_msgb_read(0x847);
}
