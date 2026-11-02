// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,verify' -verify-each -S | FileCheck %s
// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -linxv5-enable-clock-hand-opt=false -filetype=obj -o %t
// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,linx-v5-element-tile-legalize,linx-v5-element-region-verify,verify' -verify-each -S | llc -mtriple=linx64v5 -mcpu=janus -enable-all-vector-as-tilereg=true -stop-after=linxv5-clockhands-postalloc -o %t.mir
// RUN: llc -mtriple=linx64v5 -mcpu=janus -run-pass=machineverifier %t.mir -o /dev/null

extern "C" void dynamic_cpp(unsigned *__restrict output, unsigned limit,
                             unsigned stop, unsigned skip, unsigned divisor,
                             unsigned valid) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    if (element >= valid)
      continue;
    unsigned sum = element;
    unsigned boundary = element % 7 + limit;
    for (unsigned iteration = 0; iteration < boundary; ++iteration) {
      if (iteration == element % 5 + stop)
        break;
      if (iteration == skip)
        continue;
      sum += (element + iteration) / divisor;
    }
    output[element] = sum;
  }
}

// CHECK-LABEL: define{{.*}}dynamic_cpp
// CHECK: pto.loop.scheduler:
// CHECK: phi <32 x
// CHECK: call i1 @llvm.vector.reduce.or.v32i1
// CHECK: br i1 {{.*}}, label %pto.loop.scheduler,
// CHECK: ret void

extern "C" void dynamic_effect_cpp(unsigned *__restrict output,
                                    unsigned limit, unsigned skip) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element)
    for (unsigned iteration = 0; iteration < limit; ++iteration) {
      if (iteration == element % 5)
        break;
      if (iteration == skip)
        continue;
      output[element] = output[element] * 3 + iteration;
    }
}

// CHECK-LABEL: define{{.*}}dynamic_effect_cpp
// CHECK: pto.loop.scheduler:
// CHECK: call <32 x i32> @llvm.vp.gather
// CHECK: call void @llvm.vp.scatter
// CHECK: call i1 @llvm.vector.reduce.or.v32i1
// CHECK: br i1 {{.*}}, label %pto.loop.scheduler,
// CHECK: ret void

extern "C" void dynamic_nested_cpp(unsigned *__restrict output, unsigned limit,
                                    unsigned inner_limit, unsigned skip) {
#pragma pto element for
  for (unsigned element = 0; element < 32; ++element) {
    unsigned sum = 0;
    for (unsigned outer = 0; outer < limit; ++outer) {
      if (outer == skip)
        continue;
      for (unsigned inner = 0; inner < inner_limit; ++inner) {
        if (inner == element % 5)
          break;
        sum += element + outer + inner;
      }
    }
    output[element] = sum;
  }
}

// CHECK-LABEL: define{{.*}}dynamic_nested_cpp
// CHECK: pto.loop.scheduler:
// CHECK: phi <32 x
// CHECK: call i1 @llvm.vector.reduce.or.v32i1
// CHECK: br i1 {{.*}}, label %pto.loop.scheduler,
// CHECK: ret void
