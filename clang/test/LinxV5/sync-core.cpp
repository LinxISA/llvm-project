// RUN: %clang_cc1 -triple linx64v5 -O2 -emit-llvm -o - %s | FileCheck %s

void sync_core() { __builtin_linx_sync_core(); }

// CHECK-LABEL: define{{.*}} void @_Z9sync_corev
// CHECK: call void @llvm.linx.sync.core()
// CHECK: ret void
// CHECK: declare void @llvm.linx.sync.core() #[[ATTR:[0-9]+]]
// CHECK: attributes #[[ATTR]] = { convergent noduplicate
// CHECK-NOT: nosync
// CHECK-NOT: willreturn
