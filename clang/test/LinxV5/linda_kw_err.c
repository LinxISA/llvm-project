// RUN: not %clang %s --target=linx64v4 -S -emit-llvm -O2 -o /dev/null 2>&1 | FileCheck %s
// RUN: not %clang %s --target=linx64v4 -S -emit-llvm -O2 -mlinda-global-var-as-thread-local -o /dev/null 2>&1 | FileCheck %s

__linda_thread __linda_shared int a;
__linda_shared __linda_thread int b;

__linda_thread __thread int c;
__linda_shared __thread int d;

__linda_thread _Thread_local int e;
__linda_shared _Thread_local int f;

__linda_thread __attribute__((tls_model("initial-exec"))) int g;
__linda_shared __attribute__((tls_model("initial-exec"))) int h;

void foo() {
  __linda_thread int temp_a;
  __linda_shared int temp_b;

  return;
}

// CHECK: error: '__linda_shared' is conflict with '__linda_thread'
// CHECK-NEXT: __linda_thread __linda_shared int a;
// CHECK-NEXT:                ^
// CHECK: error: cannot combine with previous '__linda_thread' declaration specifier
// CHECK: error: '__linda_thread' is conflict with '__linda_shared'
// CHECK-NEXT: __linda_shared __linda_thread int b;
// CHECK-NEXT:                ^
// CHECK: error: cannot combine with previous '__linda_shared' declaration specifier
// CHECK: error: '__linda_thread' is conflict with '__thread'
// CHECK-NEXT: __linda_thread __thread int c;
// CHECK-NEXT: ^
// CHECK: error: '__linda_shared' is conflict with '__thread'
// CHECK-NEXT: __linda_shared __thread int d;
// CHECK-NEXT: ^
// CHECK: error: '__linda_thread' is conflict with '_Thread_local'
// CHECK-NEXT: __linda_thread _Thread_local int e;
// CHECK-NEXT: ^
// CHECK: error: '__linda_shared' is conflict with '_Thread_local'
// CHECK-NEXT: __linda_shared _Thread_local int f;
// CHECK-NEXT: ^
// CHECK: error: 'tls_model' attribute only applies to thread-local variables
// CHECK-NEXT: __linda_thread __attribute__((tls_model("initial-exec"))) int g;
// CHECK-NEXT:                               ^
// CHECK: error: 'tls_model' attribute only applies to thread-local variables
// CHECK-NEXT: __linda_shared __attribute__((tls_model("initial-exec"))) int h;
// CHECK-NEXT:                               ^
// CHECK: error: '__linda_thread' variables must have global storage
// CHECK-NEXT:  __linda_thread int temp_a;
// CHECK-NEXT:  ^
// CHECK: error: '__linda_shared' variables must have global storage
// CHECK-NEXT:  __linda_shared int temp_b;

