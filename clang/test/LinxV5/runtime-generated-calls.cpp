// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -mlinx-runtime-intra-abi -fcxx-exceptions -fexceptions -emit-llvm -o - %s | FileCheck %s
// RUN: %clang --target=linx64v5-unknown-linux-musl -mlinx-runtime-intra-abi -fexceptions -O2 -c %s -o %t.o
// RUN: %clang_cc1 -triple linx64v5-unknown-linux-musl -fcxx-exceptions -fexceptions -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s --check-prefix=DEFAULT

extern void work();
int catch_exception() {
  try { work(); } catch (...) { return 1; }
  return 0;
}
struct Object { Object(); ~Object(); };
Object &guarded_object() {
  static Object object;
  return object;
}
void throw_exception() { throw 42; }
void cleanup_resume() { Object object; work(); }

// CHECK-DAG: declare {{.*}}@__cxa_begin_catch({{.*}}) #[[CATCH:[0-9]+]]
// CHECK-DAG: declare {{.*}}@__cxa_guard_acquire({{.*}}) #[[GUARD:[0-9]+]]
// CHECK-DAG: declare {{.*}}@__cxa_allocate_exception({{.*}}) #[[CATCH]]
// CHECK-DAG: attributes #{{[0-9]+}} = { {{.*}}"linx-runtime-intra-abi"{{.*}} }
// CHECK-DAG: attributes #[[CATCH]] = { {{.*}}"linx-scalar-shareability"="intra"{{.*}} }
// CHECK-DAG: attributes #[[GUARD]] = { {{.*}}"linx-scalar-shareability"="intra"{{.*}} }
// DEFAULT-NOT: "linx-runtime-intra-abi"
// DEFAULT-NOT: "linx-scalar-shareability"="intra"
