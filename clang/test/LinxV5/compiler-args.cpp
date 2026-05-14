// RUN: %clang++ --target=linx64 -mlxbc -O2 -### %s 2>&1 \
// RUN:   | FileCheck %s --dump-input always -vv
// RUN: %clang++ --target=linx64-linux-musl -mlxbc -O2 -### %s 2>&1 \
// RUN:   | FileCheck %s --dump-input always -vv

// CHECK-DAG: "-fenable-matrix"
// CHECK-DAG: "-mlxbc"
// CHECK-DAG: "-include" "linx_blkc.h"
// CHECK-DAG: "-mllvm" "-enable-all-vector-as-tilereg"
void foo() {}
