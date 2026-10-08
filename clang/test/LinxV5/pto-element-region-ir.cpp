// RUN: %clang++ --target=linx64v5 -mlxbc -O0 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | FileCheck %s

using elements = unsigned int tile_size(32);

__attribute__((annotate(
                   "pto.element.view:v1;dtype=u32;rows=32;cols=1;layout=cube_m32"),
               always_inline)) inline elements &element_view(elements &value) {
  return value;
}

void arithmetic(elements &output, elements &input, unsigned bias) {
  auto &input_elements = element_view(input);
  auto &output_elements = element_view(output);

#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    unsigned local = input_elements[element] + bias;
    local = local * 3u;
    output_elements[element] = ~local;
  }
}

// CHECK-LABEL: define{{.*}}arithmetic
// CHECK-COUNT-2: call ptr @llvm.ptr.annotation
// CHECK: call void @llvm.linx.experimental.element.region(metadata [[TOKEN:![0-9]+]])
// CHECK: br label %{{.*}}
// CHECK: for.body:
// CHECK: extractelement <32 x i32>
// CHECK: insertelement <32 x i32>
// CHECK: br label %{{.*}}, !llvm.loop [[LOOP:![0-9]+]]
// CHECK-NOT: @llvm.linx.experimental.ew.tbinary
// CHECK: [[TOKEN]] = distinct !{}
// CHECK: [[LOOP]] = distinct !{[[LOOP]],
// CHECK: !{{[0-9]+}} = !{!"llvm.loop.linx.pto.element.region", [[TOKEN]]}
