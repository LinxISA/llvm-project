// RUN: not %clang --target=linx64v5 -mlxbc -O2 -mllvm -linxv5-enable-generic-element=true -DMIXED -emit-llvm -S %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=MIXED
// RUN: %clang --target=linx64v5 -mlxbc -O1 -Xclang -disable-llvm-passes -DDYNAMIC -emit-llvm -S %s -o %t.ll
// RUN: not opt -mtriple=linx64v5 -passes='mem2reg,loop-simplify,loop-rotate,instcombine,lcssa,linx-v5-element-predication' -S %t.ll -o /dev/null 2>&1 | FileCheck %s --check-prefix=DYNAMIC

#ifdef MIXED
void mixed(unsigned *__restrict output, unsigned short *__restrict narrow) {
#pragma pto element for grain(256)
  for (unsigned element = 0; element < 32; ++element) {
    output[element] = element;
    narrow[element] = element;
  }
}
// MIXED: explicit grain requires one output element width
#endif

#ifdef DYNAMIC
void dynamic(unsigned *__restrict output, unsigned limit) {
#pragma pto element for grain(256)
  for (unsigned element = 0; element < 32; ++element) {
    unsigned sum = 0;
    for (unsigned iteration = 0; iteration < limit; ++iteration) {
      if (iteration == element % 5)
        break;
      sum += element + iteration;
    }
    output[element] = sum;
  }
}
// DYNAMIC: explicit grain with dynamic nested control is not supported
#endif
