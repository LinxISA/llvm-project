// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O2 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O3 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | FileCheck %s

void element_order(int *out, const int *in) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    int value = in[element];
    if (value != 0)
      out[element] = value + 1;
  }
}

// CHECK-LABEL: define{{.*}}element_order
// CHECK: call void @llvm.linx.experimental.element.region(metadata [[TOKEN:![0-9]+]])
// CHECK: br label %{{.*}}, !llvm.loop [[LOOP:![0-9]+]]
// CHECK: [[TOKEN]] = distinct !{}
// CHECK: [[LOOP]] = distinct !{[[LOOP]],
// CHECK-DAG: !{{[0-9]+}} = !{!"llvm.loop.linx.pto.element.region", [[TOKEN]]}
// CHECK-DAG: !{{[0-9]+}} = !{!"llvm.loop.linx.pto.element.inter_element_order", !"unordered"}
// CHECK-DAG: !{{[0-9]+}} = !{!"llvm.loop.linx.pto.element.intra_element_order", !"source"}
