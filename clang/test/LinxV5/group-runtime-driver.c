// RUN: %clang --target=linx64v5-unknown-linux-musl -mlinx-group-runtime -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=linx64v5-unknown-linux-musl -### %s 2>&1 | FileCheck %s --check-prefix=NO-GROUP
// CHECK: "-u" "__linx_group_worker_start"
// NO-GROUP-NOT: __linx_group_worker_start
int main(void) { return 0; }
