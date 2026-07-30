// RUN: %clang_cc1 -triple linx64v5 -emit-llvm -o - %s | FileCheck %s

unsigned get_peid() { return __builtin_linx_get_thread_id(); }
unsigned get_peid_compat() { return __builtin_linx_get_thread_idx(); }

// CHECK-LABEL: define{{.*}} i32 @_Z8get_peidv
// CHECK: call i64 @llvm.linx.get.thread.id()
// CHECK-LABEL: define{{.*}} i32 @_Z15get_peid_compatv
// CHECK: call i64 @llvm.linx.get.thread.id()
