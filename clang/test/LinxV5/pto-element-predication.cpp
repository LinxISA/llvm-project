// RUN: %clang++ --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -emit-llvm -S -o - %s | opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication,verify' -verify-each -S | FileCheck %s
// The pragma takes ordinary C++ through Clang CFG and standard LLVM passes.
// Target Tile legalization is intentionally not part of this P2 IR test.
extern "C" void conditional(unsigned *__restrict output,
                 const unsigned *__restrict input) {
#pragma pto element for
  for (unsigned element = 0; element < 33; ++element) {
    unsigned value = input[element];
    if (value & 1)
      output[element] = value + 3;
    else if (value == 0)
      output[element] = 7;
    else
      output[element] = 100 / value;
  }
}
// CHECK-LABEL: define{{.*}}conditional
// CHECK: call void @llvm.linx.experimental.element.region
// CHECK: call <33 x i32> @llvm.vp.gather
// CHECK: select <33 x i1>
// CHECK: call void @llvm.vp.scatter
// CHECK: call <33 x i32> @llvm.vp.udiv
// CHECK: call void @llvm.vp.scatter
// CHECK-NOT: phi i32
// CHECK: ret void
