// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,verify' -verify-each -S | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t

extern "C" void nested_cpp(unsigned *__restrict output, unsigned limit,
                            unsigned stop, unsigned skip, unsigned divisor) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    unsigned sum = 0;
    for (unsigned iteration = 0; iteration < (limit & 3); ++iteration) {
      if (iteration == ((element + stop) & 3))
        break;
      if (iteration == skip)
        continue;
      for (unsigned inner = 0; inner < 2; ++inner)
        sum += (element + iteration + inner) / divisor;
    }
    output[element] = sum;
  }
}

// CHECK-LABEL: define{{.*}}nested_cpp
// CHECK: call <32 x i32> @llvm.vp.udiv
// CHECK: call void @llvm.vp.scatter
// CHECK-NOT: phi i32
// CHECK: ret void

extern "C" void lane_bound_cpp(unsigned *__restrict output, unsigned valid,
                                unsigned seed, unsigned divisor) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element >= valid)
      continue;
    unsigned sum = element;
    for (unsigned iteration = 0; iteration < ((element + seed) & 3);
         ++iteration)
      sum += (element + iteration) / divisor;
    output[element] = sum;
  }
}

// CHECK-LABEL: define{{.*}}lane_bound_cpp
// CHECK: call <32 x i32> @llvm.vp.udiv
// CHECK: call void @llvm.vp.scatter
// CHECK-NOT: phi i32
// CHECK: ret void

extern "C" void nested_effect_cpp(unsigned *__restrict output) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    for (unsigned iteration = 0; iteration < 3; ++iteration) {
      if (iteration == (element & 3))
        break;
      if (iteration == 1)
        continue;
      output[element] = output[element] * 3 + iteration;
    }
}

// CHECK-LABEL: define{{.*}}nested_effect_cpp
// CHECK: call <32 x i32> @llvm.vp.gather
// CHECK: call void @llvm.vp.scatter
// CHECK: call <32 x i32> @llvm.vp.gather
// CHECK: call void @llvm.vp.scatter
// CHECK: ret void
